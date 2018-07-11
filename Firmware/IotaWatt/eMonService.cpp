#include "IotaWatt.h"
      
bool      EmonStarted = false;                    // set true when Service started
bool      EmonStop = false;                       // set true to stop the Service
bool      EmonRestart = true;                     // Initialize or reinitialize EmonService 
uint32_t  EmonLastPost = 0;                       // Last acknowledged post for status

      // Configuration settings

char*     EmonURL = nullptr;                                // These are set from the config file 
char*     EmonURI = nullptr;
char*     apiKey = nullptr;
char*     emonNode = nullptr;
char*     EmonUsername = nullptr;
uint16_t  EmonPort = 80;
int16_t   EmonBulkSend = 1;
int32_t   EmonRevision = -1;
uint32_t  EmonBeginPosting = 0;
uint8_t   cryptoKey[16];
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
                queryLastGet,                // Get Json details about inputs from Emoncms
                queryLastWait,               // Wait for previous async request to finish then process
                getLastRecord,                // Get the logrec of the last data posted to Emoncms
                post,                           // Process logrecs and build reqData
                sendPost,                       // Send the data to Emoncms in plaintext post
                waitPost,                       // Wait for acknowledgent
                sendSecure,                     // Send the data to Emoncms using encrypted protocol
                waitSecure};                    // Wait for acknowledgement
  static states state = initialize;
  static IotaLogRecord* logRecord = nullptr;
  static IotaLogRecord* oldRecord = nullptr;
  static uint32_t lastRequestTime = 0;          // Time of last measurement in last or current request
  static uint32_t UnixNextPost = 0;             // Next measurement to be posted
  static xbuf reqData;
  static uint32_t reqUnixtime = 0;              // First measurement in current reqData
  static int  reqEntries = 0;                   // Number of measurement intervals in current reqData
  static int32_t retryCount = 0;
  static asyncHTTPrequest* request = nullptr;
  static char* base64Sha = nullptr;
  const  size_t reqDataLimit = 2000;            // Transaction yellow light size
          
  trace(T_Emon,0);
  if( ! _serviceBlock) return 0;

            // If stop signaled, do so.  

  if(EmonRestart) {
    trace(T_Emon,1);
    state = initialize;
    EmonRestart = false;
  }
        
  switch(state){

    case initialize: {
      trace(T_Emon,2);

          // We post the log to EmonCMS,
          // so wait until the log service is up and running.
      
      if(!currLog.isOpen()){
        return UNIXtime() + 5;
      }
      log("EmonService: started. url:%s:%d%s,node:%s,interval:%d,%s", EmonURL, EmonPort, EmonURI, 
           emonNode, EmonCMSInterval, (EmonSend == EmonSendGET ? " unsecure GET" : " encrypted POST"));
      EmonStarted = true;
      retryCount = 0;
      EmonLastPost = EmonBeginPosting;
      state = queryLastGet;
      return 1; 
    }

    case queryLastGet: {
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
      String URL(EmonURL);
      URL += ":" + String(EmonPort) + EmonURI + "/input/get?node=" + String(emonNode);
      request->setTimeout (10);
      request->setDebug(false);
      trace(T_Emon,3);
      request->open("GET", URL.c_str());
      String auth("Bearer ");
      auth += apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      trace(T_Emon,3);
      request->send();
      state = queryLastWait;
      return UNIXtime() + 1;
    } 

    case queryLastWait: {
      trace(T_Emon,4);
      if(request->readyState() != 4){
        return UNIXtime() + 1; 
      }
      HTTPrequestFree++;
      trace(T_Emon,4);
      if(request->responseHTTPcode() != 200){
        state = queryLastGet;
        if(++retryCount < 10){
          return UNIXtime() + 1;
        }
        if(retryCount == 10){
          log("EmonService: get input list failing, code: %d", request->responseHTTPcode());
        }
        return UNIXtime() + 30;
      }

      trace(T_Emon,4);    
      String response = request->responseText();
      delete request;
      request = nullptr;
      if (response.startsWith("\"Node does not exist\"")){
        log("EmonService: Node doesn't yet exist.");
      }
      else {
        trace(T_Emon,4);
        int pos = 0;
        while((pos = response.indexOf("\"time\":", pos)) > 0) {      
          pos += 7;
          uint32_t _time = (uint32_t)response.substring(pos, response.indexOf(',',pos)).toInt();
          EmonLastPost = MAX(EmonLastPost, _time);
        }
      }
      if(EmonLastPost == 0 || EmonLastPost > currLog.lastKey()) {
        EmonLastPost = currLog.lastKey();
      }  
      if(EmonLastPost < currLog.firstKey()){
        EmonLastPost = currLog.firstKey();
      }
      log("EmonService: Start posting at %s", dateString(EmonLastPost + EmonCMSInterval).c_str());
      state = getLastRecord;
      return 1;
    }

    case getLastRecord: {
      trace(T_Emon,5);  

            // Get the last record in the log.
            // Posting will begin with the next log entry after this one,

      if( ! oldRecord){
        oldRecord = new IotaLogRecord;
      }
      trace(T_Emon,5);
      oldRecord->UNIXtime = EmonLastPost;      
      currLog.readKey(oldRecord);

            // Assume that record was posted (not important).
            // Plan to start posting one interval later
        
      EmonLastPost = oldRecord->UNIXtime;
      UnixNextPost = EmonLastPost + EmonCMSInterval - (EmonLastPost % EmonCMSInterval);
        
            // Advance state.

      reqData.flush();
      reqEntries = 0;
      state = post;
      return 1;
    }
    
    case post: {
      trace(T_Emon,6);

          // If stop requested, do so now.

      if(EmonStop){
        if(request && request->readyState() < 4) return 1;
        trace(T_Emon,61);
        log("EmonService: Stopped.  Last post %s", dateString(EmonLastPost).c_str());
        EmonStarted = false;
        EmonStop = false;
        state = initialize;
        delete oldRecord;
        oldRecord = nullptr;
        delete logRecord;
        logRecord = nullptr;
        delete request;
        request = nullptr;
        reqData.flush();
        delete[] EmonURL;
        EmonURL = nullptr;
        delete[] EmonURI;                                // These are set from the config file 
        EmonURI = nullptr;
        delete[] apiKey;
        apiKey = nullptr;
        delete[] emonNode;
        emonNode = nullptr;
        delete[] EmonUsername;
        EmonUsername = nullptr;
        delete emonOutputs;
        emonOutputs = nullptr;
        return 0;  
      }

          // If not enough entries for bulk-send, come back in one second;

      if(((currLog.lastKey() - EmonLastPost) / EmonCMSInterval + reqEntries) < EmonBulkSend){
        return UNIXtime() + 1;
      }

          // If buffer isn't full,
          // add another measurement.

      if(reqData.available() < reqDataLimit && UnixNextPost <= currLog.lastKey()){  
 
            // Read the next log record.
            
        trace(T_Emon,6);
        if( ! logRecord){
          logRecord = new IotaLogRecord;
        }
        logRecord->UNIXtime = UnixNextPost;
        logReadKey(logRecord);    
      
            // Compute the time difference between log entries.
            // If zero, don't bother.
            
        double elapsedHours = logRecord->logHours - oldRecord->logHours;
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
        reqData.printf_P(PSTR("[%d,\"%s\""), UnixNextPost - reqUnixtime, emonNode);
        lastRequestTime = UnixNextPost;
              
        double value1;
        if( ! emonOutputs){  
          for (int i = 0; i < maxInputs; i++) {
            IotaInputChannel *_input = inputChannel[i];
            value1 = (logRecord->accum1[i] - oldRecord->accum1[i]) / elapsedHours;
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
            value1 = script->run(oldRecord, logRecord, elapsedHours);
            reqData.printf(",%.*f", script->precision(), value1);
            script = script->next();
          }
        }
        trace(T_Emon,6);
        reqData.write(']');
        reqEntries++;
        delete oldRecord;
        oldRecord = logRecord;
        logRecord = nullptr;
        UnixNextPost +=  EmonCMSInterval - (UnixNextPost % EmonCMSInterval);
      }

            // If buffer not full and there is a data backlog,
            // return to fill buffer.

      if(reqData.available() < reqDataLimit && UnixNextPost < currLog.lastKey()){
        return 1;
      }

            // Write the data.

      reqData.write(']');
      state = (EmonSend == EmonSendGET) ? sendPost : sendSecure;
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
      String URL(EmonURL);
      URL += ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(false);
      if(request->debug()){
        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
        String msg = timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second());
        Serial.println(msg);
      }
      request->open("POST", URL.c_str());
      trace(T_Emon,7);
      String auth("Bearer ");
      auth += apiKey;
      request->setReqHeader("Authorization", auth.c_str());
      request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
      trace(T_Emon,7);
      request->send(&reqData, reqData.available());
      state = waitPost;
      return 1;
    } 

