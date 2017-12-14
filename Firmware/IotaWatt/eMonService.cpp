#include "IotaWatt.h"

boolean EmonSendData(uint32_t reqUnixtime, String reqData);
String bin2hex(const uint8_t* in, size_t len);
String encryptData(String in, const uint8_t* key);
boolean EmonSendData(uint32_t reqUnixtime, String reqData, size_t timeout, bool logError);

   /*******************************************************************************************************
 * EmonService - This SERVICE posts entries from the IotaLog to EmonCMS.  Details of the EmonCMS
 * account are provided in the configuration file at startup and this SERVICE is scheduled.  It runs
 * more or less independent of everything else, just reading the log records as they become available
 * and sending the data out.
 * The advantage of doing it this way is that there is really no EmonCMS specific code anywhere else
 * except a speciific section in getConfig.  Other web data logging services could be handled
 * the same way.
 * It's possible that multiple web services could be updated independently, each having their own
 * SERVER.  The only issue right now would be the WiFi resource.  A future move to the
 * asynchWifiClient would solve that.
 ******************************************************************************************************/
uint32_t EmonService(struct serviceBlock* _serviceBlock){
  // trace T_Emon
  enum   states {initialize, getPostTime, post, resend};
  static states state = initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static File EmonPostLog;
  static double accum1Then [MAXINPUTS];
  static uint32_t UnixLastPost = 0;
  static uint32_t UnixNextPost = 0;
  static double _logHours;
  static double elapsedHours;
  static uint32_t resendCount;
  static String reqData = "";
  static uint32_t reqUnixtime = 0;
  static int  reqEntries = 0;
  static uint32_t postTime = millis();
  struct SDbuffer {uint32_t data; SDbuffer(){data = 0;}};
  static SDbuffer* buf = new SDbuffer;

  trace(T_Emon,0);

            // If stop signaled, do so.

  if(EmonStop) {
    msgLog("EmonService: stopped.");
    EmonStarted = false;
    trace(T_Emon,1);
    EmonPostLog.close();
    trace(T_Emon,2);
    state = initialize;
    return 0;
  }
  if(EmonInitialize){
    state = initialize;
    EmonInitialize = false;
  }

  switch(state){

    case initialize: {

          // We post the log to EmonCMS,
          // so wait until the log service is up and running.

      if(!currLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("EmonService: started.",
       "url: " + EmonURL + ":" + String(EmonPort) + EmonURI + ", node: " + String(node) + ", post interval: " +
       String(EmonCMSInterval) + (EmonSend == EmonSendGET ? ", unsecure GET" : ", encrypted POST"));

      state = getPostTime;
    }

    case getPostTime: {

      String URL = EmonURI + "/input/get?node=" + String(node);
      http.begin(EmonURL, EmonPort, URL);
      String auth = "Bearer " + apiKey;
      http.addHeader("Authorization", auth.c_str());
      http.setTimeout(500);
      int httpCode = http.GET();
      if(httpCode != HTTP_CODE_OK){
        msgLog("EmonService: input/get failed.");
        return UNIXtime() + 30;
      }
      String response = http.getString();
      if (response.startsWith("\"Node does not exist\"")){
        UnixLastPost = UNIXtime();
        UnixLastPost -= UnixLastPost % EmonCMSInterval;
      }
      else {
        int pos = 0;
        while((pos = response.indexOf("\"time\":", pos)) > 0) {
          pos += 7;
          uint32_t _time = (uint32_t)response.substring(pos, response.indexOf(',',pos)).toInt();
          UnixLastPost = MAX(UnixLastPost, _time);
        }
        if(UnixLastPost == 0 || UnixLastPost > currLog.lastKey()) {
          UnixLastPost = currLog.lastKey();
        }
        if(UnixLastPost < currLog.firstKey()){
          UnixLastPost = currLog.firstKey();
        }
      }
      msgLog("EmonService: Start posting at ", UnixLastPost);

            // Get the last record in the log.
            // Posting will begin with the next log entry after this one,

      logRecord->UNIXtime = UnixLastPost;
      currLog.readKey(logRecord);

            // Save the value*hrs to date, and logHours to date

      for(int i=0; i<maxInputs; i++){
        accum1Then[i] = logRecord->channel[i].accum1;
        if(accum1Then[i] != accum1Then[i]) accum1Then[i] = 0;
      }
      _logHours = logRecord->logHours;
      if(_logHours != _logHours /*NaN*/) _logHours = 0;

            // Assume that record was posted (not important).
            // Plan to start posting one interval later

      UnixLastPost = logRecord->UNIXtime;
      UnixNextPost = UnixLastPost + EmonCMSInterval - (UnixLastPost % EmonCMSInterval);

            // Advance state.
            // Set task priority low so that datalog will run before this.

      reqData = "";
      reqEntries = 0;
      state = post;
      _serviceBlock->priority = priorityLow;
      return UnixNextPost;
    }

    case post: {
      trace(T_Emon,4);

          // If WiFi is not connected,
          // just return without attempting to log and try again in a few seconds.

      if(WiFi.status() != WL_CONNECTED) {
        return 2;
      }

          // If we are current,
          // Anticipate next posting at next regular interval and break to reschedule.

      if(currLog.lastKey() < UnixNextPost){
        UnixNextPost = UNIXtime() + EmonCMSInterval - (UNIXtime() % EmonCMSInterval);
        return UnixNextPost;
      }

          // Not current.  Read the next log record.

      trace(T_Emon,1);
      logRecord->UNIXtime = UnixNextPost;
      logReadKey(logRecord);

          // Compute the time difference between log entries.
          // If zero, don't bother.

      elapsedHours = logRecord->logHours - _logHours;
      if(elapsedHours == 0){
        UnixNextPost += EmonCMSInterval;
        return UnixNextPost;
      }

          // If new request, format preamble, otherwise, just tack it on with a comma.

      if(reqData.length() == 0){
        reqUnixtime = UnixNextPost;
        reqData = "time=" + String(reqUnixtime) +  "&data=[";
      }
      else {
        reqData += ',';
      }

          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.

      trace(T_Emon,5);
      reqData += '[' + String(UnixNextPost - reqUnixtime) + ",\"" + String(node) + "\",";

      double value1;
      _logHours = logRecord->logHours;
      if( ! emonOutputs){
        for (int i = 0; i < maxInputs; i++) {
          IotaInputChannel *_input = inputChannel[i];
          value1 = (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;
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
      }
      else {
        Script* script = emonOutputs->first();
        int index=1;
        while(script){
         //Serial.print(script->name());
          //Serial.print(" ");
          while(index++ < String(script->name()).toInt()) reqData += "null,";
          value1 = script->run([](int i)->double {return (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;});
          if(value1 > -1.0 && value1 < 1){
            reqData += "0,";
          }
          else {
            reqData += String(value1,1) + ',';
          }
          script = script->next();
         // Serial.println(reqData);
        }
      }
      for (int i = 0; i < maxInputs; i++) {
        accum1Then[i] = logRecord->channel[i].accum1;
      }
      trace(T_Emon,6);
      reqData.setCharAt(reqData.length()-1,']');
      reqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  EmonCMSInterval - (UnixNextPost % EmonCMSInterval);

      if ((reqEntries < EmonBulkSend) ||
         ((currLog.lastKey() > UnixNextPost) &&
         (reqData.length() < 1000))) {
        return UnixNextPost;
      }

          // Send the post

      reqData += ']';
      if(!EmonSendData(reqUnixtime, reqData, 500, false)){
        state = resend;
        resendCount = 0;
        return UNIXtime() + 5;
      }
      buf->data = UnixLastPost;
      reqData = "";
      reqEntries = 0;
      state = post;
      return UnixNextPost;
    }


    case resend: {
      trace(T_Emon,7);
      resendCount++;
      if(resendCount > 1){
        msgLog("EmonService: Resending EmonCMS data:", resendCount);
      }
      if(!EmonSendData(reqUnixtime, reqData, 1500, resendCount == 1)){
        if(resendCount < 10){
          return UNIXtime() + 60 * resendCount;
        }
        msgLog(F("EmonService: Unable to post to Emoncms after 10 retries.  Restarting ESP."));
        ESP.restart();
      }
      else {
        if(resendCount > 1){
          msgLog(F("EmonService: Retry successful."));
        }
        buf->data = UnixLastPost;
        EmonPostLog.write((byte*)buf,4);
        EmonPostLog.flush();
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
 *  EmonSend - send data to the EmonCMS server.
 *  if secure transmission is configured, pas sthe request to a
 *  similar WiFiClientSecure function.
 *  Secure takes about twice as long and can block sampling for more than a second.
 ***********************************************************************************************/
boolean EmonSendData(uint32_t reqUnixtime, String reqData, size_t timeout, bool logError){
  trace(T_Emon,8);
  uint32_t startTime = millis();
  // Serial.println(reqData);

  if(EmonSend == EmonSendGET){
    String URL = EmonURI + "/input/bulk.json?" + reqData + "&apikey=" + apiKey;
    http.begin(EmonURL, EmonPort, URL);
    http.setTimeout(timeout);
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      if(logError){
        msgLog("EmonService: GET failed. HTTP code:", http.errorToString(httpCode) );
      }
      else {
        Serial.print("EmonService: GET failed. HTTP code: ");
        Serial.println(http.errorToString(httpCode));
      }
      http.end();
      return false;
    }
    String response = http.getString();
    http.end();
    if(response.startsWith("ok")){
      return true;
    }
    Serial.println("response not ok.");
    return false;
  }

  if(EmonSend == EmonSendPOSTsecure){
    String URI = EmonURI + "/input/bulk";
    sha256.reset();
    sha256.update(reqData.c_str(), reqData.length());
    uint8_t value[32];
    sha256.finalize(value, 32);
    String base64Sha = base64encode(value, 32);
    sha256.resetHMAC(cryptoKey,16);
    sha256.update(reqData.c_str(), reqData.length());
    sha256.finalizeHMAC(cryptoKey, 16, value, 32);
    String auth = EmonUsername + ':' + bin2hex(value, 32);
    http.begin(EmonURL, EmonPort, URI);
    http.addHeader("Host",EmonURL);
    http.addHeader("Content-Type","aes128cbc");
    http.addHeader("Authorization", auth.c_str());
    http.setTimeout(timeout);
    int httpCode = http.POST(encryptData(reqData, cryptoKey));
    trace(T_Emon,9);
    int responseLength = http.getSize();
    String response = "";
    if(responseLength > 0 && responseLength <= 1000){
      response = http.getString();
    }
    http.end();
    if(httpCode != HTTP_CODE_OK){
      String code = String(httpCode);
      if(httpCode < 0){
        code = http.errorToString(httpCode);
      }
      if(logError){
        msgLog("EmonService: POST failed. HTTP code:", code);
      }
      else {
        Serial.print("EmonService: POST failed. HTTP code: ");
        Serial.println(code);
        Serial.println(response);
      }
      return false;
    }

    if(response.startsWith(base64Sha)){
      return true;
    }
    msgLog(reqData.substring(0,60));
    msgLog("EmonService: Invalid response: ", response.substring(0,60));
    msgLog("EmonService: Expectd response: ", base64Sha);

    return false;
  }

  msgLog("EmonService: Unsupported protocol - ", EmonSend);
  return false;
}

String encryptData(String in, const uint8_t* key) {
  trace(T_encryptEncode, 0);
  uint8_t iv[16];
  os_get_random((unsigned char*)iv,16);
  uint8_t padLen = 16 - (in.length() % 16);
  int encryptLen = in.length() + padLen;
  uint8_t* ivBuf = new uint8_t[encryptLen + 16];
  uint8_t* encryptBuf = ivBuf + 16;
  trace(T_encryptEncode, 2);
  for(int i=0; i<16; i++){
    ivBuf[i] = iv[i];
  }
  for(int i=0; i<in.length(); i++){
    encryptBuf[i] = in[i];
  }
  for(int i=0; i<padLen; i++){
    encryptBuf[in.length()+i] = padLen;
  }
  trace(T_encryptEncode, 3);
  cypher.setIV(iv, 16);
  cypher.setKey(cryptoKey, 16);
  trace(T_encryptEncode, 4);
  cypher.encrypt(encryptBuf, encryptBuf, encryptLen);
  trace(T_encryptEncode, 5);
  String result = base64encode(ivBuf, encryptLen+16);
  trace(T_encryptEncode, 6);
  delete[] ivBuf;
  trace(T_encryptEncode, 7);
  return result;
}

String base64encode(const uint8_t* in, size_t len){
  static const char* base64codes = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  int base64Len = int((len+2)/3) * 4;
  int wholeSextets = int((len*8)/6);
  char* base64 = new char[base64Len + 1];
  char* base64out = base64;
  int sextetSeq = 0;
  uint8_t sextet;
  trace(T_encryptEncode, 8);
  for(int i=0; i<wholeSextets; i++){
    if(sextetSeq == 0) sextet = *in >> 2;
    else if(sextetSeq == 1) sextet = (*in++ << 4) | (*in >> 4);
    else if(sextetSeq == 2) sextet = (*in++ << 2) | (*in >> 6);
    else sextet = *in++;
    *(base64out++) = base64codes[sextet & 0x3f];
    sextetSeq = ++sextetSeq % 4;
  }
  if(sextetSeq == 1){
    *(base64out++) = base64codes[(*in << 4) & 0x3f];
    *(base64out++) = '=';
    *(base64out++) = '=';
  }
  else if(sextetSeq == 2){
    *(base64out++) = base64codes[(*in << 2) & 0x3f];
    *(base64out++) = '=';
  }
  else if(sextetSeq == 3){
    *(base64out++) = base64codes[*in & 0x3f];
  }
  *base64out = 0;
  if((base64out - base64) != base64Len){
    Serial.print("Base 64 output length error:");
    Serial.print(base64out - base64);
    Serial.print(" ");
    Serial.println(base64Len);
    dropDead();
  }
  trace(T_encryptEncode, 9);
  String out = base64;
  delete[] base64;
  return out;
}

String bin2hex(const uint8_t* in, size_t len){
  static const char* hexcodes = "0123456789abcdef";
  String out = "";
  for(int i=0; i<len; i++){
    out += hexcodes[*in >> 4];
    out += hexcodes[*in++ & 0x0f];
  }
  return out;
}
