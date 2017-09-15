/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock) {
  if(checkUpdate()){
    msgLog ("Firmware updated, restarting.");
    delay(500);
    ESP.restart();
  }  
  return UNIXtime() + updaterServiceInterval;
}

/*************************************************************************************************
 *  bool checkUpdate()
 *  
 *  Check to see if there is an update available.
 *  If so, download, install, return true;.
 *  If not, return false;
  ************************************************************************************************/
bool checkUpdate(){
  if(updateClass == "NONE") return false;
  http.begin(updateURL, 80, updateURI);
  http.addHeader(F("Host"),updateURL);
  http.setUserAgent(F("IotaWatt"));
  http.addHeader(F("X_STA_MAC"), WiFi.macAddress());
  http.addHeader(F("X-UPDATE-CLASS"), updateClass);
  http.addHeader(F("X_CURRENT_VERSION"), IOTAWATT_VERSION);
  http.setTimeout(500);
  int httpCode = http.GET();
  uint32_t len = http.getSize();
  if(httpCode != HTTP_CODE_OK  || len != 8){
    msgLog("checkUpdate: Invalid response from server.");
    http.writeToStream(&Serial);
    Serial.println();
    http.end();
    return false;
  }
  String updateVersion = http.getString();
  http.end();
  if(strcmp(updateVersion.c_str(),IOTAWATT_VERSION) == 0){
    return false;
  }
  String msg = "Update from " + String(IOTAWATT_VERSION) + " to " + updateVersion;
  msgLog("Updater: ", msg);
  if(downloadUpdate(updateVersion)) {
     return installUpdate(updateVersion);
  }
  deleteRecursive(updateVersion);
  return false; 
}

/**************************************************************************************************
 * bool downloadUpdate(String version)
 * 
 * This function will download a release version blob from IotaWatt.com,
 * verify the signature,
 * create a release installation directory on the SD with all of the component files of the release
 * create a binary of the ESP8266 firmware on the SD with an md5 hash on the end.
 * 
 * This function is used by the updater service when an update is indicated,
 * and it could also be used by a web server app to initiate a manual change of release.
 * 
 * Only release files from IotaWatt.com can be verified because the private-key is needed to
 * sign with the digital signature.
 * 
 *************************************************************************************************/

bool downloadUpdate(String version){
  union {
  struct {
    char file[4];
    uint32_t len;
    char name[24];
  } fileHeader;
  struct { 
    char IotaWatt[8];
    char release[8];
  } updtHeader;
  uint8_t header[32];
} headers;

  char updtDirName[9];
  strcpy(updtDirName,version.c_str());
  String URI = "/firmware/bin/" + String(updtDirName) + ".bin";
  File updtDir;
  bool binaryFound = false;
  http.begin(updateURL, 80, URI);
  http.addHeader(F("Host"),updateURL);
  http.setUserAgent(F("IotaWatt"));
  int httpCode = http.GET();
  if(httpCode != HTTP_CODE_OK) {
    Serial.print("Http code: ");
    Serial.println(http.errorToString(httpCode));
    http.writeToStream(&Serial);
    Serial.println();
    http.end();
    return false;
  }
  int signatureSize = 64;
  uint32_t binarySize = http.getSize() - signatureSize;
  sha256.reset();

        // Read and verify the header

  if(true){
    binarySize -= httpRead(headers.header,sizeof(headers.updtHeader));
    if((memcmp(headers.updtHeader.IotaWatt, "IotaWatt", 8) != 0) ||
       (memcmp(headers.updtHeader.release, updtDirName, 8) != 0)) {
      msgLog("Update file header invalid.");
      http.end();
      return false;
    }
  }
        // Create the local update directory.

  deleteRecursive(String(updtDirName));
  if( ! SD.mkdir(updtDirName)){
    msgLog("Cannot create update directory: ", updtDirName);
    return false;
  }
        // Read the update blob and create the various files.

  while(binarySize){
    if((httpRead(headers.header,sizeof(headers.fileHeader)) != sizeof(headers.fileHeader)) ||
       (memcmp(headers.fileHeader.file,"FILE",4) != 0)) {
      msgLog("Update file format error.");
      http.end();
      return false;
    }
    String filename = String(headers.fileHeader.name);
    bool iotawattBin = filename.equalsIgnoreCase("iotawatt.bin");
    if(iotawattBin){
      binaryFound = true;
      md5.begin();
    }
    binarySize -= sizeof(headers.fileHeader);
    String filePath = String(updtDirName) + "/" + headers.fileHeader.name;
    uint32_t fileSize = headers.fileHeader.len;
    File outFile = SD.open((char*)filePath.c_str(), FILE_WRITE);
    if( ! outFile){
      msgLog("Update: unable to create file: ", filePath);
      http.end();
      return false;
    }
    int buffSize = 512;
    uint8_t* buff = new uint8_t [buffSize];
    while(fileSize){
      int chunk = MIN(fileSize, buffSize);
      if(chunk % 8) chunk += 8 - chunk % 8;
      if(chunk != httpRead(buff, chunk)) break;
      binarySize -= chunk;
      outFile.write(buff, MIN(fileSize, buffSize));
      if(iotawattBin) md5.add(buff, MIN(fileSize, buffSize));
      fileSize -= MIN(fileSize, buffSize);
    }
    delete[] buff;
    if(iotawattBin) {
      md5.calculate();
      uint8_t md5Char[32];
      md5.getChars((char*)md5Char);
      outFile.write(md5Char, 32);
    }
    outFile.close();
    if(fileSize){
      return false;
    } 
  }
        // verify the signature
  
  uint8_t signature[64];
  uint8_t sha[32];
  if(http.getStreamPtr()->available() != 64){
    msgLog("Updater: Update rejected, no signature.");
    http.end();
    return false;
  }
  http.getStreamPtr()->readBytes(signature, 64);
  http.end();
  sha256.finalize(sha,32);
  if(! Ed25519::verify(signature, publicKey, sha, 32)){
    msgLog("Updater: Updater: Signature does not verify.");
    return false;
  }
  msgLog("Updater: Update downloaded and signature verified");
  return binaryFound;
}

