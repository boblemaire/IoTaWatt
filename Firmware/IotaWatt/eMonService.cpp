#include "IotaWatt.h"
      
bool      EmonStarted = false;                    // set true when Service started
bool      EmonStop = false;                       // set true to stop the Service
bool      EmonRestart = true;                  // Initialize or reinitialize EmonService                                         
String    EmonURL;                                // These are set from the config file 
uint16_t  EmonPort = 80;
String    EmonURI = "";
String    apiKey;
uint8_t   cryptoKey[16];
String    node = "IotaWatt";
boolean   EmonSecure = false;
String    EmonUsername;
int16_t   EmonBulkSend = 1;
int32_t   EmonRevision = -1;
EmonSendMode EmonSend = EmonSendPOSTsecure;
ScriptSet* emonOutputs;

   /*******************************************************************************************************
 * EmonService - This SERVICE posts entries from the IotaLog to EmonCMS.  Details of the EmonCMS
 * account are provided in the configuration file at startup and this SERVICE is scheduled.  It runs
 * more or less independent of everything else, just reading the log records as they become available
 * and sending the data out.
 * The advantage of doing it this way is that there is really no EmonCMS specific code anywhere else
 * except a complimentary section in getConfig.  Other web data logging services should be handled
 * the same way.
 * Now that the HTTP posts are asynchronous, It's possible that multiple web services could be
 * updated independently, each having their own independantly scheduled Server instance.
 ******************************************************************************************************/
