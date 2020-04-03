#include "IotaWatt.h"
#include "xbuf.h"
#include "xurl.h"

bool      influxStarted = false;                    // True when Service started
bool      influxStop = false;                       // Stop the influx service
bool      influxRestart = false;                    // Restart the influx service
bool      influxLogHeap = false;                    // Post a heap size measurement (diag)
bool      influxStaticKeyset = true;                // True if keyset has no variables (except $device)
uint32_t  influxLastPost = 0;                       // Last acknowledge post for status report

    // Configuration settings

uint16_t  influxBulkSend = 1;                       
uint16_t  influxPort = 8086;
int32_t   influxRevision = -1;                      // Revision control for dynamic config
uint32_t  influxBeginPosting = 0;                   // Begin date specified in config
char*     influxUser = nullptr;
char*     influxPwd = nullptr; 
char*     influxRetention = nullptr;
char*     influxMeasurement = nullptr;
char*     influxFieldKey = nullptr; 
xurl*      influxURL = nullptr;
char*     influxDataBase = nullptr;
influxTag* influxTagSet = nullptr;  
ScriptSet* influxOutputs;      

uint32_t influxService(struct serviceBlock* _serviceBlock){

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
  static size_t reqDataLimit = 4000;            // transaction yellow light size
  static uint32_t HTTPtoken = 0;                // HTTP resource reservation token
  static Script* script = nullptr;              // current Script


  trace(T_influx,0);                            // Announce entry
      
          // Handle current state

  switch(state){

//********************************************************* Initialize **************************************
    case initialize: {
      trace(T_influx,2);
      if(influxStop){
        influxStarted = false;
        return 0;
      }

          // We post from the log, so wait if not available.          

      if(!Current_log.isOpen()){                  
        return UTCtime() + 5;
      }
      log("influxDB: started, url=%s, db=%s, interval=%d", influxURL->build().c_str(),
              influxDataBase, influxDBInterval);
      state = queryLastPostTime;
      trace(T_influx,2);
      return 1;
    }
 
 //********************************************************* queryLastPostTime *****************************
    case queryLastPostTime:{
      trace(T_influx,3);
      influxLastPost = influxBeginPosting;
      script = influxOutputs->first();
      retryCount = 0;
      trace(T_influx,4);
      state = queryLast;
      return 1;
    }

  //********************************************************* queryLast *****************************    

    case queryLast:{

          // Make sure wifi is connected and there is a resource available.

      if( ! WiFi.isConnected()) return UTCtime() + 1;
       
      HTTPtoken = HTTPreserve(T_influx);
      if( ! HTTPtoken){
        return UTCtime() + 1;
      }

          // Create a new request

      if( ! request) request = new asyncHTTPrequest;
      request->setTimeout(5);
      request->setDebug(false);
      {
        char URL[100];
        sprintf_P(URL, PSTR("%s/query"),influxURL->build().c_str());
        if( ! request->open("POST", URL)){
          HTTPrelease(HTTPtoken);
          return UTCtime() + 2;
        }
      }
      
      if(influxUser && influxPwd){
        xbuf xb;
        xb.printf("%s:%s", influxUser, influxPwd);
        base64encode(&xb);
        String auth = "Basic ";
        auth += xb.readString(xb.available());
        request->setReqHeader("Authorization", auth.c_str()); 
      }
      trace(T_influx,4);
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      reqData.flush();
      reqData.printf_P(PSTR("db=%s&epoch=s&q=SELECT LAST(%s) FROM %s"), influxDataBase,
            influxVarStr(influxFieldKey, script).c_str(),
            influxVarStr(influxMeasurement, script).c_str());
      influxTag* tag = influxTagSet;
      trace(T_influx,41);
      while(tag){
        reqData.printf_P(PSTR(" %s %s=\'%s\'"), tag == influxTagSet ? "WHERE" : "AND", tag->key, influxVarStr(tag->value, script).c_str());
        tag = tag->next;
      }
      
          // Send the request

      if( ! request->send(&reqData, reqData.available())){
        HTTPrelease(HTTPtoken);
        request->abort();
        return UTCtime() + 2;
      }
      trace(T_influx,42);
      state = queryLastWait;
      return 1;
    }

    case queryLastWait: {

          // If not completed, return to wait.

      trace(T_influx,5); 
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrelease(HTTPtoken);
      if(influxStop || influxRestart){
        state = post;
        return 1;
      }
      String response = request->responseText(); 
      int HTTPcode = request->responseHTTPcode();
      
      if(HTTPcode < 0){
        if(retryCount++ == 10){
          log("influxDB: last entry query failed: %d, retrying.", HTTPcode);
        }
        state = queryLast;
        return UTCtime() + (retryCount <= 10 ? 2 : 10);
      }
      delete request;
      request = nullptr;
      retryCount = 0;
      trace(T_influx,5);

            // Check for authentication error

      if(HTTPcode == 401){
        log("influxDB: Authentication failed. Stopping influx service.");
        influxStop = true;
        state = post;
        return 1;
      }

            // Check for invalid request.

      if(HTTPcode != 200){
        log("influxDB: Last entry query failed.");
        log("influxDB: HTTPcode %d, %.60s", HTTPcode, response.c_str());
        influxStop = true;
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
            log("influxDB: last entry query failed %d %s", HTTPcode, error);
            influxStop = true;
            state = post;
            return 1;
          }
          JsonArray& columns = results["results"][0]["series"][0]["columns"];
          JsonArray& values = results["results"][0]["series"][0]["values"][0];
          if(columns.success() && values.success()){
            for(int i=0; i<columns.size(); i++){
              if(strcmp("time",columns[i].as<char*>()) == 0){
                if(values[i].as<unsigned long>() > influxLastPost){
                  influxLastPost = values[i].as<unsigned long>();
                }
                break;
              }
            }
          } else {
            const char* error = results["results"][0]["error"].as<const char*>();
            if(error){
              log("influxDB: last entry query failed %d %s", HTTPcode, error);
              influxStop = true;
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

      if(influxLastPost == 0){
        influxLastPost = UTCtime();
      }
      influxLastPost -= influxLastPost % influxDBInterval;
      log("influxDB: Start posting at %s", localDateString(influxLastPost + influxDBInterval).c_str());
      delete request;
      request = nullptr;
      state = getLastRecord;
      return 1;
    }

    case getLastRecord: {
      trace(T_influx,6);   
      if( ! oldRecord){
        oldRecord = new IotaLogRecord;
      }
      oldRecord->UNIXtime = influxLastPost;      
      Current_log.readKey(oldRecord);
      trace(T_influx,6);

          // Assume that record was posted (not important).
          // Plan to start posting one interval later
      
      UnixNextPost = oldRecord->UNIXtime + influxDBInterval - (oldRecord->UNIXtime % influxDBInterval);
      
          // Advance state.

      reqData.flush();
      reqEntries = 0;
      state = post;
      return UnixNextPost;
    }

    
    case post: {
      trace(T_influx,7);

          // If stop requested, do it now.

      if(influxStop || influxRestart) {
        if(request && request->readyState() < 4) return 1;
        trace(T_influx,71);
        state = initialize;
        if(influxRestart){
          log("influxDB: Restart. Last post %s", localDateString(influxLastPost).c_str());
          influxRestart = false;
          return 1;
        } else {
          log("influxDB: Stopped. Last post %s", localDateString(influxLastPost).c_str());
          trace(T_influx,72);    
          delete oldRecord;
          oldRecord = nullptr;
          delete logRecord;
          logRecord = nullptr;
          delete request;
          request = nullptr;
          reqData.flush();
          delete[] influxUser;
          influxUser = nullptr;
          delete[] influxPwd;
          influxPwd = nullptr; 
          delete[] influxRetention;
          influxRetention = nullptr;
          delete[] influxMeasurement;
          influxMeasurement = nullptr;
          delete[] influxFieldKey;
          influxFieldKey = nullptr; 
          delete influxURL;
          influxURL = nullptr;
          delete[] influxDataBase;
          influxDataBase = nullptr;
          delete influxTagSet;
          influxTagSet = nullptr;  
          delete influxTagSet;
          influxOutputs;      
          influxStarted = false;
          return 0;
        }
        
      }
      
          // If not enough entries for bulk-send, come back in one second;

      if(((Current_log.lastKey() - influxLastPost) / influxDBInterval + reqEntries) < influxBulkSend){
        return UTCtime() + 1;
      }

          // If buffer isn't full,
          // add another measurement.

      if(reqData.available() < reqDataLimit && UnixNextPost <= Current_log.lastKey()){  

            // Read the next log record.

        if( ! logRecord){
          logRecord = new IotaLogRecord;            
        }
        trace(T_influx,7);
        logRecord->UNIXtime = UnixNextPost;
        Current_log.readKey(logRecord);
        trace(T_influx,7);
        
            // Compute the time difference between log entries.
            // If zero, don't bother.
            
        double elapsedHours = logRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
          if(Current_log.readNext(logRecord) == 0) {
            UnixNextPost = logRecord->UNIXtime - (logRecord->UNIXtime % influxDBInterval);
          }
          UnixNextPost += influxDBInterval;
          return UnixNextPost;  
        }
            
            // Build the request string.
            // values for each channel are (delta value hrs)/(delta log hours) = period value.
            // Update the previous (Then) buckets to the most recent values.
      
        script = influxOutputs->first();
        trace(T_influx,7);
        String lastMeasurement;
        String thisMeasurement;
        while(script){
          double value = script->run(oldRecord, logRecord, elapsedHours);
          if(value == value){
            thisMeasurement = influxVarStr(influxMeasurement, script);
            if(influxStaticKeyset && thisMeasurement.equals(lastMeasurement)){
              reqData.printf_P(PSTR(",%s=%.*f"), influxVarStr(influxFieldKey, script).c_str(), script->precision(), value);
            } else {
              if(lastMeasurement.length()){
                reqData.printf(" %d\n", UnixNextPost);
              }
              reqData.write(thisMeasurement);
              if(influxTagSet){
                trace(T_influx,71);
                influxTag* tag = influxTagSet;
                while(tag){
                  reqData.printf_P(PSTR(",%s=%s"), tag->key, influxVarStr(tag->value, script).c_str());
                  tag = tag->next;
                }
              }
              reqData.printf_P(PSTR(" %s=%.*f"), influxVarStr(influxFieldKey, script).c_str(), script->precision(), value);
            }
          }
          lastMeasurement = thisMeasurement;
          script = script->next();
        }
        reqData.printf(" %d\n", UnixNextPost);

        trace(T_influx,7);
        delete oldRecord;
        oldRecord = logRecord;
        logRecord = nullptr;
        
        trace(T_influx,7);  
        reqEntries++;
        lastBufferTime = UnixNextPost;
        UnixNextPost +=  influxDBInterval - (UnixNextPost % influxDBInterval);
        return 1;
      }

            // If there's no request pending and we have bulksend entries,
            // set to post.

      if((( ! request || request->readyState() == 4) && HTTPrequestFree) && 
          (reqEntries >= influxBulkSend || reqData.available() >= reqDataLimit)){
        state = sendPost;
        if(influxLogHeap && heapMsPeriod != 0){
          reqData.printf_P(PSTR("heap"));
          influxTag* tag = influxTagSet;
          if(tag){
            Script* script = influxOutputs->first();
            reqData.printf_P(PSTR(",%s=%s"), tag->key, influxVarStr(tag->value, script).c_str());
          }
          reqData.printf_P(PSTR(" value=%d %d\n"), (uint32_t)heapMs / heapMsPeriod, UTCtime());
          heapMs = 0.0;
          heapMsPeriod = 0;
        }       
      }
      return (UnixNextPost > UTCtime()) ? UTCtime() + 1 : 1;
    }

    case sendPost: {
      trace(T_influx,8);
      if( ! WiFi.isConnected()){
        return UTCtime() + 1;
      }
      HTTPtoken = HTTPreserve(T_influx);
      if( ! HTTPtoken){
        return 1;
      }
      if( ! request){
        request = new asyncHTTPrequest;
      }
      request->setTimeout(3);
      request->setDebug(false);
      if(request->debug()){
        Serial.println(ESP.getFreeHeap()); 
        Serial.println(datef(localTime(),"hh:mm:ss"));
        Serial.println(reqData.peekString(reqData.available()));
      }
      trace(T_influx,8);
      {
        char URL[128];
        size_t len = sprintf_P(URL, PSTR("%s/write?precision=s&db=%s"), influxURL->build().c_str(), influxDataBase);
        if(influxRetention){
          sprintf(URL+len,"&rp=%s", influxRetention);
        }
        if( ! request->open("POST", URL)){
          HTTPrelease(HTTPtoken);
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return 1;
        }
      }
      trace(T_influx,8);
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_influx,8);
      if(influxUser && influxPwd){
        xbuf xb;
        xb.printf("%s:%s", influxUser, influxPwd);
        base64encode(&xb);
        String auth = "Basic ";
        auth += xb.readString(xb.available());
        request->setReqHeader("Authorization", auth.c_str()); 
      }
      request->send(&reqData, reqData.available());
      reqEntries = 0;
      lastRequestTime = lastBufferTime;
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_influx,9);
      if(request && request->readyState() == 4){
        HTTPrelease(HTTPtoken);
        trace(T_influx,9);
        if(request->responseHTTPcode() < 0){
          if(++retryCount == 50){
            log("influxDB: Post Failed: %d", request->responseHTTPcode());
          }
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return UTCtime() + (retryCount < 30 ? 1 : retryCount / 10);
        }

            // Check for unsuccessful post.

        if(request->responseHTTPcode() != 204){
          if(++retryCount == 10){
            DynamicJsonBuffer Json;
            JsonObject& results = Json.parseObject(request->responseText().c_str());
            if(results.success()){ 
              log("influxDB: Post Failed: %d %s", request->responseHTTPcode(), results.get<const char*>("error"));
            } else {
              log("influxDB: Post Failed: %d", request->responseHTTPcode());
            }
          }
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return UTCtime() + (retryCount < 10 ? 1 : 30);
        }

        trace(T_influx,9);
        retryCount = 0;
        influxLastPost = lastRequestTime; 
        state = post;
        trace(T_influx,9);
      }
      return 1;
    }   
  }

  return 1;
}