case sendSecure:{
      trace(T_Emon,9);
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }

              // Temporary pause code until lwip 2 is fixed for "time Wait" problem.

      if(ESP.getFreeHeap() < 15000){
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
      delete[] base64Sha;
      base64Sha = charstar(base64encode(value, 32).c_str());
      shaHMAC->finalizeHMAC(cryptoKey, 16, value, 32);
      delete sha256;
      delete shaHMAC;

        // Now base64 encode and send

      base64encode(&reqData); 
      trace(T_Emon,10);
      String URL(EmonURL);
      URL += ":" + String(EmonPort) + EmonURI + "/input/bulk";
      request->setTimeout(1);
      request->setDebug(false);
      trace(T_Emon,10); 
      String auth(EmonUsername);
      auth += ':' + bin2hex(value, 32);
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
      state = waitPost;
      return 1;
    } 

    case waitPost: {
      trace(T_Emon,11);
      if(request->readyState() != 4){
        return 1; 
      }
      HTTPrequestFree++;
      reqData.flush();
      trace(T_Emon,11);
      if(request->responseHTTPcode() != 200){
        if(++retryCount == 3){
            log("EmonService: HTTP response %d, retrying.", request->responseHTTPcode());
        }
        EmonLastPost = lastRequestTime;
        state = getLastRecord;
        return UNIXtime() + retryCount / 10;
      }
      trace(T_Emon,11);
      String response = request->responseText();
      if((EmonSend == EmonSendGET && ! response.startsWith("ok")) ||
        (EmonSend == EmonSendPOSTsecure && ! response.startsWith(base64Sha))){
        if(++retryCount == 3){
          log("EmonService: Invalid response, retrying.");
        }
        EmonLastPost = reqUnixtime - EmonCMSInterval;
        state = getLastRecord;
        return UNIXtime() + retryCount / 10;
      }
      trace(T_Emon,11);
      if(retryCount >= 3){
        log("EmonService: Retry successful after %d attempts.", retryCount);
      }
      retryCount = 0;
      reqEntries = 0;
      EmonLastPost = lastRequestTime;     
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
    log("EmonService: Json parse failed.");
    return false;
  }
  trace(T_EmonConfig,0);
  if(config["type"].as<String>() != "emoncms"){
    EmonStop = true;
    if(config["type"].as<String>() == "none") return true;
    return false;
  }
  trace(T_EmonConfig,1);
  int revision = config["revision"];
  if(revision == EmonRevision) {
    return true;
  }
  EmonRevision = config["revision"];
  if(config["stop"].as<bool>()){
    trace(T_EmonConfig,1);
    EmonStop = true;
  }
  else if(EmonStarted){
    trace(T_EmonConfig,2);
    EmonRestart = true;
  }
  String URL = config["url"].as<String>();
  URL = config["url"].as<String>();
  if(URL.startsWith("http://")) URL = URL.substring(7);
  else if(URL.startsWith("https://")){
    URL = URL.substring(8);
  }
  delete[] EmonURI;
  if(URL.indexOf("/") > 0){
    EmonURI = charstar(URL.substring(URL.indexOf("/")).c_str());
    URL.remove(URL.indexOf("/"));
  } else {
    EmonURI = charstar("");
  }
  if(URL.indexOf(":") > 0){
    EmonPort = URL.substring(URL.indexOf(":")+1).toInt();
    URL.remove(URL.indexOf(":"));
  }
  delete[] EmonURL;
  EmonURL = charstar(URL.c_str());
  trace(T_EmonConfig,2);
  EmonBeginPosting = config.get<uint32_t>("begdate");
  delete[] apiKey;
  apiKey = charstar(config["apikey"].as<char*>());
  hex2bin(cryptoKey, apiKey, 16);
  delete[] emonNode;
  emonNode = charstar(config["node"].as<char*>());
  EmonCMSInterval = config["postInterval"].as<int>();
  EmonBulkSend = config["bulksend"].as<int>();
  if(EmonBulkSend > 10) EmonBulkSend = 10;
  if(EmonBulkSend <1) EmonBulkSend = 1;
  delete[] EmonUsername;
  EmonUsername = charstar(config["userid"].as<char*>());
  EmonSend = EmonSendGET;
  if(strlen(EmonUsername)){
    EmonSend = EmonSendPOSTsecure;
  } 
  trace(T_EmonConfig,3);
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
    NewService(EmonService, T_Emon);
    EmonStarted = true;
  }
  trace(T_EmonConfig,7);
  return true; 
}
