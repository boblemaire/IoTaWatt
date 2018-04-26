#include "IotaWatt.h"
#include "xbuf.h"

bool      influxStarted = false;                    // True when Service started
bool      influxStop = false;                       // Stop the influx service
bool      influxRestart = true;                     // Restart the influx service
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
influxTag* influxTagSet = nullptr;  
ScriptSet* influxOutputs;      
String    influxURL = "";
String    influxDataBase = "";

uint32_t influxService(struct serviceBlock* _serviceBlock){

      // This is a standard IoTaWatt Service operating as a state machine.

  enum   states {initialize,        // Basic startup of the service - one time
                 queryLastPostTime, // Setup to query for last post time of each measurement
                 queryLastGet,
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
  // Extern       influxLastPost                // Time of last measurement acknowledged by influx
  static uint32_t UnixNextPost = UNIXtime();    // Next measurement to be posted
  static xbuf reqData;                          // Current request buffer
  static uint32_t reqUnixtime = 0;              // First measurement in current reqData buffer
  static int  reqEntries = 0;                   // Number of measurement intervals in current reqData
  static int16_t retryCount = 0;                // HTTP error count
  static asyncHTTPrequest* request = nullptr;   // -> instance of asyncHTTPrequest
  static uint32_t postFirstTime = UNIXtime();   // First measurement in outstanding post request
  static uint32_t postLastTime = UNIXtime();    // Last measurement in outstanding post request
  static size_t reqDataLimit = 2000;            // transaction yellow light size


  trace(T_influx,0);                            // Announce entry

          // If restart, set to reinitialize. 

  if(influxRestart){
    trace(T_influx,1);
    state = initialize;
    influxRestart = false;
  }
      
          // Handle current state

  switch(state){

    case initialize: {
      trace(T_influx,2);

          // We post from the log, so wait if not available.          

      if(!currLog.isOpen()){                  
        return UNIXtime() + 5;
      }
      msgLog("influxDB: started.", "url: " + influxURL + ",port=" + String(influxPort) + ",db=" + influxDataBase + 
      ",post interval: " + String(influxDBInterval));
      state = queryLastPostTime;
      _serviceBlock->priority = priorityLow;
      return 1;
    }
 
    case queryLastPostTime:{
      trace(T_influx,3);
      influxLastPost = influxBeginPosting;
      state = queryLastGet;
      return 1;
    } 

    case queryLastGet:{
      trace(T_influx,4);

          // Make sure wifi is connected and there is a resource available.

      if( ! WiFi.isConnected() || ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;

          // Create a new request

      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = influxURL + ":" + String(influxPort) + "/query";
      request->setTimeout(5);
      request->setDebug(false);
      request->open("POST", URL.c_str());
      if(influxUser && influxPwd){
        xbuf xb;
        xb.printf("%s:%s", influxUser, influxPwd);
        base64encode(&xb);
        String auth = "Basic ";
        auth += xb.readString(xb.available());
        request->setReqHeader("Authorization", auth.c_str()); 
      }
      trace(T_influx,4);
      reqData.flush();
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      reqData.printf_P(PSTR("db=%s&epoch=s&q=select *::field from /.*/"), influxDataBase.c_str());
      if(influxTagSet){
        reqData.printf_P(PSTR(" where %s = \'%s\'"), influxTagSet->key, influxVarStr(influxTagSet->value, influxOutputs->first()).c_str());
      }
      reqData.write(" order by time desc limit 1");

          // Send the request

      request->send(&reqData, reqData.available());
      trace(T_influx,4);
      state = queryLastWait;
      return 1;
    }

    case queryLastWait: {

          // If not completed, return to wait.

      trace(T_influx,5); 
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrequestFree++;
      String response = request->responseText();
      int HTTPcode = request->responseHTTPcode();
      delete request;
      request = nullptr;
      if(HTTPcode != 200){
        msgLog("influxDB: last entry query failed: ", HTTPcode);
        influxStop = true;
        state = post;
        return 1;
      } 
      trace(T_influx,5);
      DynamicJsonBuffer Json;  
      trace(T_influx,5);
      JsonObject& result = Json.parseObject(response); 
      if(result.success()){
        trace(T_influx,5);
        JsonArray& lastSeries = result["results"][0]["series"];
        if(lastSeries.success()){
          for(int i=0; i<lastSeries.size(); i++){
            JsonArray& lastValues = lastSeries[i]["values"][0];
            if(lastValues.get<unsigned long>(0) > influxLastPost){
              influxLastPost = lastValues.get<unsigned long>(0);
            }
          }
        }
      }
      if(influxLastPost == 0){
        influxLastPost = UNIXtime();
        influxLastPost -= influxLastPost % influxDBInterval;
      }
      msgLog("influxDB: Start posting from ", dateString(influxLastPost + influxDBInterval));
      state = getLastRecord;
      return 1;
    }

    case getLastRecord: {
      trace(T_influx,6);   
      if( ! oldRecord){
        oldRecord = new IotaLogRecord;
      }
      oldRecord->UNIXtime = influxLastPost;      
      logReadKey(oldRecord);
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

    case waitPost: {
      trace(T_influx,9);
      if(request && request->readyState() == 4){
        HTTPrequestFree++;
        trace(T_influx,9);
        if(request->responseHTTPcode() != 204){
          if(++retryCount == 10){
            msgLog("influxDB: Post Failed: ", request->responseHTTPcode());
          }
          delete request;
          request = nullptr; 
          state = getLastRecord;
          return 1;
        }
        trace(T_influx,9);
        delete request;
        request = nullptr;
        retryCount = 0;
        influxLastPost = lastRequestTime; 
        state = post;
        trace(T_influx,9);
        return 1;
      }
    }   
    
    case post: {
      trace(T_influx,7);

          // If stop requested, do it now.

      if(influxStop) {
        if(request) return 1;
        trace(T_influx,71);
        msgLog("influxDB: Stopped. Last post ", dateString(influxLastPost));
        influxStarted = false;
        trace(T_influx,72);    
        state = initialize;
        delete oldRecord;
        oldRecord = nullptr;
        delete logRecord;
        logRecord = nullptr;
        delete request;
        request = nullptr;
        influxRevision = -1;
        return 0;
      }

      if(request && reqData.available() > reqDataLimit){
        return 1; 
      }  

          // If buffer isn't full,
          // add another measurement.

      if(reqData.available() < reqDataLimit && UnixNextPost <= currLog.lastKey()){  

            // Read the next log record.

        if( ! logRecord){
          logRecord = new IotaLogRecord;            
        }
        trace(T_influx,7);
        logRecord->UNIXtime = UnixNextPost;
        logReadKey(logRecord);
        trace(T_influx,7);
        
            // Compute the time difference between log entries.
            // If zero, don't bother.
            
        double elapsedHours = logRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
          UnixNextPost += influxDBInterval;
          return UnixNextPost;  
        }
            
            // Build the request string.
            // values for each channel are (delta value hrs)/(delta log hours) = period value.
            // Update the previous (Then) buckets to the most recent values.
      
        Script* script = influxOutputs->first();
        trace(T_influx,7);
        while(script){
          reqData.write(influxVarStr(influxMeasurement, script));
          if(influxTagSet){
            trace(T_influx,71);
            influxTag* tag = influxTagSet;
            while(tag){
              reqData.printf_P(PSTR(",%s=%s"), tag->key, influxVarStr(tag->value, script).c_str());
              tag = tag->next;
            }
          }
          char separator = ' ';
          double value = script->run(oldRecord, logRecord, elapsedHours);
          if(value == value){
            reqData.printf_P(PSTR("%c%s=%.*f"), separator,influxVarStr(influxFieldKey, script).c_str(), script->precision(), value);
            separator = ',';
          }
          reqData.printf(" %d\n", UnixNextPost);
          script = script->next();
        }
        trace(T_influx,7);
        delete oldRecord;
        oldRecord = logRecord;
        logRecord = nullptr;
        
        trace(T_influx,7);  
        reqEntries++;
        lastBufferTime = UnixNextPost;
        UnixNextPost +=  influxDBInterval - (UnixNextPost % influxDBInterval);
      }

            // If there's no request pending and we have bulksend entries,
            // set to post.

      if( ! request && reqEntries >= influxBulkSend && HTTPrequestFree){
        state = sendPost;
      }
      return 1;
    }

    case sendPost: {
      trace(T_influx,8);
      
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if(ESP.getFreeHeap() < 10000){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return 1;
      }
      HTTPrequestFree--;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = influxURL + ":" + String(influxPort) + "/write?precision=s&db=" + influxDataBase;
      if(influxRetention){
        URL.concat("&rp=");
        URL.concat(influxRetention);
      }
      request->setTimeout(3);
      request->setDebug(false);
      if(request->debug()){
        Serial.println(ESP.getFreeHeap()); 
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.println(msg);
        Serial.println(reqData.peekString(reqData.available()));
      }
      trace(T_influx,8);
      request->open("POST", URL.c_str());
      trace(T_influx,8);
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      request->setReqHeader("Connection","close");
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

    
  }

  return 1;
}

bool influxConfig(const char* configObj){
  trace(T_influxConfig,0);
  DynamicJsonBuffer Json;
  JsonObject& config = Json.parseObject(configObj);
  trace(T_influxConfig,0);
  if( ! config.success()){
    msgLog(F("influxDB: Json parse failed."));
    return false;
  }
  int revision = config["revision"];
  if(revision == influxRevision){
    return true;
  }
  trace(T_influxConfig,0);
  influxRevision = revision;
  influxStop = false;
  if(config["stop"].as<bool>()){
    trace(T_influxConfig,1);
    influxStop = true;
  }
  else if(influxStarted){
    trace(T_influxConfig,2);
    influxRestart = true;
  }
  trace(T_influxConfig,3);
  influxURL = config.get<String>("url");
  if(influxURL.startsWith("http")){
    influxURL.remove(0,4);
    if(influxURL.startsWith("s"))influxURL.remove(0,1);
    if(influxURL.startsWith(":"))influxURL.remove(0,1);
    while(influxURL.startsWith("/")) influxURL.remove(0,1);
  }  
  if(influxURL.indexOf(":") > 0){
    influxPort = influxURL.substring(influxURL.indexOf(":")+1).toInt();
    influxURL.remove(influxURL.indexOf(":"));
  }
  trace(T_influxConfig,4);
  influxDataBase = config.get<String>("database");
  influxDBInterval = config.get<unsigned int>("postInterval");
  influxBulkSend = config.get<unsigned int>("bulksend");
  trace(T_influxConfig,5);
  delete[] influxUser;
  influxUser = charstar(config.get<const char*>("user"));
  delete[] influxPwd;
  influxPwd = charstar(config.get<const char*>("pwd"));  
  if(influxBulkSend <1) influxBulkSend = 1;
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
    }
  }

  delete influxOutputs;
  JsonVariant var = config["outputs"];
  if(var.success()){
    trace(T_influxConfig,9);
    influxOutputs = new ScriptSet(var.as<JsonArray>()); 
  }
  if( ! influxStarted) {
    trace(T_influxConfig,10);
    NewService(influxService);
    influxStarted = true;
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