uint32_t EmonService(struct serviceBlock* _serviceBlock){
  // trace T_Emon
  enum   states {initialize, getPostTime, waitPostTime, sendPost, waitPost, sendSecure, waitSecure, post, resend};
  static states state = initialize;
  static IotaLogRecord* lastRecord = nullptr;
  static IotaLogRecord* logRecord = nullptr;
  static uint32_t UnixLastPost = 0;
  static uint32_t UnixNextPost = 0;
  static double elapsedHours;
  static xbuf reqData;
  static uint32_t reqUnixtime = 0;
  static String* reqStr = nullptr;
  static int  reqEntries = 0; 
  static uint32_t postTime = millis();
  static asyncHTTPrequest* request = nullptr;
  static String base64Sha;
  static int32_t retryCount = 0;
          
  trace(T_Emon,0);

            // If stop signaled, do so.  


  if((EmonRestart || EmonStop) && state != waitPost && state != waitSecure) {
    trace(T_Emon,1);
    delete lastRecord;
    lastRecord = nullptr;
    delete logRecord;
    logRecord = nullptr;
    delete request;
    request = nullptr;
    delete reqStr;
    reqStr = nullptr;
    state = initialize;
    if(EmonStop) {
      EmonStop = false;
      EmonStarted = false;
      msgLog("EmonService: stopped.");
      return 0;
    }
    EmonRestart = false;
    return 1;
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
      EmonStarted = true;
      state = getPostTime; 
    }

    case getPostTime: {
      trace(T_Emon,2);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/get?node=" + String(node);
      request->setTimeout(1);
      request->setDebug(false);
      request->open("GET", URL.c_str());
      String auth = "Bearer " + apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      trace(T_Emon,2);
      request->send();
      state = waitPostTime;
      return 1;
    } 

    case waitPostTime: {
      trace(T_Emon,3);
      if(request->readyState() != 4){
        return UNIXtime() + 1; 
      }

      trace(T_Emon,3);
      if(request->responseHTTPcode() != 200){
        msgLog("EmonService: get input list failed, code: ", request->responseHTTPcode());
        state = getPostTime;
        return UNIXtime() + 5;
      }

      trace(T_Emon,3);    
      String response = request->responseText();
      delete request;
      request = nullptr;
      if (response.startsWith("\"Node does not exist\"")){
        msgLog(F("EmonService: Node doesn't yet exist, starting posting now."));
        UnixLastPost = UNIXtime();
        UnixLastPost -= UnixLastPost % EmonCMSInterval;
      }
      else {
        trace(T_Emon,3);
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

      if( ! lastRecord){
        lastRecord = new IotaLogRecord;
      }
      trace(T_Emon,3);
      lastRecord->UNIXtime = UnixLastPost;      
      currLog.readKey(lastRecord);

            // Save the value*hrs to date, and logHours to date

      trace(T_Emon,3);  

            // Assume that record was posted (not important).
            // Plan to start posting one interval later
        
      UnixLastPost = lastRecord->UNIXtime;
      UnixNextPost = UnixLastPost + EmonCMSInterval - (UnixLastPost % EmonCMSInterval);
        
            // Advance state.
            // Set task priority low so that datalog will run before this.

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
        return UNIXtime() + 1;
      }

          // If we are current,
          // Anticipate next posting at next regular interval and break to reschedule.
 
      if(currLog.lastKey() < UnixNextPost){ 
        UnixNextPost = UNIXtime() + EmonCMSInterval - (UNIXtime() % EmonCMSInterval);
        return UnixNextPost;
      } 
      
          // Not current.  Read the next log record.
          
      trace(T_Emon,4);
      if( ! logRecord){
        logRecord = new IotaLogRecord;
      }
      logRecord->UNIXtime = UnixNextPost;
      logReadKey(logRecord);    
     
          // Compute the time difference between log entries.
          // If zero, don't bother.
          
      elapsedHours = logRecord->logHours - lastRecord->logHours;
      if(elapsedHours == 0 || elapsedHours != elapsedHours){
        UnixNextPost += EmonCMSInterval;
        return UnixNextPost;  
      }
      
          // If new request, format preamble, otherwise, just tack it on with a comma.
      
      trace(T_Emon,4);
      if(reqData.available() == 0){
        reqUnixtime = UnixNextPost;
        reqData.printf_P(PSTR("time=%d&data=["),reqUnixtime);
      }
      else {
        reqData.write(',');
      }
      
          // Build the request string.
          // values for each channel are (delta value hrs)/(delta log hours) = period value.
          // Update the previous (Then) buckets to the most recent values.
     
      trace(T_Emon,4);
      reqData.printf_P(PSTR("[%d,\"%s\""), UnixNextPost - reqUnixtime, node.c_str());
            
      double value1;
      if( ! emonOutputs){  
        for (int i = 0; i < maxInputs; i++) {
          IotaInputChannel *_input = inputChannel[i];
          value1 = (logRecord->channel[i].accum1 - lastRecord->channel[i].accum1) / elapsedHours;
          if( ! _input){
            reqData.write(",null");
          }
          else if(_input->_type == channelTypeVoltage){
            reqData.printf(",%.1f", value1);
          }
          else if(_input->_type == channelTypePower){
            reqData.printf(",%.0f", value1);
          }
          else{
            reqData.printf(",%.0f", value1);
          }
        }
      }
      else {
        trace(T_Emon,5);
        Script* script = emonOutputs->first();
        int index=1;
        while(script){
          while(index++ < String(script->name()).toInt()) reqData.write(",null");
          value1 = script->run([](int i)->double {return (logRecord->channel[i].accum1 - lastRecord->channel[i].accum1) / elapsedHours;});
          if(value1 > -1.0 && value1 < 1){
            reqData.write(",0");
          }
          else {
            reqData.printf(",%.1f", value1);
          }
          script = script->next();
        }
      }
      trace(T_Emon,6);
      reqData.write(']');
      reqEntries++;
      delete lastRecord;
      lastRecord = logRecord;
      logRecord = nullptr;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  EmonCMSInterval - (UnixNextPost % EmonCMSInterval);
      
      if ((reqEntries < EmonBulkSend) ||
         ((currLog.lastKey() > UnixNextPost) &&
         (reqData.available() < 2000))) {
        return UnixNextPost;
      }

          // Send the post       

      reqData.write(']');
      if(EmonSend == EmonSendGET){
        state = sendPost;
      }
      else {
        if( ! reqStr){
          reqStr = new String;
        }
        *reqStr = reqData.readString(reqData.available());
        state = sendSecure;
      }
      return 1;
    }

    case sendPost:{
      trace(T_Emon,6);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(false);
      if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.println(msg);
      }
      request->open("POST", URL.c_str());
      trace(T_Emon,6);
      String auth = "Bearer " + apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_Emon,6);
      request->send(&reqData, reqData.available());
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_Emon,7);
      if(request->readyState() != 4){
        return 1; 
      }
      trace(T_Emon,7);
      if(request->responseHTTPcode() != 200){
        if(++retryCount  % 10 == 0){
            msgLog("EmonService: retry count ", retryCount);
        }
        state = sendPost;
        return UNIXtime() + 1;
      }
      trace(T_Emon,7);
      String response = request->responseText();
      if(! response.startsWith("ok")){
        msgLog("EmonService: response not ok. Retrying.");
        state = sendPost;
        return UNIXtime() + 1;
      }
      trace(T_Emon,7);
      delete request;
      request = nullptr;
      retryCount = 0;
      reqData.flush();
      reqEntries = 0;    
      state = post;
      return UnixNextPost;
    }

