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
  enum   states {initialize, post, resend};
  static states state = initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static double accum1Then [channels];
  static double accum2Then [channels];
  static uint32_t UnixLastPost = UnixTime();
  static uint32_t UnixNextPost = UnixTime();
  static double _logHours;
  static String req = "";  
  static mseconds_t postTime = millis();
      
  switch(state){

    case initialize: {

          // We post the log to EmonCMS,
          // so wait until the log service is up and running.
      
      if(!iotaLog.isOpen()){
        return ((uint32_t) NTPtime() + 5);
      }
      msgLog("EmonCMS service started.");

          // Get the last record in the log.
          // Posting will begin with the next log entry after this one,
          // so in the future if there is a way to remember the last
          // that was posted (RTC memory or maybe use the SPIFFS) then 
          // that record should be loaded here. Benchmarks indicate 
          // can upload about three per second, so maybe put some 
          // limits on the lookback.
          
      logRecord->UNIXtime = iotaLog.lastKey();
      iotaLog.readKey(logRecord);

          // Save the value*hrs to date, and logHours to date
      
      for(int i=0; i<channels; i++){ 
        accum1Then[i] = logRecord->channel[i].accum1;
        accum2Then[i] = logRecord->channel[i].accum2;
      }
      _logHours = logRecord->logHours;

          // Assume that record was posted (not important).
          // Plan to start posting one interval later
      
      UnixLastPost = logRecord->UNIXtime;
      UnixNextPost = UnixLastPost + eMonCMSInterval - (UnixLastPost % eMonCMSInterval);

          // Advance state.
          // Set task priority low so that datalog will run before this.
      
      state = post;
      _serviceBlock->priority = priorityLow;
      return ((uint32_t) UnixNextPost + SEVENTY_YEAR_SECONDS);
    }

        
    case post: {

          // If we are current,
          // Anticipate next posting at next regular interval and break to reschedule.
 
      if(iotaLog.lastKey() < UnixNextPost){
        
        UnixNextPost = UnixTime() + eMonCMSInterval - (UnixTime() % eMonCMSInterval);
        return ((uint32_t)UnixNextPost + SEVENTY_YEAR_SECONDS);
      } 
      
          // Not current.  Read sequentially to get the entry >= scheduled post time
      
      while(logRecord->UNIXtime < UnixNextPost){
        if(logRecord->UNIXtime >= iotaLog.lastKey()){
          msgLog("runaway seq read.", logRecord->UNIXtime);
          ESP.reset();
        }
        iotaLog.readNext(logRecord);
      }

          // Adjust the posting time to match the log entry time.
      
      UnixNextPost = logRecord->UNIXtime - logRecord->UNIXtime % eMonCMSInterval;

          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.

      req = "GET /input/post.json?time=" + String(UnixNextPost) + "&node=" + String(node) + "&csv=";    
      int commas = 0;
      double value1;
      double elapsedHours = logRecord->logHours - _logHours;
      _logHours = logRecord->logHours;
      for (int i = 0; i < channels; i++) {
        value1 = (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;
        accum1Then[i] = logRecord->channel[i].accum1;
        accum2Then[i] = logRecord->channel[i].accum2;
        if(channelType[i] != channelTypeUndefined) {
          while(commas > 0) {
            req += ",";
            commas--;
          }
          if(channelType[i] == channelTypeVoltage) req += String(value1,1);
          else if(channelType[i] == channelTypePower) req += String(long(value1+0.5));
          else req += String(long(value1+0.5));
        }
        commas++;
      }
      req += "&apikey=" + apiKey;

          // Send the post
          
      Serial.print(formatHMS(NTPtime() + (localTimeDiff * 3600)));
      Serial.print(" ");
      Serial.print(offset[0]);
      Serial.print(" ");
      Serial.println(req);
      
      if(!eMonSend(req)){
        state = resend;
        return ((uint32_t)NTPtime() + 1);
      }
            
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  eMonCMSInterval - (UnixNextPost % eMonCMSInterval);
      return 0;   
    }

    case resend: {
      msgLog("Resending eMonCMS data.");
      if(!eMonSend(req)){
        return ((uint32_t)NTPtime() + 5);
      }
      else {
        UnixLastPost = UnixNextPost;
        UnixNextPost +=  eMonCMSInterval - (UnixNextPost % eMonCMSInterval);
        state = post;
      }
      break;
    }
  }
  return 0;
}

/************************************************************************************************
 *  This synchronous HTTP GET transaction seems to take about 160-180ms from Boston to the UK.
 *  It takes about half that to ping the site, so we could be doing sampling while the bits are
 *  swimming across the ocean.
 *  There is a WiFiClient for the ESP that allows this to be done asynchronously. That would be
 *  something to explore in order to keep the AC sampling rates up.
 ***********************************************************************************************/

boolean eMonSend(String req){
  if(!WifiClient.connect(cloudURL.c_str(), 80)) {
        msgLog("failed to connect to:", cloudURL);
        WifiClient.stop();
        return false;
  }   
  WifiClient.println(req);
  uint32_t _time = millis();
  while(WifiClient.available() < 2){
    yield();
    if((uint32_t)millis()-_time >= 200){
      msgLog("eMonCMS timeout.");
      WifiClient.stop();
      return false;
    }
  }
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
  return true;
}



