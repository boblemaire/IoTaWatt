    /*******************************************************************************************************
 * eMonService - This SERVICE posts entries from the IotaLog to eMonCMS.  Details of the eMonCMS
 * account are provided in the configuration file at startup and this SERVICE is scheduled.  It runs
 * more or less independent of everything else, just reading the log records as they become available
 * and sending the data out.
 * The advantage of doing it this way is that there is really no eMonCMS specific code anywhere else
 * except a speciific section in getConfig.  Other web data logging services could be handled
 * the same way.
 * It's possible that multiple web services could be updated independently, each having their own 
 * SERVER.  The only issue right now would be the WiFi resource.  A future move to the 
 * asynchWifiClient would solve that.
 ******************************************************************************************************/
uint32_t eMonService(struct serviceBlock* _serviceBlock){
  // trace T_EMON
  enum   states {initialize, post, resend};
  static states state = initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static File eMonPostLog;
  static double accum1Then [MAXINPUTS];
  static uint32_t UnixLastPost = UNIXtime();
  static uint32_t UnixNextPost = UNIXtime();
  static double _logHours;
  static String req = "";
  static uint32_t currentReqUnixtime = 0;
  static int  currentReqEntries = 0; 
  static uint32_t postTime = millis();
  struct SDbuffer {uint32_t data; SDbuffer(){data = 0;}};
  static SDbuffer* buf = new SDbuffer;
  String eMonPostLogFile = "/iotawatt/emonlog.log";
    
  trace(T_EMON,0);

            // If stop signaled, do so.  

  if(eMonStop) {
    msgLog("EmonService: stopped.");
    eMonStarted = false;
     trace(T_EMON,4);
    eMonPostLog.close();
     trace(T_EMON,5);
     SD.remove((char *)eMonPostLogFile.c_str());
     trace(T_EMON,6);
    return 0;
  }
      
  switch(state){

    case initialize: {

          // We post the log to EmonCMS,
          // so wait until the log service is up and running.
      
      if(!iotaLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("EmonService: started.");
     
      if(!eMonPostLog){
        eMonPostLog = SD.open(eMonPostLogFile,FILE_WRITE);
      }
            
      if(eMonPostLog){
        if(eMonPostLog.size() == 0){
          buf->data = iotaLog.lastKey();
          eMonPostLog.write((byte*)buf,4);
          eMonPostLog.flush();
          msgLog("EmonService: Emonlog file created.");
        }
        eMonPostLog.seek(eMonPostLog.size()-4);
        eMonPostLog.read((byte*)buf,4);
        logRecord->UNIXtime = buf->data;
        msgLog("EmonService: Start posting from ", String(logRecord->UNIXtime));
      }
      else {
        logRecord->UNIXtime = iotaLog.lastKey();
      }
      

          // Get the last record in the log.
          // Posting will begin with the next log entry after this one,
            
      iotaLog.readKey(logRecord);

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
      UnixNextPost = UnixLastPost + eMonCMSInterval - (UnixLastPost % eMonCMSInterval);

          // Advance state.
          // Set task priority low so that datalog will run before this.
      
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
        UnixNextPost = UNIXtime() + eMonCMSInterval - (UNIXtime() % eMonCMSInterval);
        return UnixNextPost;
      } 
      
          // Not current.  Read sequentially to get the entry >= scheduled post time
      trace(T_EMON,1);    
      while(logRecord->UNIXtime < UnixNextPost){
        if(logRecord->UNIXtime >= iotaLog.lastKey()){
          msgLog("runaway seq read.", logRecord->UNIXtime);
          ESP.reset();
        }
        iotaLog.readNext(logRecord);
      }

          // Adjust the posting time to match the log entry time.
            
      UnixNextPost = logRecord->UNIXtime - logRecord->UNIXtime % eMonCMSInterval;

          // Compute the time difference between log entries.
          // If zero, don't bother.
          
      double elapsedHours = logRecord->logHours - _logHours;
      if(elapsedHours == 0){
        UnixNextPost += eMonCMSInterval;
        return UnixNextPost;  
      }
      
          // If new request, format preamble, otherwise, just tack it on with a comma.
      
      if(req.length() == 0){
        req = eMonPiUri + "/input/bulk.json?time=" + String(UnixNextPost) + "&apikey=" + apiKey + "&data=[";
        currentReqUnixtime = UnixNextPost;
      }
      else {
        req += ',';
      }

          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.
     
      trace(T_EMON,2);

      req += '[' + String(UnixNextPost - currentReqUnixtime) + ",\"" + String(node) + "\",";
      double value1;
      
      _logHours = logRecord->logHours;   
      for (int i = 0; i < maxInputs; i++) {
        IotaInputChannel *_input = inputChannel[i];
        value1 = (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;
        accum1Then[i] = logRecord->channel[i].accum1;
        if( ! _input){
          req += "null,";
        }
        else if(_input->_type == channelTypeVoltage){
          req += String(value1,1) + ',';
        }
        else if(_input->_type == channelTypePower){
          req += String(long(value1+0.5)) + ',';
        }
        else{
          req += String(long(value1+0.5)) + ',';
        }
      }
      trace(T_EMON,3);    
      req.setCharAt(req.length()-1,']');
      currentReqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  eMonCMSInterval - (UnixNextPost % eMonCMSInterval);
      
      if ((currentReqEntries < eMonBulkSend) ||
         ((iotaLog.lastKey() > UnixNextPost) &&
         (req.length() < 1000))) {
        return UnixNextPost;
      }

          // Send the post       

      req += ']';
      uint32_t sendTime = millis();
      Serial.print(formatHMS(NTPtime() + (localTimeDiff * 3600)));
      Serial.print(" ");
      Serial.print(millis()-sendTime);
      Serial.print(" ");
      Serial.println(req);
      if(!eMonSend(req)){
        state = resend;
        return UNIXtime() + 30;
      }
      buf->data = UnixLastPost;
      eMonPostLog.write((byte*)buf,4);
      eMonPostLog.flush();
      req = "";
      currentReqEntries = 0;    
      state = post;
      return UnixNextPost;
    }


    case resend: {
      msgLog("Resending eMonCMS data.");
      if(!eMonSend(req)){ 
        return UNIXtime() + 60;
      }
      else {
        buf->data = UnixLastPost;
        eMonPostLog.write((byte*)buf,4);
        eMonPostLog.flush();
        req = "";
        currentReqEntries = 0;  
        state = post;
        return 1;
      }
      break;
    }
  }
  return 1;
}

/************************************************************************************************
 *  eMonSend - send data to the eMonCMS server. 
 *  if secure transmission is configured, pas sthe request to a 
 *  similar WiFiClientSecure function.
 *  Secure takes about twice as long and can block sampling for more than a second.
 ***********************************************************************************************/
boolean eMonSend(String req){
  
  if(eMonSecure) return eMonSendSecure(req);
  trace(T_EMON,7);
  
  uint32_t startTime = millis();
  if(!WifiClient.connect(eMonURL.c_str(), 80)) {
        msgLog("failed to connect to:", eMonURL);
        WifiClient.stop();
        return false;
  } 
  yield();  
  WifiClient.println(String("GET ") + req);
  uint32_t _time = millis();
  while(WifiClient.available() < 2){
    yield();
    if((uint32_t)millis()-_time >= 200){
      msgLog("eMonCMS timeout.");
      WifiClient.stop();
      return false;
    }
  }
  yield();
  String reply = "";
  int maxlen = 40;
  while(WifiClient.available()){
    reply += (char)WifiClient.read();
    if(!maxlen--){
      break;
    }
  }
  if(reply.substring(0,2) != "ok"){
    msgLog("eMonCMS reply: ", reply);
    WifiClient.stop();
    return false;
  }
  WifiClient.stop();
//  Serial.print("Open Send ms: ");
//  Serial.println(millis()-startTime);
  return true;
}

boolean eMonSendSecure(String req){
  trace(T_EMON,8);
  ESP.wdtFeed();

      // Should always be disconnected, but test can't hurt.
    
  uint32_t startTime = millis();
  if(!WifiClientSecure.connected()){
    if(!WifiClientSecure.connect(eMonURL.c_str(), HttpsPort)) {
          msgLog("failed to connect to:",  eMonURL);
          WifiClientSecure.stop();
          return false;
    }
    if(!WifiClientSecure.verify(eMonSHA1,  eMonURL.c_str())){
      msgLog("eMonCMS could not validate certificate.");
      WifiClientSecure.stop();
      return false;
    }
  }
  yield();
  
      // Send the packet
   
  WifiClientSecure.print(String("GET ") + req + " HTTP/1.1\r\n" +
               "Host: " + eMonURL + "\r\n" +
               "User-Agent: IotaWatt\r\n" +
               "Connection: close\r\n\r\n"); 
 
      // Read through response header until blank line (\r\n)

  yield();    
  while (WifiClientSecure.connected()) {
    String line = WifiClientSecure.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  yield(); 
  String line;
  while(WifiClientSecure.available()){
    line += char(WifiClientSecure.read());
  }
  if (!line.startsWith("ok")) {
    msgLog ("eMonCMS reply: ", line);
    WifiClientSecure.stop();
    return false;
  }              
  
//  Serial.print("Secure Send ms: ");
//  Serial.println(millis()-startTime);
  return true;
}



