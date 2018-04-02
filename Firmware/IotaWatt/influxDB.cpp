#include "IotaWatt.h"
#include "xbuf.h"

bool      influxStarted = false;                    // True when Service started
bool      influxStop = false;                       // Stop the influx service
bool      influxRestart = true;                     // Restart the influx service
String    influxURL = "";
String    influxDataBase = "";

uint16_t  influxBulkSend = 1;
uint16_t  influxPort = 8086;
int32_t   influxRevision = -1;
char*     influxMeasurement;
char*     influxUser = nullptr;
char*     influxPwd = nullptr;  
influxTag* influxTagSet = nullptr;  
ScriptSet* influxOutputs;      

uint32_t influxService(struct serviceBlock* _serviceBlock){
  // trace T_influx

      // This is a standard IoTaWatt Service operating as a state machine.

  enum   states {initialize,        // Basic startup of the service - one time
                 queryLast,         // Issue query to get time of last post
                 queryLastWait,     // wait for [async] query to complete
                 getLastRecord,     // Read the logRec and prep the context for logging
                 post,              // Add a measurement to the reqData xbuf
                 sendPost,          // Send the accumulated measurements
                 waitPost};         // Wait for the [async] post to complete

  static states state = initialize;
  static IotaLogRecord* logRecord = nullptr;
  static IotaLogRecord* oldRecord = nullptr;
  static uint32_t UnixLastPost = UNIXtime();
  static uint32_t UnixNextPost = UNIXtime();
  static uint32_t reqUnixtime = 0;
  static double elapsedHours; 
  static int  reqEntries = 0; 
  static int16_t retryCount = 0;
  static xbuf reqData;
  static asyncHTTPrequest* request = nullptr;
  static String* response = nullptr;

  if( ! _serviceBlock) return 0;
  trace(T_influx,0);

            // If stop signaled, do so. 

  if(influxStop && state != waitPost) {
    trace(T_influx,1);
    msgLog(F("influxDB: stopped."));
    influxStarted = false;
    trace(T_influx,4);    
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
  if(influxRestart){
    trace(T_influx,2);
    state = initialize;
    influxRestart = false;
  }
      
  switch(state){

    case initialize: {
      trace(T_influx,3);

          // We post the log to influx,
          // so wait until the log service is up and running.
      
      if(!currLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("influxDB: started.", "url: " + influxURL + ",port=" + String(influxPort) + ",db=" + influxDataBase + 
      ",post interval: " + String(influxDBInterval));
      state = queryLast;
      return 1;
    }
 
    case queryLast:{
      trace(T_influx,4);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = influxURL + ":" + String(influxPort) + "/query";
      request->setTimeout(3);
      request->setDebug(false);
      request->open("POST", URL.c_str());
      trace(T_influx,4);
      reqData.flush();
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      reqData.printf_P(PSTR("db=%s&epoch=s&q=select last(%s) from %s"), influxDataBase.c_str(), influxOutputs->first()->name(), influxMeasurement);
      if(influxTagSet){
        reqData.printf_P(PSTR(" where %s = '%s'"), influxTagSet->key, influxTagSet->value);
      }
      request->send(&reqData, reqData.available());
      trace(T_influx,4);
      state = queryLastWait;
      return 1;
    }

    case queryLastWait: {
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
        msgLog("influxService: last entry query failed: ", HTTPcode);
        UnixLastPost = currLog.lastKey();
        state = getLastRecord;
        return 1;
      } 
      trace(T_influx,5);
      UnixLastPost = currLog.lastKey() - 604800L;
      DynamicJsonBuffer Json;  
      trace(T_influx,6);
      JsonObject& result = Json.parseObject(response); 
      if( ! result.success()){
        msgLog(F("influxService: Json parse of last post query failed."));
        UnixLastPost = currLog.lastKey();
        state = getLastRecord;
        return 1;
      }
      trace(T_influx,6);
      JsonArray& lastColumns = result["results"][0]["series"][0]["columns"];
      trace(T_influx,6);
      JsonArray& lastValues = result["results"][0]["series"][0]["values"][0];
      trace(T_influx,6);
      if(lastColumns.success() && lastValues.success()){
        for(int i=0; i<lastColumns.size(); i++){
          if(strcmp("time", lastColumns.get<const char*>(i)) == 0){
            UnixLastPost = lastValues.get<unsigned long>(i);         
          }
        }
      }
      msgLog("influxService: start posting after ", UnixLastPost);        
      trace(T_influx,6);
      state = getLastRecord;
      return 1;
    }

    case getLastRecord: {
      trace(T_influx,7);
      if( ! oldRecord){
        oldRecord = new IotaLogRecord;
      }
      oldRecord->UNIXtime = UnixLastPost;      
      logReadKey(oldRecord);
      trace(T_influx,7);

          // Assume that record was posted (not important).
          // Plan to start posting one interval later
      
      UnixLastPost = oldRecord->UNIXtime;
      UnixNextPost = UnixLastPost + influxDBInterval - (UnixLastPost % influxDBInterval);
      
          // Advance state.
          // Set task priority low so that datalog will run before this.

      reqData.flush();
      reqEntries = 0;
      state = post;
      _serviceBlock->priority = priorityLow;
      return UnixNextPost;
    }
    
    case post: {
      trace(T_influx,8);

          // If WiFi is not connected,
          // just return without attempting to log and try again in a few seconds.

      if(WiFi.status() != WL_CONNECTED) {
        return UNIXtime() + 1; 
      }
 
          // Determine when the next post should occur and wait if needed.
          // Careful here - arithmetic is unsigned.

      uint32_t nextBulkPost = UnixNextPost + ((influxBulkSend > reqEntries) ? influxBulkSend - reqEntries : 0) * influxDBInterval;
      if(currLog.lastKey() < nextBulkPost){
        return nextBulkPost;
      }

          // Read the next log record.

      if( ! logRecord){
        logRecord = new IotaLogRecord;            
      }
      trace(T_influx,8);
      logRecord->UNIXtime = UnixNextPost;
      logReadKey(logRecord);
      trace(T_influx,8);
      
          // Compute the time difference between log entries.
          // If zero, don't bother.
          
      elapsedHours = logRecord->logHours - oldRecord->logHours;
      if(elapsedHours == 0){
        UnixNextPost += influxDBInterval;
        return UnixNextPost;  
      }
      
          // If new request, save the start time;
      
      if(reqData.available() == 0){
        reqUnixtime = UnixNextPost;
      }
     
          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.
     
      trace(T_influx,8);
      reqData.write(influxMeasurement);
      if(influxTagSet){
        trace(T_influx,81);
        influxTag* tag = influxTagSet;
        while(tag){
          reqData.printf_P(PSTR(",%s=%s"), tag->key, tag->value);
          tag = tag->next;
        }
      }
      Script* script = influxOutputs->first();
      char separator = ' ';
      trace(T_influx,8);
      while(script){
        double value = script->run(oldRecord, logRecord, elapsedHours);
        if(value == value){
          reqData.printf_P(PSTR("%c%s=%.*f"), separator, script->name(), script->precision(), value);
          separator = ',';
        }
        script = script->next();
      }
      trace(T_influx,8);
      reqData.printf(" %d\n", UnixNextPost);
      delete oldRecord;
      oldRecord = logRecord;
      logRecord = nullptr;
      
      trace(T_influx,8);  
      reqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  influxDBInterval - (UnixNextPost % influxDBInterval);
      
      if ((reqEntries < influxBulkSend) ||
         ((currLog.lastKey() > UnixNextPost) &&
         (reqData.available() < 2000))) {
        return UnixNextPost;
      }

          // Send the post  

      state = sendPost;
      return 1;
    }

    case sendPost: {
      trace(T_influx,9);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = influxURL + ":" + String(influxPort) + "/write?precision=s&db=" + influxDataBase;
      request->setTimeout(3);
      request->setDebug(false);
      if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.println(msg);
        Serial.println(reqData.peekString(reqData.available()));
      }
      Serial.printf("ms: %d, time: %d\r\n", millis(), UnixNextPost);
      trace(T_influx,9);
      request->open("POST", URL.c_str());
      trace(T_influx,9);
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_influx,9);
      if(strlen(influxUser)){
        xbuf xb;
        xb.printf("%s:%s", influxUser, influxPwd);
        base64encode(&xb);
        String auth = "Basic ";
        auth += xb.readString(xb.available());
        request->setReqHeader("Authorization", auth.c_str()); 
      }
      request->send(&reqData, reqData.available());
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_influx,10);
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrequestFree++;
      trace(T_influx,10);
      if(request->responseHTTPcode() != 204){
        if(++retryCount < 10){
          state = sendPost;
          return UNIXtime() + 1;
        }
        retryCount = 0;
        msgLog("influxService: Post Failed: ", request->responseHTTPcode());
        Serial.println(request->responseText());
        UnixLastPost = reqUnixtime - influxDBInterval;
        state = getLastRecord;
        return 1;
      }
      trace(T_influx,10);
      delete request;
      request = nullptr;
      retryCount = 0;
      reqData.flush();
      reqEntries = 0;    
      state = post;
      return UnixNextPost + influxBulkSend ? 1 : 0;
    }   
  }

  return 1;
}

bool influxConfig(const char* configObj){
  DynamicJsonBuffer Json;
  JsonObject& config = Json.parseObject(configObj);
  if( ! config.success()){
    msgLog(F("influxService: Json parse failed."));
    return false;
  }
  int revision = config["revision"];
  if(revision == influxRevision){
    return true;
  }
  influxRevision = revision;
  if(influxStarted){
    influxRestart = true;
  }
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
  influxDataBase = config.get<String>("database");
  influxDBInterval = config.get<unsigned int>("postInterval");
  influxBulkSend = config.get<unsigned int>("bulksend");
  delete influxMeasurement;
  influxMeasurement = charstar(config.get<const char*>("measurement"));
  delete influxUser;
  influxUser = charstar(config.get<const char*>("user"));
  delete influxPwd;
  influxPwd = charstar(config.get<const char*>("pwd"));  
  if(influxBulkSend <1) influxBulkSend = 1;

  delete influxTagSet;
  influxTagSet = nullptr;
  JsonArray& tagset = config["tagset"];
  if(tagset.success()){
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
    influxOutputs = new ScriptSet(var.as<JsonArray>()); 
  }
  if( ! influxStarted) {
    NewService(influxService);
    influxStarted = true;
    influxStop = false;
  }
  return true;
}

