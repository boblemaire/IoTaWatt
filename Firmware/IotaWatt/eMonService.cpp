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
 * except a shout out in getConfig to process the Emoncms config Json. Other web data logging services 
 * are handled the same way.
 * Now that the HTTP posts are asynchronous,  multiple web services can be run simultaneously with
 * minimal impact on overall sampling and operation.
 * This service uses the new xbuf extensively. xbuf is like a circular buffer, but is implimented 
 * using linked segments in heap, so that rather than wrapping the data, new segments are added
 * as data is written, and old segments are deleted as data is read.  This works to advantage handling
 * large buffers because the size is dynamically adjusted as data is sent, received, and copied
 * from one xbuf to another or, as in the case here, read and written  to the same buffer to
 * do things like encrypt and base64encode.  xbuf also does a good job of using fragmented heap by
 * not requiring large contiguous allocations.
 ******************************************************************************************************/
uint32_t EmonService(struct serviceBlock* _serviceBlock){
  // trace T_Emon
  enum   states {initialize,                    // Initialize the service
                getLastPostTime,                // Get Json details about inputs from Emoncms
                waitLastPostTime,               // Wait for previous async request to finish then process
                primeLastRecord,                // Get the logrec of the last data posted to Emoncms
                post,                           // Process logrecs and build reqData
                sendPost,                       // Send the data to Emoncms in plaintext post
                waitPost,                       // Wait for acknowledgent
                sendSecure,                     // Send the data to Emoncms using encrypted protocol
                waitSecure};                    // Wait for acknowledgement
  static states state = initialize;
  static IotaLogRecord* lastRecord = nullptr;
  static IotaLogRecord* logRecord = nullptr;
  static uint32_t UnixLastPost = 0;
  static uint32_t UnixNextPost = 0;
  static double elapsedHours;
  static xbuf reqData;
  static uint32_t reqUnixtime = 0;
  static int  reqEntries = 0; 
  static uint32_t postTime = millis();
  static asyncHTTPrequest* request = nullptr;
  static String base64Sha;
  static int32_t retryCount = 0;
          
  trace(T_Emon,0);
  if( ! _serviceBlock) return 0;

            // If stop signaled, do so.  


  if((EmonRestart || EmonStop) && state != waitPost && state != waitSecure) {
    trace(T_Emon,1);
    delete lastRecord;
    lastRecord = nullptr;
    delete logRecord;
    logRecord = nullptr;
    delete request;
    request = nullptr;
    delete request;
    request = nullptr;
    state = initialize;
    if(EmonStop) {
      EmonStop = false;
      EmonStarted = false;
      EmonRevision = -1;
      msgLog("EmonService: stopped.");
      return 0;
    }
    EmonRestart = false;
    return 1;
  }
        
  switch(state){

    case initialize: {
      trace(T_Emon,2);

          // We post the log to EmonCMS,
          // so wait until the log service is up and running.
      
      if(!currLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("EmonService: started.", 
       "url: " + EmonURL + ":" + String(EmonPort) + EmonURI + ", node: " + String(node) + ", post interval: " + 
       String(EmonCMSInterval) + (EmonSend == EmonSendGET ? ", unsecure GET" : ", encrypted POST"));
      EmonStarted = true;
      state = getLastPostTime; 
    }

    case getLastPostTime: {
      trace(T_Emon,3);
      if(! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/get?node=" + String(node);
      request->setTimeout (1);
      request->setDebug(false);
      trace(T_Emon,3);
      request->open("GET", URL.c_str());
      String auth = "Bearer " + apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      trace(T_Emon,3);
      request->send();
      state = waitLastPostTime;
      return 1;
    } 

    case waitLastPostTime: {
      trace(T_Emon,4);
      if(request->readyState() != 4){
        return UNIXtime() + 1; 
      }
      HTTPrequestFree++;
      trace(T_Emon,4);
      if(request->responseHTTPcode() != 200){
        msgLog("EmonService: get input list failed, code: ", request->responseHTTPcode());
        state = getLastPostTime;
        return UNIXtime() + 5;
      }

      trace(T_Emon,4);    
      String response = request->responseText();
      delete request;
      request = nullptr;
      if (response.startsWith("\"Node does not exist\"")){
        msgLog(F("EmonService: Node doesn't yet exist, starting posting now."));
        UnixLastPost = UNIXtime();
        UnixLastPost -= UnixLastPost % EmonCMSInterval;
      }
      else {
        trace(T_Emon,4);
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
      state = primeLastRecord;
      return 1;
    }

    case primeLastRecord: {
      trace(T_Emon,5);  

            // Get the last record in the log.
            // Posting will begin with the next log entry after this one,

      if( ! lastRecord){
        lastRecord = new IotaLogRecord;
      }
      trace(T_Emon,5);
      lastRecord->UNIXtime = UnixLastPost;      
      currLog.readKey(lastRecord);

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
      trace(T_Emon,6);

          // If WiFi is not connected,
          // just return without attempting to log and try again in a few seconds.

      if(WiFi.status() != WL_CONNECTED) {
        return UNIXtime() + 1;
      }

          // Determine when the next post should occur and wait if needed.
          // Careful here - arithmetic is unsigned.

      uint32_t nextBulkPost = UnixNextPost + ((EmonBulkSend > reqEntries) ? EmonBulkSend - reqEntries : 0) * EmonCMSInterval;
      if(currLog.lastKey() < nextBulkPost){
        return nextBulkPost;
      }
      
          // Read the next log record.
          
      trace(T_Emon,6);
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
      
      trace(T_Emon,6);
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
     
      trace(T_Emon,6);
      reqData.printf_P(PSTR("[%d,\"%s\""), UnixNextPost - reqUnixtime, node.c_str());
            
      double value1;
      if( ! emonOutputs){  
        for (int i = 0; i < maxInputs; i++) {
          IotaInputChannel *_input = inputChannel[i];
          value1 = (logRecord->accum1[i] - lastRecord->accum1[i]) / elapsedHours;
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
        trace(T_Emon,6);
        Script* script = emonOutputs->first();
        int index=1;
        while(script){
          while(index++ < String(script->name()).toInt()) reqData.write(",null");
          value1 = script->run(lastRecord, logRecord, elapsedHours);
          reqData.printf(",%.*f", script->precision(), value1);
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
         (reqData.available() < 1000))) {
        return UnixNextPost;
      }

          // Send the post       

      reqData.write(']');
      if(EmonSend == EmonSendGET){
        state = sendPost;
      }
      else {
        state = sendSecure;
      }
      return 1;
    }

    case sendPost:{
      trace(T_Emon,7);
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
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(false);
      if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.println(msg);
      }
      request->open("POST", URL.c_str());
      trace(T_Emon,7);
      String auth = "Bearer " + apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_Emon,7);
      request->send(&reqData, reqData.available());
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_Emon,8);
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrequestFree++;
      trace(T_Emon,8);
      if(request->responseHTTPcode() != 200){
        if(++retryCount  % 10 == 0){
            msgLog("EmonService: retry count ", retryCount);
        }
        UnixLastPost = reqUnixtime - EmonCMSInterval;
        state = primeLastRecord;
        return UNIXtime() + 1;
      }
      trace(T_Emon,8);
      String response = request->responseText();
      if(! response.startsWith("ok")){
        msgLog("EmonService: response not ok. Retrying.");
        Serial.println(response.substring(0,60));
        UnixLastPost = reqUnixtime - EmonCMSInterval;
        state = primeLastRecord;
        return UNIXtime() + 1;
      }
      trace(T_Emon,8);
      delete request;
      request = nullptr;
      retryCount = 0;
      reqData.flush();
      reqEntries = 0;    
      state = post;
      return UnixNextPost + EmonBulkSend ? 1 : 0;
    }

case sendSecure:{
      trace(T_Emon,9);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      if( ! request) {
        request = new asyncHTTPrequest;
      }
      trace(T_Emon,9);

        // Need to put in a decent RNG, have plenty of entropy in the low ADC bits.

      uint8_t iv[16];
      os_get_random((unsigned char*)iv,16);
      
        // Initialize sha256, shaHMAC and cypher

      SHA256* sha256 = new SHA256;
      sha256->reset();
      SHA256* shaHMAC = new SHA256;
      shaHMAC->resetHMAC(cryptoKey,16);
      CBC<AES128>* cypher = new CBC<AES128>;
      cypher->setIV(iv, 16);
      cypher->setKey(cryptoKey, 16);

        // Process payload while
        // updating SHAs and encrypting. 

      trace(T_Emon,9);    
      uint8_t* temp = new uint8_t[64+16];
      size_t supply = reqData.available();
      reqData.write(iv, 16);
      while(supply){
        size_t len = supply < 64 ? supply : 64;
        reqData.read(temp, len);
        supply -= len;
        sha256->update(temp, len);
        shaHMAC->update(temp, len);
        if(len < 64 || supply == 0){
          size_t padlen = 16 - (len % 16);
          for(int i=0; i<padlen; i++){
            temp[len+i] = padlen;
          }
          len += padlen;
        }
        cypher->encrypt(temp, temp, len);
        reqData.write(temp, len);
      }
      trace(T_Emon,9);
      delete[] temp;
      delete cypher;
      
        // finalize the Sha256 and shaHMAC

      trace(T_Emon,9);
      uint8_t value[32];
      sha256->finalize(value, 32);
      String _base64Sha = base64encode(value, 32);
      shaHMAC->finalizeHMAC(cryptoKey, 16, value, 32);
      delete sha256;
      delete shaHMAC;

        // Now base64 encode and send

      base64encode(&reqData); 
      trace(T_Emon,10);
      String URL = EmonURL + ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(false);
      trace(T_Emon,10); 
      String auth = EmonUsername + ':' + bin2hex(value, 32);
      if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        Serial.printf_P(PSTR("time %02d:%02d:%02d, length %d, %d\r\n"), now.hour(),now.minute(),now.second(), reqData.available(), reqUnixtime);
      }
      request->open("POST", URL.c_str());
      trace(T_Emon,10);
      request->setReqHeader("Content-Type","aes128cbc");
      request->setReqHeader("Authorization", auth.c_str());
      trace(T_Emon,10);
      request->send(&reqData, reqData.available());
      reqData.flush();
      state = waitSecure;
      return 1;
    } 

    case waitSecure: {
      trace(T_Emon,11);
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrequestFree++;
      reqData.flush();
      trace(T_Emon,11);
      if(request->responseHTTPcode() != 200){
        if(++retryCount  % 10 == 0){
            msgLog("EmonService: retry count ", retryCount);
        }
        UnixLastPost = reqUnixtime - EmonCMSInterval;
        state = primeLastRecord;
        return UNIXtime() + 1;
      }
      trace(T_Emon,11);
      String response = request->responseText();
      if(! response.startsWith(base64Sha)){
        msgLog("EmonService: Invalid response, Retrying.");
        Serial.println(base64Sha);
        Serial.println(response);
        UnixLastPost = reqUnixtime - EmonCMSInterval;
        state = primeLastRecord;
        return UNIXtime() + 1;
      }
      trace(T_Emon,11);
      delete request;
      request = nullptr;
      retryCount = 0;
      reqEntries = 0;    
      state = post;
      return UnixNextPost + EmonBulkSend ? 1 : 0;
    }
  }
  return 1;   
}


          // EmonConfig - process the configuration Json
          // invoked from getConfig

