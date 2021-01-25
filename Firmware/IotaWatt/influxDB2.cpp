#include "IotaWatt.h"
#include "xbuf.h"
#include "xurl.h"

bool      influx2Started = false;                    // True when Service started
bool      influx2Stop = false;                       // Stop the influx2 service
bool      influx2Restart = false;                    // Restart the influx2 service
bool      influx2LogHeap = false;                    // Post a heap size measurement (diag)
bool      influx2StaticKeyset = true;                // True if keyset has no variables (except $device)
uint32_t  influx2LastPost = 0;                       // Last acknowledge post for status report

    // Configuration settings

uint16_t  influx2BulkSend = 1;                       
uint16_t  influx2Port = 8086;
int32_t   influx2Revision = -1;                      // Revision control for dynamic config
uint32_t  influx2BeginPosting = 0;                   // Begin date specified in config
char*     influx2orgID = nullptr;
char*     influx2Token = nullptr; 
char*     influx2Retention = nullptr;
char*     influx2Measurement = nullptr;
char*     influx2FieldKey = nullptr; 
xurl*     influx2URL = nullptr;
char*      influx2Org = nullptr;
char*     influx2Bucket = nullptr;
influxTag* influx2TagSet = nullptr;  
ScriptSet* influx2Outputs;

uint32_t influx2Service(struct serviceBlock* _serviceBlock){

      // This is a standard IoTaWatt Service operating as a state machine.

  enum   states {initialize,        // Basic startup of the service - one time
                 queryLastPostTime, // Setup to query for last post time of each measurement
                 queryLast,         // Query last() for this measurement
                 queryLastWait,     // wait for [async] query to complete
                 getLastRecord,     // Read the logRec and prep the context for logging
                 post,              // Add a measurement to the reqData xbuf
                 sendPost,          // Send the accumulated measurements
                 waitPost};         // Wait for the [async] post to complete

  static states state = initialize;
  static IotaLogRecord* logRecord = nullptr;
  static IotaLogRecord* oldRecord = nullptr;
  static uint32_t lastRequestTime = 0;          // Time of last measurement in last or current request
  static uint32_t lastBufferTime = 0;           // Time of last measurement reqData buffer
  static uint32_t UnixNextPost = UTCtime();     // Next measurement to be posted
  static xbuf reqData;                          // Current request buffer
  static uint32_t reqUnixtime = 0;              // First measurement in current reqData buffer
  static int  reqEntries = 0;                   // Number of measurement intervals in current reqData
  static int16_t retryCount = 0;                // HTTP error count
  static asyncHTTPrequest* request = nullptr;   // -> instance of asyncHTTPrequest
  static uint32_t postFirstTime = UTCtime();    // First measurement in outstanding post request
  static uint32_t postLastTime = UTCtime();     // Last measurement in outstanding post request
  static size_t reqDataLimit = 3000;            // transaction yellow light size
  static uint32_t HTTPtoken = 0;                // HTTP resource reservation token
  static Script* script = nullptr;              // current Script
  static char influx2Proxy[] = "http://192.168.1.152:9000";

  trace(T_influx2,0);                            // Announce entry
      
          // Handle current state

  switch(state){

//********************************************************* Initialize **************************************
    case initialize: {
      trace(T_influx2,2);
      if(influx2Stop){
        influx2Started = false;
        return 0;
      }

          // We post from the log, so wait if not available.          

      if(!Current_log.isOpen()){                  
        return UTCtime() + 5;
      }
      log("influxDB2: started, url=%s, bucket=%s, org=%s, interval=%d", influx2URL->build().c_str(),
              influx2Bucket, influx2orgID, influxDB2Interval);
      state = queryLastPostTime;
      trace(T_influx2,2);
      return 1;
    }
 
 //********************************************************* queryLastPostTime *****************************
    case queryLastPostTime:{
      trace(T_influx2,3);
      influx2LastPost = influx2BeginPosting;
      script = influx2Outputs->first();
      retryCount = 0;
      trace(T_influx2,4);
      state = queryLast;
      return 1;
    }

  //********************************************************* queryLast *****************************    

    case queryLast:{

          // Make sure wifi is connected and there is a resource available.

      if( ! WiFi.isConnected()) return UTCtime() + 1;

      influx2LastPost = UTCtime();
      influx2LastPost -= influx2LastPost % influxDB2Interval;
      state = getLastRecord;
      return 1;

      HTTPtoken = HTTPreserve(T_influx2);
      if( ! HTTPtoken){
        return UTCtime() + 1;
      }

          // Create a new request

      if( ! request) request = new asyncHTTPrequest;
      request->setTimeout(5);
      request->setDebug(false);
      {
        char URL[100];
        sprintf_P(URL, PSTR("%s/query"),influx2URL->build().c_str());
        if( ! request->open("POST", URL)){
          HTTPrelease(HTTPtoken);
          return UTCtime() + 2;
        }
      }
      
    //   if(influx2User && influx2Pwd){
    //     xbuf xb;
    //     xb.printf("%s:%s", influx2User, influx2Pwd);
    //     base64encode(&xb);
    //     String auth = "Basic ";
    //     auth += xb.readString(xb.available());
    //     request->setReqHeader("Authorization", auth.c_str()); 
    //   }
      trace(T_influx2,4);
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      reqData.flush();
      reqData.printf_P(PSTR("db=%s&epoch=s"), influx2Bucket); 
      if(influx2Retention){
        reqData.printf_P(PSTR("&rp=%s"), influx2Retention);
      }
      reqData.printf_P(PSTR("&q= SELECT LAST(% s) FROM % s "),influx2VarStr(influx2FieldKey, script).c_str(),influx2VarStr(influx2Measurement, script).c_str());
      influxTag* tag = influx2TagSet;
      trace(T_influx2,41);
      while(tag){
        reqData.printf_P(PSTR(" %s %s=\'%s\'"), tag == influx2TagSet ? "WHERE" : "AND", tag->key, influx2VarStr(tag->value, script).c_str());
        tag = tag->next;
      }
      
          // Send the request

      if( ! request->send(&reqData, reqData.available())){
        HTTPrelease(HTTPtoken);
        request->abort();
        return UTCtime() + 2;
      }
      trace(T_influx2,42);
      state = queryLastWait;
      return 1;
    }

    case queryLastWait: {

          // If not completed, return to wait.

      trace(T_influx2,5); 
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrelease(HTTPtoken);
      if(influx2Stop || influx2Restart){
        state = post;
        return 1;
      }
      String response = request->responseText(); 
      int HTTPcode = request->responseHTTPcode();
      
      if(HTTPcode < 0){
        if(retryCount++ == 10){
          log("influxDB2: last entry query failed: %d, retrying.", HTTPcode);
        }
        state = queryLast;
        return UTCtime() + (retryCount <= 10 ? 2 : 10);
      }
      delete request;
      request = nullptr;
      retryCount = 0;
      trace(T_influx2,5);

            // Check for authentication error

      if(HTTPcode == 401){
        log("influxDB2: Authentication failed. Stopping influx2 service.");
        influx2Stop = true;
        state = post;
        return 1;
      }

            // Check for invalid request.

      if(HTTPcode != 200){
        log("influxDB2: Last entry query failed.");
        log("influxDB2: HTTPcode %d, %.60s", HTTPcode, response.c_str());
        influx2Stop = true;
        state = post;
        return 1;
      }

            // Json parse the response to get the columns and values arrays
            // and extract time

      {
        DynamicJsonBuffer Json;
        JsonObject& results = Json.parseObject(response);
        if(results.success()){ 
          const char* error = results.get<const char*>("error");
          if(error){
            log("influxDB2: last entry query failed %d %s", HTTPcode, error);
            influx2Stop = true;
            state = post;
            return 1;
          }
          JsonArray& columns = results["results"][0]["series"][0]["columns"];
          JsonArray& values = results["results"][0]["series"][0]["values"][0];
          if(columns.success() && values.success()){
            for(int i=0; i<columns.size(); i++){
              if(strcmp("time",columns[i].as<char*>()) == 0){
                if(values[i].as<unsigned long>() > influx2LastPost){
                  influx2LastPost = values[i].as<unsigned long>();
                }
                break;
              }
            }
          } else {
            const char* error = results["results"][0]["error"].as<const char*>();
            if(error){
              log("influxDB2: last entry query failed %d %s", HTTPcode, error);
              influx2Stop = true;
              state = post;
              return 1;
            }
          }
        }
      }
      
      script = script->next();
      if(script){
        state = queryLast;
        return 1;
      }

      if(influx2LastPost == 0){
        influx2LastPost = UTCtime();
      }
      influx2LastPost -= influx2LastPost % influxDB2Interval;
      log("influxDB2: Start posting at %s", localDateString(influx2LastPost + influxDB2Interval).c_str());
      delete request;
      request = nullptr;
      state = getLastRecord;
      return 1;
    }

    case getLastRecord: {
      trace(T_influx2,6);   
      if( ! oldRecord){
        oldRecord = new IotaLogRecord;
      }
      oldRecord->UNIXtime = influx2LastPost;      
      Current_log.readKey(oldRecord);
      trace(T_influx2,6);

          // Assume that record was posted (not important).
          // Plan to start posting one interval later
      
      UnixNextPost = oldRecord->UNIXtime + influxDB2Interval - (oldRecord->UNIXtime % influxDB2Interval);
      
          // Advance state.

      reqData.flush();
      reqEntries = 0;
      state = post;
      return UnixNextPost;
    }

    
    case post: {
      trace(T_influx2,7);

          // If stop requested, do it now.

      if(influx2Stop || influx2Restart) {
        if(request && request->readyState() < 4) return 1;
        trace(T_influx2,71);
        state = initialize;
        if(influx2Restart){
          log("influxDB2: Restart. Last post %s", localDateString(influx2LastPost).c_str());
          influx2Restart = false;
          return 1;
        } else {
          log("influxDB2: Stopped. Last post %s", localDateString(influx2LastPost).c_str());
          trace(T_influx2,72);    
          delete oldRecord;
          oldRecord = nullptr;
          delete logRecord;
          logRecord = nullptr;
          delete request;
          request = nullptr;
          reqData.flush();
          delete[] influx2orgID;
          influx2orgID = nullptr;
          delete[] influx2Token;
          influx2Token = nullptr; 
          delete[] influx2Retention;
          influx2Retention = nullptr;
          delete[] influx2Measurement;
          influx2Measurement = nullptr;
          delete[] influx2FieldKey;
          influx2FieldKey = nullptr; 
          delete influx2URL;
          influx2URL = nullptr;
          delete[] influx2Bucket;
          influx2Bucket = nullptr;
          delete influx2TagSet;
          influx2TagSet = nullptr;  
          delete influx2TagSet;
          influx2Outputs;      
          influx2Started = false;
          return 0;
        }
        
      }
      
          // If not enough entries for bulk-send, come back in one second;

      if(((Current_log.lastKey() - influx2LastPost) / influxDB2Interval + reqEntries) < influx2BulkSend){
        return UTCtime() + 1;
      }

          // If buffer isn't full,
          // add another measurement.

      if(reqData.available() < reqDataLimit && UnixNextPost <= Current_log.lastKey()){  

            // Read the next log record.

        if( ! logRecord){
          logRecord = new IotaLogRecord;            
        }
        trace(T_influx2,7);
        logRecord->UNIXtime = UnixNextPost;
        Current_log.readKey(logRecord);
        trace(T_influx2,7);
        
            // Compute the time difference between log entries.
            // If zero, don't bother.
            
        double elapsedHours = logRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
          if(Current_log.readNext(logRecord) == 0) {
            UnixNextPost = logRecord->UNIXtime - (logRecord->UNIXtime % influxDB2Interval);
          }
          UnixNextPost += influxDB2Interval;
          return UnixNextPost;  
        }
            
            // Build the request string.
            // values for each channel are (delta value hrs)/(delta log hours) = period value.
            // Update the previous (Then) buckets to the most recent values.
      
        script = influx2Outputs->first();
        trace(T_influx2,7);
        String lastMeasurement;
        String thisMeasurement;
        while(script){
          double value = script->run(oldRecord, logRecord, elapsedHours);
          if(value == value){
            thisMeasurement = influx2VarStr(influx2Measurement, script);
            if(influx2StaticKeyset && thisMeasurement.equals(lastMeasurement)){
              reqData.printf_P(PSTR(",%s=%.*f"), influx2VarStr(influx2FieldKey, script).c_str(), script->precision(), value);
            } else {
              if(lastMeasurement.length()){
                reqData.printf(" %d\n", UnixNextPost);
              }
              reqData.write(thisMeasurement);
              if(influx2TagSet){
                trace(T_influx2,71);
                influxTag* tag = influx2TagSet;
                while(tag){
                  reqData.printf_P(PSTR(",%s=%s"), tag->key, influx2VarStr(tag->value, script).c_str());
                  tag = tag->next;
                }
              }
              reqData.printf_P(PSTR(" %s=%.*f"), influx2VarStr(influx2FieldKey, script).c_str(), script->precision(), value);
            }
          }
          lastMeasurement = thisMeasurement;
          script = script->next();
        }
        reqData.printf(" %d\n", UnixNextPost);

        trace(T_influx2,7);
        delete oldRecord;
        oldRecord = logRecord;
        logRecord = nullptr;
        
        trace(T_influx2,7);  
        reqEntries++;
        lastBufferTime = UnixNextPost;
        UnixNextPost +=  influxDB2Interval - (UnixNextPost % influxDB2Interval);
        return 1;
      }

            // If there's no request pending and we have bulksend entries,
            // set to post.

      if((( ! request || request->readyState() == 4) && HTTPrequestFree) && 
          (reqEntries >= influx2BulkSend || reqData.available() >= reqDataLimit)){
        state = sendPost;
        if(influx2LogHeap && heapMsPeriod != 0){
          reqData.printf_P(PSTR("heap"));
          influxTag* tag = influx2TagSet;
          if(tag){
            Script* script = influx2Outputs->first();
            reqData.printf_P(PSTR(",%s=%s"), tag->key, influx2VarStr(tag->value, script).c_str());
          }
          reqData.printf_P(PSTR(" value=%d %d\n"), (uint32_t)heapMs / heapMsPeriod, UTCtime());
          heapMs = 0.0;
          heapMsPeriod = 0;
        }       
      }
      return (UnixNextPost > UTCtime()) ? UTCtime() + 1 : 1;
    }

    case sendPost: {
      trace(T_influx2,8);
      if( ! WiFi.isConnected()){
        return UTCtime() + 1;
      }
      HTTPtoken = HTTPreserve(T_influx2);
      if( ! HTTPtoken){
        return 1;
      }
      if( ! request){
        request = new asyncHTTPrequest;
      }
      request->setTimeout(3);
      request->setDebug(true);
      if(request->debug()){
        Serial.println(ESP.getFreeHeap()); 
        Serial.println(datef(localTime(),"hh:mm:ss"));
        Serial.println(reqData.peekString(reqData.available()));
      }
      trace(T_influx2,8);
      {
        char URL[128];
        size_t len = sprintf_P(URL, PSTR("%s/api/v2/write?precision=s&orgID=%s&bucket=%s"), &influx2Proxy, influx2orgID, influx2Bucket);
        // if(influx2Retention){
        //   sprintf(URL+len,"&rp=%s", influx2Retention);
        // }
        if( ! request->open("POST", URL)){
          HTTPrelease(HTTPtoken);
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return 1;
        }
      }
      trace(T_influx2,8);
      request->setReqHeader("X-proxypass", influx2URL->build().c_str());
      //request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_influx2,8);
      String auth = "Token ";
      auth += influx2Token;
      request->setReqHeader("Authorization", auth.c_str());
      request->send(&reqData, reqData.available());
      reqEntries = 0;
      lastRequestTime = lastBufferTime;
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_influx2,9);
      if(request && request->readyState() == 4){
        HTTPrelease(HTTPtoken);
        trace(T_influx2,9);
        if(request->responseHTTPcode() < 0){
          if(++retryCount == 50){
            log("influxDB2: Post Failed: %d", request->responseHTTPcode());
          }
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return UTCtime() + (retryCount < 30 ? 1 : retryCount / 10);
        }

            // Check for unsuccessful post.

        if(request->responseHTTPcode() != 204){
        //   if(++retryCount == 10){
        //     DynamicJsonBuffer Json;
        //     JsonObject& results = Json.parseObject(request->responseText().c_str());
        //     if(results.success()){ 
        //       log("influxDB2: Post Failed: %d %s", request->responseHTTPcode(), results.get<const char*>("error"));
        //     } else {
        //       log("influxDB2: Post Failed: %d", request->responseHTTPcode());
        //     }
        //   }
        Serial.printf("influxDB2: Post failed: %d\n", request->responseHTTPcode());
        Serial.println(request->responseText());
        delete request;
        request = nullptr;
        state = getLastRecord;
        return UTCtime() + (retryCount < 10 ? 1 : 30);
        }

        trace(T_influx2,9);
        retryCount = 0;
        influx2LastPost = lastRequestTime; 
        state = post;
        trace(T_influx2,9);
      }
      return 1;
    }   
  }

  return 1;
}