size_t httpRead(uint8_t* buff, size_t len){
  uint32_t reqTime = millis();
  while(http.getStreamPtr()->available() < len){
    yield();
    if((millis() - reqTime) > 3000){
      msgLog("Updater: download timeout.");
      return 0;
    }
  }
  int bytesRead = http.getStreamPtr()->readBytes(buff, len);
  sha256.update(buff, bytesRead);
  return bytesRead;
}

/***********************************************************************************************************
 * bool installUpdate(String version)
 * 
 * Install the update in the directory "version".
 * Directory contains the ESP8266 binary "iotawatt.bin" with an md5 hash appended.
 * After binary is successfully updated, the ESP is restarted.
 * Upon restart, any files remaining in the update directory are 
 * placed in the SD root directory, replacing any pre-existing version.
 * 
 **********************************************************************************************************/

bool installUpdate(String version){
  UpdaterClass update;
  File inFile;
  String inPath = version + "/iotawatt.bin";
  inFile = SD.open((char*)inPath.c_str());
  if( ! inFile) return false;
  uint32_t inLen = inFile.size() - 32;
  update.begin(inLen);
  int buffSize = 512;
  uint8_t* buff = new uint8_t[buffSize];
  while(inLen){
    int chunk = MIN(inLen, buffSize);
    inFile.read(buff, chunk);
    update.write(buff, chunk);
    inLen -= chunk;
  }
  inFile.read(buff,32);
  update.setMD5((char*)buff);
  delete[] buff;
  if( ! update.end()){
    msgLog("Updater: update end failed. ", update.getError());
    return false; 
  }
  SD.remove((char*)inPath.c_str());
  msgLog("Updater: firmware upgraded to version ", version);
  return true;
}

/************************************************************************************************************
 * bool copyUpdate(String version)
 * 
 * Copy release files staged in the update directory to the SD root.
 * Delete the files as they are copied and delete the directory when complete.
 * 
 ***********************************************************************************************************/

bool copyUpdate(String version){
  File updtDir = SD.open((char*)version.c_str());
  if( ! updtDir) return false;
  if( ! updtDir.isDirectory()){
    updtDir.close();
    SD.remove((char*)version.c_str());
    return false;
  }
  msgLog("Updater: Installing update files for version ", version);
  int buffSize = 512;
  uint8_t* buff = new uint8_t [buffSize];
  File inFile;
  while(inFile = updtDir.openNextFile()){
    msgLog("Updater: Installing ", inFile.name());
    SD.remove(inFile.name());
    File outFile = SD.open(inFile.name(), FILE_WRITE);
    uint32_t fileSize = inFile.size();
    while(fileSize){
      int chunk = MIN(fileSize, buffSize);
      inFile.read(buff, chunk);
      outFile.write(buff, chunk);
      fileSize -= chunk;
    }
    inFile.close();
    outFile.close();
  }
  delete[] buff;
  msgLog("Updater: Installation complete.");
  deleteRecursive(version);
  return true;
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



