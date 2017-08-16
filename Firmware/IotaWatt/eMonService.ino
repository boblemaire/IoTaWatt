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
  static String reqData = "";
  static uint32_t reqUnixtime = 0;
  static int  reqEntries = 0; 
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
    state = initialize;
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
      
      if(reqData.length() == 0){
        reqData = "[";
        reqUnixtime = UnixNextPost;
      }
      else {
        reqData += ',';
      }

          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.
     
      trace(T_EMON,2);

      if(EmonSend == EmonSendPOSTsecure){
        reqData += '[' + String(UnixNextPost);
      }
      else {
        reqData += '[' + String(UnixNextPost - reqUnixtime);
      }
      reqData +=  ",\"" + String(node) + "\",";
      
      double value1;
      
      _logHours = logRecord->logHours;   
      for (int i = 0; i < maxInputs; i++) {
        IotaInputChannel *_input = inputChannel[i];
        value1 = (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;
        accum1Then[i] = logRecord->channel[i].accum1;
        if( ! _input){
          reqData += "null,";
        }
        else if(_input->_type == channelTypeVoltage){
          reqData += String(value1,1) + ',';
        }
        else if(_input->_type == channelTypePower){
          reqData += String(long(value1+0.5)) + ',';
        }
        else{
          reqData += String(long(value1+0.5)) + ',';
        }
      }
      trace(T_EMON,3);    
      reqData.setCharAt(reqData.length()-1,']');
      reqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  eMonCMSInterval - (UnixNextPost % eMonCMSInterval);
      
      if ((reqEntries < eMonBulkSend) ||
         ((iotaLog.lastKey() > UnixNextPost) &&
         (reqData.length() < 1000))) {
        return UnixNextPost;
      }

          // Send the post       

      reqData += ']';
      if(!eMonSend(reqUnixtime, reqData)){
        state = resend;
        return UNIXtime() + 30;
      }
      buf->data = UnixLastPost;
      eMonPostLog.write((byte*)buf,4);
      eMonPostLog.flush();
      reqData = "";
      reqEntries = 0;    
      state = post;
      return UnixNextPost;
    }
  

    case resend: {
      msgLog("Resending eMonCMS data.");
      if(!eMonSend(reqUnixtime,reqData)){ 
        return UNIXtime() + 60;
      }
      else {
        buf->data = UnixLastPost;
        eMonPostLog.write((byte*)buf,4);
        eMonPostLog.flush();
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
 *  eMonSend - send data to the eMonCMS server. 
 *  if secure transmission is configured, pas sthe request to a 
 *  similar WiFiClientSecure function.
 *  Secure takes about twice as long and can block sampling for more than a second.
 ***********************************************************************************************/
boolean eMonSend(uint32_t reqUnixtime, String reqData){ 
  trace(T_EMON,7);
  uint32_t startTime = millis();
  
  if(EmonSend == EmonSendGET){
    String URL = EmonURI + "/input/bulk.json?time=" + String(reqUnixtime) + "&apikey=" + apiKey + "&data=" + reqData;
    Serial.println(URL);
    http.begin(EmonURL, 80, URL);
    http.setTimeout(100);
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      msgLog("EmonService: GET failed. HTTP code: ", String(httpCode));
      http.end();
      return false;
    }
    String response = http.getString();
    http.end();
    if(response.startsWith("ok")){
      return true;        
    }
    Serial.println("response not ok.");
    Serial.println(response);
    return false;
  }

  if(EmonSend == EmonSendPOSTsecure){
    String postData = "username=" + EmonUsername;               
                      
    if(EmonURL.equals("iotawatt.com")){
      postData += "&apikey=" + apiKey;
    }
    postData += "&data=" + encryptData(reqData, cryptoKey);
    Serial.println(EmonURL);
    Serial.println(EmonURI);
    Serial.println(postData);
    sha256.reset();
    sha256.update(reqData.c_str(), reqData.length());
    uint8_t value[32];
    sha256.finalize(value, 32);
    String base64Sha = base64encode(value, 32);
    http.begin(EmonURL, 80, EmonURI);
    http.addHeader("Host",EmonURL);
    http.addHeader("Content-Type","application/x-www-form-urlencoded");
    http.setTimeout(400);
    int httpCode = http.POST(postData);
    String response = http.getString();
    http.end();
    if(httpCode != HTTP_CODE_OK){
      msgLog("EmonService: POST failed. HTTP code: ", String(httpCode));
      Serial.println(response);
      return false;
    }
    if(response.startsWith(base64Sha)){
      Serial.println("EmonService: Response SHA valid.");
      return true;        
    }
    msgLog("EmonService: Invalid response: ", response.substring(0,40));
    return false;
  }
  
  msgLog("EmonService: Unsupported protocol - ", EmonSend);
  return false;
}

String encryptData(String in, uint8_t* key) {
  uint8_t iv[16];
  os_get_random((unsigned char*)iv,16);
  uint8_t padLen = 16 - (in.length() % 16);
  int encryptLen = in.length() + padLen;
  uint8_t* ivBuf = new uint8_t[encryptLen + 16];
  uint8_t* encryptBuf = ivBuf + 16;
  for(int i=0; i<16; i++){
    ivBuf[i] = iv[i];
  }  
  for(int i=0; i<encryptLen; i++){
    encryptBuf[i] = (i<in.length()) ? in[i] : padLen;
  }
  cypher.setIV(iv, 16);
  cypher.setKey(cryptoKey, 16); 
  cypher.encrypt(encryptBuf, encryptBuf, encryptLen);
  String result = base64encode(ivBuf, encryptLen+16);
  delete[] ivBuf;
  return result;
}

String base64encode(const uint8_t* in, size_t len){
  static const char* base64codes = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  int base64Len = int((len+2)/3) * 4;
  int wholeSextets = int((len*8)/6);
  String out = "";
  int sextetSeq = 0;
  uint8_t sextet;
  for(int i=0; i<wholeSextets; i++){
    if(sextetSeq == 0) sextet = *in >> 2;
    else if(sextetSeq == 1) sextet = (*in++ << 4) | (*in >> 4);
    else if(sextetSeq == 2) sextet = (*in++ << 2) | (*in >> 6);
    else sextet = *in++;
    out += base64codes[sextet & 0x3f];
    sextetSeq = ++sextetSeq % 4;
  }
  if(sextetSeq == 1){
    out += base64codes[(*in << 4) & 0x3f];
    out += '=';
    out += '=';
  }
  else if(sextetSeq == 2){
    out += base64codes[(*in << 2) & 0x3f];
    out += '=';
  }
  else if(sextetSeq == 3){
    out += base64codes[*in & 0x3f];
  }
  return out;
}