bool influx2Config(const char* configObj){
  trace(T_influx2Config,0);
  DynamicJsonBuffer Json;
  JsonObject& config = Json.parseObject(configObj);
  trace(T_influx2Config,0);
  if( ! config.success()){
    log("influxDB2: Json parse failed.");
    return false;
  }
  int revision = config["revision"];
  if(revision == influx2Revision){
    return true;
  }
  trace(T_influx2Config,0);
  influx2Revision = revision;
  influx2Stop = config["stop"].as<bool>();
  influx2LogHeap = config["heap"].as<bool>();
  if( ! influx2URL){
    influx2URL = new xurl;
  }
  
  influx2URL->parse(config.get<char*>("url"));
  influx2URL->query(nullptr);
  trace(T_influx2Config,4);
  delete[] influx2Bucket;
  influx2Bucket = charstar(config.get<char*>("bucket"));
  influxDB2Interval = config.get<unsigned int>("postInterval");
  influx2BulkSend = config.get<unsigned int>("bulksend");
  if(influx2BulkSend <1) influx2BulkSend = 1;
  trace(T_influx2Config,5);
  delete[] influx2orgID;
  influx2orgID = charstar(config.get<const char*>("orgid"));
  delete[] influx2Token;
  influx2Token = charstar(config.get<const char*>("authtoken"));
  Serial.println(influx2Token);
  trace(T_influx2Config,7);
  delete[] influx2Retention;
  influx2Retention = charstar(config.get<const char*>("retp"));
  trace(T_influx2Config,7);
  delete[] influx2Measurement;
  influx2Measurement = charstar(config.get<const char*>("measurement"));
  if( ! influx2Measurement){
    influx2Measurement = charstar("$name");
  }
  trace(T_influx2Config,7);
  delete[] influx2FieldKey;;
  influx2FieldKey = charstar(config.get<const char*>("fieldkey"));
  if( ! influx2FieldKey){
    influx2FieldKey = charstar("value");
  }
  trace(T_influx2Config,7);
  influx2BeginPosting = config.get<uint32_t>("begdate");
  
  delete influx2TagSet;
  influx2TagSet = nullptr;
  JsonArray& tagset = config["tagset"];
  if(tagset.success()){
    trace(T_influx2Config,8);
    for(int i=tagset.size(); i>0;){
      i--;
      influxTag* tag = new influxTag;
      tag->next = influx2TagSet;
      influx2TagSet = tag;
      tag->key = charstar(tagset[i]["key"].as<const char*>());
      tag->value = charstar(tagset[i]["value"].as<const char*>());
      if((strstr(tag->value,"$units") != nullptr) || (strstr(tag->value,"$name") != nullptr)) influx2StaticKeyset = false;
    }
  }

        // Build the measurement scriptset

  delete influx2Outputs;
  influx2Outputs = nullptr;
  JsonVariant var = config["outputs"];
  if(var.success()){
    trace(T_influx2Config,9);
    influx2Outputs = new ScriptSet(var.as<JsonArray>());
  }
  else {
    return false;
  }

        // sort the measurements by measurement name

  influx2Outputs->sort([](Script* a, Script* b)->int {
    return strcmp(influx2VarStr(influx2Measurement, a).c_str(), influx2VarStr(influx2Measurement, b).c_str());
  });

  if( ! influx2Started) {
    trace(T_influx2Config,10);
    NewService(influx2Service, T_influx2);
    influx2Started = true;
  } else if(! influx2Stop) {
    influx2Restart = true;
  }
  log("influxDB2: config completed");
  return true;
}

String influx2VarStr(const char* in, Script* script){
  String out;
  while(*in){ 
    if(memcmp(in,"$device",7) == 0){
      out += deviceName;
      in += 7;
    }
    else if(memcmp(in,"$name",5) == 0){
      out += script->name();
      in += 5;
    }
    else if(memcmp(in,"$units",6) == 0){
      out += script->getUnits();
      in += 6;
    }
    else {
      out += *(in++);
    }
  } 
  return out;
}