bool influxConfig(const char* configObj){
  trace(T_influxConfig,0);
  DynamicJsonBuffer Json;
  JsonObject& config = Json.parseObject(configObj);
  trace(T_influxConfig,0);
  if( ! config.success()){
    log("influxDB: Json parse failed.");
    return false;
  }
  int revision = config["revision"];
  if(revision == influxRevision){
    return true;
  }
  trace(T_influxConfig,0);
  influxRevision = revision;
  influxStop = config["stop"].as<bool>();
  influxLogHeap = config["heap"].as<bool>();
  if( ! influxURL){
    influxURL = new xurl;
  }
  
  influxURL->parse(config.get<char*>("url"));
  influxURL->query(nullptr);
  if( ! influxURL->port()){
    influxURL->port(":8086");
  }
  influxURL->method("HTTP://");
  trace(T_influxConfig,4);
  delete[] influxDataBase;
  influxDataBase = charstar(config.get<char*>("database"));
  influxDBInterval = config.get<unsigned int>("postInterval");
  influxBulkSend = config.get<unsigned int>("bulksend");
  if(influxBulkSend <1) influxBulkSend = 1;
  trace(T_influxConfig,5);
  delete[] influxUser;
  influxUser = charstar(config.get<const char*>("user"));
  delete[] influxPwd;
  influxPwd = charstar(config.get<const char*>("pwd"));  
  trace(T_influxConfig,7);
  delete[] influxRetention;
  influxRetention = charstar(config.get<const char*>("retp"));
  trace(T_influxConfig,7);
  delete[] influxMeasurement;
  influxMeasurement = charstar(config.get<const char*>("measurement"));
  if( ! influxMeasurement){
    influxMeasurement = charstar("$name");
  }
  trace(T_influxConfig,7);
  delete[] influxFieldKey;;
  influxFieldKey = charstar(config.get<const char*>("fieldkey"));
  if( ! influxFieldKey){
    influxFieldKey = charstar("value");
  }
  trace(T_influxConfig,7);
  influxBeginPosting = config.get<uint32_t>("begdate");
  
  delete influxTagSet;
  influxTagSet = nullptr;
  JsonArray& tagset = config["tagset"];
  if(tagset.success()){
    trace(T_influxConfig,8);
    for(int i=tagset.size(); i>0;){
      i--;
      influxTag* tag = new influxTag;
      tag->next = influxTagSet;
      influxTagSet = tag;
      tag->key = charstar(tagset[i]["key"].as<const char*>());
      tag->value = charstar(tagset[i]["value"].as<const char*>());
      if((strstr(tag->value,"$units") != nullptr) || (strstr(tag->value,"$name") != nullptr)) influxStaticKeyset = false;
    }
  }

        // Build the measurement scriptset

  delete influxOutputs;
  influxOutputs = nullptr;
  JsonVariant var = config["outputs"];
  if(var.success()){
    trace(T_influxConfig,9);
    influxOutputs = new ScriptSet(var.as<JsonArray>());
  }
  else {
    return false;
  }

        // sort the measurements by measurement name

  influxOutputs->sort([](Script* a, Script* b)->int {
    return strcmp(influxVarStr(influxMeasurement, a).c_str(), influxVarStr(influxMeasurement, b).c_str());
  });

  if( ! influxStarted) {
    trace(T_influxConfig,10);
    NewService(influxService, T_influx);
    influxStarted = true;
  } else if(! influxStop) {
    influxRestart = true;
  }
  return true;
}

String influxVarStr(const char* in, Script* script){
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

