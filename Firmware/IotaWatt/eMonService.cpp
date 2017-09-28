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

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <SD.h>

#include "IotaWatt.h"
#include "msgLog.h"
#include "IotaLog.h"
#include "timeServices.h"

boolean EmonSendData(uint32_t reqUnixtime, String reqData);   
String base64encode(const uint8_t* in, size_t len);
String encryptData(String in, uint8_t* key);
  
uint32_t EmonService(struct serviceBlock* _serviceBlock){
  // trace T_Emon
  enum   states {initialize, post, resend};
  static states state = initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static File EmonPostLog;
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
          
  trace(T_Emon,0);

            // If stop signaled, do so.  

  if(EmonStop) {
    msgLog("EmonService: stopped.");
    EmonStarted = false;
    trace(T_Emon,4);
    EmonPostLog.close();
    trace(T_Emon,5);
    SD.remove((char *)EmonPostLogFile.c_str());
    trace(T_Emon,6);
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
      
      if(!iotaLog.isOpen()){
        return UNIXtime() + 5;
      }
      msgLog("EmonService: started.", 
       "url: " + EmonURL + EmonURI + ", node: " + String(node) + ", post interval: " + String(EmonCMSInterval) +
       (EmonSend == EmonSendGET ? ", unsecure GET" : ", encrypted POST")); 

     
      if(!EmonPostLog){
        EmonPostLog = SD.open(EmonPostLogFile,FILE_WRITE);
      }
            
      if(EmonPostLog){
        if(EmonPostLog.size() == 0){
          buf->data = iotaLog.lastKey();
          EmonPostLog.write((byte*)buf,4);
          EmonPostLog.flush();
          msgLog("EmonService: Emonlog file created.");
        }
        EmonPostLog.seek(EmonPostLog.size()-4);
        EmonPostLog.read((byte*)buf,4);
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

          // If WiFi is not connected,
          // just return without attempting to log and try again in a few seconds.

      if(WiFi.status() != WL_CONNECTED) {
        return 2; 
      }

          // If we are current,
          // Anticipate next posting at next regular interval and break to reschedule.
 
      if(iotaLog.lastKey() < UnixNextPost){ 
        UnixNextPost = UNIXtime() + EmonCMSInterval - (UNIXtime() % EmonCMSInterval);
        return UnixNextPost;
      } 
      
          // Not current.  Read sequentially to get the entry >= scheduled post time
          
      trace(T_Emon,1);    
      while(logRecord->UNIXtime < UnixNextPost){
        if(logRecord->UNIXtime >= iotaLog.lastKey()){
          msgLog("runaway seq read.", logRecord->UNIXtime);
          ESP.reset();
        }
        iotaLog.readNext(logRecord);
      }

          // Adjust the posting time to match the log entry time.
            
      UnixNextPost = logRecord->UNIXtime - logRecord->UNIXtime % EmonCMSInterval;

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
     
      trace(T_Emon,2);

      
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
          while(index++ < String(script->name()).toInt()) reqData += ',';
          value1 = script->run([](int i)->double {return (logRecord->channel[i].accum1 - accum1Then[i]) / elapsedHours;});
          if(value1 > -1.0 && value1 < 1){
            reqData += "0,";
          }
          else {
            reqData += String(value1,1) + ',';
          }
          script = script->next();
        }
      }
      for (int i = 0; i < maxInputs; i++) {  
        accum1Then[i] = logRecord->channel[i].accum1;
      }
      trace(T_Emon,3);    
      reqData.setCharAt(reqData.length()-1,']');
      reqEntries++;
      UnixLastPost = UnixNextPost;
      UnixNextPost +=  EmonCMSInterval - (UnixNextPost % EmonCMSInterval);
      
      if ((reqEntries < EmonBulkSend) ||
         ((iotaLog.lastKey() > UnixNextPost) &&
         (reqData.length() < 1000))) {
        return UnixNextPost;
      }

          // Send the post       

      reqData += ']';
      if(!EmonSendData(reqUnixtime, reqData)){
        state = resend;
        return UNIXtime() + 30;
      }
      buf->data = UnixLastPost;
      EmonPostLog.write((byte*)buf,4);
      EmonPostLog.flush();
      reqData = "";
      reqEntries = 0;    
      state = post;
      return UnixNextPost;
    }
  

    case resend: {
      msgLog("Resending EmonCMS data.");
      if(!EmonSendData(reqUnixtime, reqData)){ 
        return UNIXtime() + 60;
      }
      else {
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
boolean EmonSendData(uint32_t reqUnixtime, String reqData){ 
  trace(T_Emon,7);
  uint32_t startTime = millis();
  Serial.println(reqData);
  
  if(EmonSend == EmonSendGET){
    String URL = EmonURI + "/input/bulk.json?" + reqData + "&apikey=" + apiKey;
    http.begin(EmonURL, 80, URL);
    http.setTimeout(500);
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      msgLog("EmonService: GET failed. HTTP code: ", http.errorToString(httpCode));
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
    String URI = EmonURI + "/input/bulk";               
    sha256.reset();
    sha256.update(reqData.c_str(), reqData.length());
    uint8_t value[32];
    sha256.finalize(value, 32);
    String base64Sha = base64encode(value, 32);
    sha256.resetHMAC(cryptoKey,16);
    sha256.update(reqData.c_str(), reqData.length());
    sha256.finalizeHMAC(cryptoKey, 16, value, 32);
    String hmac = bin2hex(value, 32);
    String auth = EmonUsername + ':' + hmac;
    http.begin(EmonURL, 80, URI);
    http.addHeader("Host",EmonURL);
    
    http.addHeader("Content-Type","aes128cbc");
    http.addHeader("Authorization", auth.c_str());
    http.setTimeout(500);
    int httpCode = http.POST(encryptData(reqData, cryptoKey));
    String response = http.getString();
    http.end();
    if(httpCode != HTTP_CODE_OK){
      String code = String(httpCode);
      if(httpCode < 0){
        code = http.errorToString(httpCode);
      }
      msgLog("EmonService: POST failed. HTTP code: ", code);
      Serial.println(response);
      return false;
    }
    if(response.startsWith(base64Sha)){
      return true;        
    }
    msgLog(reqData);
    msgLog("EmonService: Invalid response: ", response.substring(0,44));
    msgLog("EmonService: Expectd response: ", base64Sha);
    
    return true;
  }
  
  msgLog("EmonService: Unsupported protocol - ", EmonSend);
  return false;
}

String encryptData(String in, const uint8_t* key) {
  uint8_t iv[16];
  os_get_random((unsigned char*)iv,16);
  uint8_t padLen = 16 - (in.length() % 16);
  int encryptLen = in.length() + padLen;
  uint8_t* ivBuf = new uint8_t[encryptLen + 16];
  uint8_t* encryptBuf = ivBuf + 16;
  for(int i=0; i<16; i++){
    ivBuf[i] = iv[i];
  }  
  for(int i=0; i<in.length(); i++){
    encryptBuf[i] = in[i];
  }
  for(int i=0; i<padLen; i++){
    encryptBuf[in.length()+i] = padLen;
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

String bin2hex(const uint8_t* in, size_t len){
  static const char* hexcodes = "0123456789abcdef";
  String out = "";
  for(int i=0; i<len; i++){
    out += hexcodes[*in >> 4];
    out += hexcodes[*in++ & 0x0f];
  }
  return out;
}

