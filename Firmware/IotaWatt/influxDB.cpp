#include "IotaWatt.h"

boolean influxSendData(uint32_t reqUnixtime, String reqData);

uint32_t influxService(struct serviceBlock* _serviceBlock){
  // trace T_influx
  enum   states {initialize, post, resend};
  static states state = initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static File influxPostLog;
  static double accum1Then [MAXINPUTS];
  static uint32_t UnixLastPost = UNIXtime();
  static uint32_t UnixNextPost = UNIXtime();
  static double _logHours;
  static double elapsedHours;
  static String reqData = "";
  static uint32_t reqUnixtime = 0;
  static int  reqEntries = 0; 
  static uint32_t postTime = millis();
  struct SDbuffer {uint32_t data; SDbuffer(){data = 0;}};
  static SDbuffer* buf = new SDbuffer;
          
  trace(T_influx,0);

            // If stop signaled, do so.  
    if(influxStop) {
    msgLog(F("influxDB: stopped."));
    influxStarted = false;
    trace(T_influx,4);
    influxPostLog.close();
    trace(T_influx,5);
    SD.remove((char *)influxPostLogFile.c_str());
    trace(T_influx,6);
    state = initialize;
    return 0;
  }
  if(influxInitialize){
    state = initialize;
    influxInitialize = false;
  }
      
  switch(state){

    case initialize: {

          // We post the log to influx,
          // so wait until the log service is up and running.
      
      if(!iotaLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("influxDB: started.", "url: " + influxURL + ",port=" + String(influxPort) + ",db=" + influxDataBase + 
      ",post interval: " + String(influxDBInterval));
    
      if(!influxPostLog){
        influxPostLog = SD.open(influxPostLogFile,FILE_WRITE);
      }
            
      if(influxPostLog){
        if(influxPostLog.size() == 0){
          buf->data = iotaLog.lastKey();
          influxPostLog.write((byte*)buf,4);
          influxPostLog.flush();
          msgLog(F("influxService: influxlog file created."));
        }
        influxPostLog.seek(influxPostLog.size()-4);
        influxPostLog.read((byte*)buf,4);
        logRecord->UNIXtime = buf->data;
        msgLog("influxDB: Start posting from ", String(logRecord->UNIXtime));
      }
      else {
        logRecord->UNIXtime = iotaLog.lastKey();
      }
      
          // Get the last record in the log.
          // Posting will begin with the next log entry after this one,
            
      logReadKey(logRecord);

          // Save the value*hrs to date, and logHours to date
      
      for(int i=0; i<maxInputs; i++){ 
        accum1Then[i] = logRecord->channel[i].accum1;
        if(accum1Then[i] != accum1Then[i]) accum1Then[i] = 0;
      }
      _logHours = logRecord->logHours;
      if(_logHours != _logHours) _logHours = 0;

          // Assume that record was posted (not important).
          // Plan to start posting one interval later
      
      UnixLastPost = logRecord->UNIXtime;
      UnixNextPost = UnixLastPost + influxDBInterval - (UnixLastPost % influxDBInterval);
      
          // Advance state.
          // Set task priority low so that datalog will run before this.

      reqData = "";
      reqEntries = 0;
      state = post;
      _serviceBlock->priority = priorityLow;
      return UnixNextPost;
    }
    
    case post: {

          // If WiFi is not connected,
          // just return without attempting to log and try again in a few seconds.

      if(WiFi.status() != WL_CONNECTED) {
        return 2; 
      }

          // If we are current,
          // Anticipate next posting at next regular interval and break to reschedule.
 
      if(iotaLog.lastKey() < UnixNextPost){ 
        UnixNextPost = UNIXtime() + influxDBInterval - (UNIXtime() % influxDBInterval);
        return UnixNextPost;
      } 
      
          // Not current.  Read the next log record.
          
      trace(T_influx,1);
      logRecord->UNIXtime = UnixNextPost;
      logReadKey(logRecord);    
      
          // Compute the time difference between log entries.
          // If zero, don't bother.
          
      elapsedHours = logRecord->logHours - _logHours;
      if(elapsedHours == 0){
        UnixNextPost += influxDBInterval;
        return UnixNextPost;  
      }
      
          // If new request, save the uinx time;
      
      if(reqData.length() == 0){
        reqUnixtime = UnixNextPost;
      }
     
          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.
     
      trace(T_influx,2);
      Script* script = influxOutputs->first();
      while(script){
        reqData += script->name();
        double value = script->run([](int i)->double {return (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;});
        reqData += " value=" + String(value,1) + ' ' + String(UnixNextPost) + "\n";
        script = script->next();
      }

      _logHours = logRecord->logHours;
      for(int i=0; i<MAXINPUTS; ++i){
        accum1Then[i] = logRecord->channel[i].accum1;
      }
      
      trace(T_influx,3);  
      reqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  influxDBInterval - (UnixNextPost % influxDBInterval);
      
      if ((reqEntries < influxBulkSend) ||
         ((iotaLog.lastKey() > UnixNextPost) &&
         (reqData.length() < 1000))) {
        return UnixNextPost;
      }

          // Send the post       

      if(!influxSendData(reqUnixtime, reqData)){
        state = resend;
        return UNIXtime() + 30;
      }
      buf->data = UnixLastPost;
      influxPostLog.write((byte*)buf,4);
      influxPostLog.flush();
      reqData = "";
      reqEntries = 0;    
      state = post;
      return UnixNextPost;
    }
  

    case resend: {
      msgLog(F("Resending influx data."));
      if(!influxSendData(reqUnixtime, reqData)){ 
        return UNIXtime() + 60;
      }
      else {
        buf->data = UnixLastPost;
        influxPostLog.write((byte*)buf,4);
        influxPostLog.flush();
        reqData = "";
        reqEntries = 0;  
        state = post;
        return 1;
      }
      break;
    }
  }
  return 1;
}

/************************************************************************************************
 *  influxSend - send data to the influx server. 
 *  if secure transmission is configured, pas sthe request to a 
 *  similar WiFiClientSecure function.
 *  Secure takes about twice as long and can block sampling for more than a second.
 ***********************************************************************************************/
boolean influxSendData(uint32_t reqUnixtime, String reqData){ 
  trace(T_influx,7);
  uint32_t startTime = millis();
  String URI = "/write?precision=s&db=" + influxDataBase;
  Serial.print(influxURL);
  Serial.println(URI);
  http.begin(influxURL, influxPort, URI);
  http.addHeader("Host",influxURL);
  http.addHeader("Content-Type","application/x-www-form-urlencoded");
  http.setTimeout(500);
  Serial.print(reqData);
  int httpCode = http.POST(reqData);
  String response = http.getString();
  http.end();
  if(httpCode != HTTP_CODE_OK && httpCode != 204){
    String code = String(httpCode);
    if(httpCode < 0){
      code = http.errorToString(httpCode);
    }
    msgLog("influxDB: POST failed. HTTP code: ", code);
    Serial.println(response);
    return false;
  } 
  return true;
}

 
