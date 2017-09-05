/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock) {
  #define SIGNATURE_SIZE 64 
  static bool upToDateMsg = false;
  uint8_t signature[64];
  uint8_t sha[32];
  char md5char[33] = "0123456789abcdef0123456789abcdef";
  http.begin(updateURL, 80, updateURI);
  http.addHeader(F("Host"),updateURL);
  http.setUserAgent(F("IotaWatt"));
  http.addHeader(F("X_STA_MAC"), WiFi.macAddress());
  http.addHeader(F("X-UPDATE-CLASS"), updateClass);
  http.addHeader(F("X_CURRENT_VERSION"), IOTAWATT_VERSION);
  http.setTimeout(1000);
  int httpCode = http.GET();
  int len = http.getSize();
  if(len < 200){
    msgLog("Updater HTTP failed: \r\n", http.getString());
  }
  else if(httpCode == HTTP_CODE_OK){
    if(len > 64){
      msgLog("Updater: Downloading update.");
      UpdaterClass update;
      MD5Builder md5;
      sha256.reset();
      md5.begin();      
      uint32_t buffSize = 512;
      uint8_t* buff = new uint8_t[buffSize];
      uint32_t chunk;  
      uint32_t objLen = len - SIGNATURE_SIZE;
      update.begin(objLen);
      update.setMD5(md5char);
      int bytesRead = 0;
      int bytesWritten = 0;
      uint32_t timeStart = millis();
      while(objLen){
        chunk = MIN(objLen, buffSize);
        if((millis()-timeStart) > 5000){
          msgLog("Updater: download timeout.");
          break;
        }
        bytesRead = http.getStreamPtr()->readBytes(buff, chunk);
        if(bytesRead > 0){
          timeStart = millis();
          md5.add(buff, bytesRead);
          sha256.update(buff, bytesRead);
          bytesWritten = update.write(buff, bytesRead);
          objLen -= bytesRead;
        }
      }
      delete[] buff;
      if(objLen == 0){
        http.getStreamPtr()->readBytes(signature, 64);
        sha256.finalize(sha, 32);
        if(! Ed25519::verify(signature, publicKey, sha, 32)){
          msgLog("Updater: Signature does not verify.");
          update.end();
        }
        else {
          msgLog("Updater: Signature verified");     
          md5.calculate();
          md5.getChars(md5char);
          update.setMD5(md5char);
          if( ! update.end()) {
            msgLog("Updater: Update end failed: ", update.getError());
          }
          else {
            msgLog("Updater: Firmware updated. Restarting...");
            delay(500);
            ESP.restart();
          }
        } 
      } 
      else {
        update.end();
      }
    }
    else {
      msgLog("Update: update length is zero.");
    }
  }
  else if(httpCode != HTTP_CODE_NOT_MODIFIED) {
    msgLog("Update: GET failed. HTTP code: ", http.errorToString(httpCode));
  }  

  http.end();
  return ((uint32_t)UNIXtime() + updaterServiceInterval);
}

void printHex(uint8_t* data, size_t len){
  const char* hexchars = "0123456789abcdef";
  for(int i=0; i<len; i+=16){
    for(int j=i; j<(i+16); j++){
      if(j < len){
        Serial.print(hexchars[*data >> 4]);
        Serial.print(hexchars[*data++ & 0x0f]);
      }
      if((j % 4) == 3) Serial.print(" ");
    }
    Serial.println();
  }
}