bool EmonConfig(const char* configObj){
  DynamicJsonBuffer Json;
  JsonObject& config = Json.parseObject(configObj);
  if( ! config.success()){
    msgLog(F("EmonService: Json parse failed."));
    return false;
  }
  trace(T_EmonConfig,0);
  if(config["type"].as<String>() != "emoncms"){
    EmonStop = true;
    if(config["type"].as<String>() == "none") return true;
    return false;
  }
  trace(T_EmonConfig,1);
  if(EmonRevision == config["revision"]){
    return true;
  }
  EmonRevision = config["revision"];
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
  trace(T_EmonConfig,2);
  apiKey = config["apikey"].as<String>();
  node = config["node"].as<String>();
  EmonCMSInterval = config["postInterval"].as<int>();
  EmonBulkSend = config["bulksend"].as<int>();
  if(EmonBulkSend > 10) EmonBulkSend = 10;
  if(EmonBulkSend <1) EmonBulkSend = 1;
  EmonUsername = config["userid"].as<String>();
  EmonSend = EmonSendGET;
  if(EmonUsername != "")EmonSend = EmonSendPOSTsecure;
  
  trace(T_EmonConfig,3);
  #define hex2bin(x) (x<='9' ? (x - '0') : (x - 'a') + 10)
  apiKey.toLowerCase();
  for(int i=0; i<16; i++){
    cryptoKey[i] = hex2bin(apiKey[i*2]) * 16 + hex2bin(apiKey[i*2+1]); 
  }
  delete emonOutputs;
  JsonVariant var = config["outputs"];
  if(var.success()){
    trace(T_EmonConfig,4);
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
  trace(T_EmonConfig,5);
  if( ! EmonStarted) {
    trace(T_EmonConfig,6);
    NewService(EmonService);
    EmonStarted = true;
    EmonStop = false;
  }
  trace(T_EmonConfig,7);
  return true; 
}