case sendSecure:{
      trace(T_Emon,8);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! request) {
        request = new asyncHTTPrequest;
      }
      trace(T_Emon,8);
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(true);
      trace(T_Emon,8);
      SHA256* sha256 = new SHA256;
      sha256->reset();
      sha256->update(reqStr->c_str(), reqStr->length());
      uint8_t value[32];
      sha256->finalize(value, 32);
      trace(T_Emon,8);
      base64Sha = base64encode(value, 32);
      trace(T_Emon,8);
      sha256->resetHMAC(cryptoKey,16);
      sha256->update(reqStr->c_str(), reqStr->length());
      sha256->finalizeHMAC(cryptoKey, 16, value, 32);
      delete sha256;
      trace(T_Emon,8);
      String auth = EmonUsername + ':' + bin2hex(value, 32);
      //if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        //String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.printf_P(PSTR("time %02d:%02d:%02d, length %d\r\n"), now.hour(),now.minute(),now.second(), reqStr->length());
      //}
      request->open("POST", URL.c_str());
      trace(T_Emon,8);
      request->setReqHeader("Content-Type","aes128cbc");
      request->setReqHeader("Authorization", auth.c_str());
      trace(T_Emon,8);
      request->send(encryptData(*reqStr, cryptoKey));
      state = waitSecure;
      return 1;
    } 

    case waitSecure: {
      trace(T_Emon,9);
      if(request->readyState() != 4){
        return 1; 
      }
      trace(T_Emon,9);
      if(request->responseHTTPcode() != 200){
        if(++retryCount  % 10 == 0){
            msgLog("EmonService: retry count ", retryCount);
        }
        state = sendSecure;
        return UNIXtime() + 1;
      }
      trace(T_Emon,9);
      String response = request->responseText();
      if(! response.startsWith(base64Sha)){
        msgLog("EmonService: Invalid response, Retrying.");
        Serial.println(base64Sha);
        Serial.println(response);
        state = sendSecure;
        return UNIXtime() + 1;
      }
      trace(T_Emon,9);
      delete request;
      request = nullptr;
      retryCount = 0;
      delete reqStr;
      reqStr = nullptr;
      reqData.flush();
      reqEntries = 0;    
      state = post;
      return UnixNextPost;
    }
  }
  return 1;   
}

String encryptData(String in, const uint8_t* key) {
  CBC<AES128> cypher;
  trace(T_encryptEncode, 0);
  uint8_t iv[16];
  os_get_random((unsigned char*)iv,16);
  uint8_t padLen = 16 - (in.length() % 16);
  int encryptLen = in.length() + padLen;
  uint8_t* ivBuf = new uint8_t[encryptLen + 16];
  if( ! ivBuf){
    Serial.printf("no ivBuf, length %\r\n", encryptLen+16);
  }
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
  trace(T_encryptEncode, 3); 
  cypher.encrypt(encryptBuf, encryptBuf, encryptLen);
  trace(T_encryptEncode, 3);
  String result = base64encode(ivBuf, encryptLen+16);
  trace(T_encryptEncode, 3);
  delete[] ivBuf;
  trace(T_encryptEncode, 3);
  return result;
}

String base64encode(const uint8_t* in, size_t len){
  static const char* base64codes = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  int base64Len = int((len+2)/3) * 4;
  int wholeSextets = int((len*8)/6);
  char* base64 = new char[base64Len + 1];
  if( ! base64){
    Serial.printf("no base64, length %d\r\n", base64Len + 1);
  }
  char* base64out = base64;
  int sextetSeq = 0;
  uint8_t sextet;
  trace(T_encryptEncode, 4);
  for(int i=0; i<wholeSextets; i++){
    if(sextetSeq == 0) sextet = *in >> 2;
    else if(sextetSeq == 1) sextet = (*in++ << 4) | (*in >> 4);
    else if(sextetSeq == 2) sextet = (*in++ << 2) | (*in >> 6);
    else sextet = *in++;
    *(base64out++) = base64codes[sextet & 0x3f];
    sextetSeq = ++sextetSeq % 4;
  }
  trace(T_encryptEncode, 4);
  if(sextetSeq == 1){
    trace(T_encryptEncode, 5);
    *(base64out++) = base64codes[(*in << 4) & 0x3f];
    *(base64out++) = '=';
    *(base64out++) = '=';
  }
  else if(sextetSeq == 2){
    trace(T_encryptEncode, 6);
    *(base64out++) = base64codes[(*in << 2) & 0x3f];
    *(base64out++) = '=';
  }
  else if(sextetSeq == 3){
    trace(T_encryptEncode, 7);
    *(base64out++) = base64codes[*in & 0x3f];
  }
  *base64out = 0;
  
  trace(T_encryptEncode, 8);
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

bool EmonConfig(JsonObject& config){
  if(config["type"].as<String>() == "none"){
    EmonStop = true;
    return true;
  }
  if(config["type"].as<String>() != "emoncms"){
    EmonStop = true;
    return false;
  }
  int revision = config["revision"];
  if(revision == EmonRevision){
    return true;
  }
  EmonRevision = revision;
  EmonRestart = true;
  EmonURL = config["url"].as<String>();
  if(EmonURL.startsWith("http://")) EmonURL = EmonURL.substring(7);
  else if(EmonURL.startsWith("https://")){
    EmonURL = EmonURL.substring(8);
  }
  EmonURI = "";
  if(EmonURL.indexOf("/") > 0){
    EmonURI = EmonURL.substring(EmonURL.indexOf("/"));
    EmonURL.remove(EmonURL.indexOf("/"));
  }
  if(EmonURL.indexOf(":") > 0){
    EmonPort = EmonURL.substring(EmonURL.indexOf(":")+1).toInt();
    EmonURL.remove(EmonURL.indexOf(":"));
  }
  apiKey = config["apikey"].as<String>();
  node = config["node"].as<String>();
  EmonCMSInterval = config["postInterval"].as<int>();
  EmonBulkSend = config["bulksend"].as<int>();
  if(EmonBulkSend > 10) EmonBulkSend = 10;
  if(EmonBulkSend <1) EmonBulkSend = 1;
  EmonUsername = config["userid"].as<String>();
  EmonSend = EmonSendGET;
  if(EmonUsername != "")EmonSend = EmonSendPOSTsecure;
  
  #define hex2bin(x) (x<='9' ? (x - '0') : (x - 'a') + 10)
  apiKey.toLowerCase();
  for(int i=0; i<16; i++){
    cryptoKey[i] = hex2bin(apiKey[i*2]) * 16 + hex2bin(apiKey[i*2+1]); 
  }
  delete emonOutputs;
  JsonVariant var = config["outputs"];
  if(var.success()){
    emonOutputs = new ScriptSet(var.as<JsonArray>());
    Script* script = emonOutputs->first();
    int index = 0;
    while(script){
      if(String(script->name()).toInt() <= index){
        delete emonOutputs;
        break;
      }
      else {
        index = String(script->name()).toInt();
      }
      script = script->next();
    }
  }
  
  if( ! EmonStarted) {
    NewService(EmonService);
    EmonStarted = true;
    EmonStop = false;
  }
  return true; 
}
