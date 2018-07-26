#include "IotaWatt.h"

bool   unpackUpdate(String updateVersion);

/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock) {
  enum states {initialize, checkAutoUpdate, getVersion, waitVersion, createFile, download, waitDownload, install};
  static states state = initialize;
  static asyncHTTPrequest* request = nullptr;
  static String updateVersion;
  static File releaseFile;
  static bool upToDate = false;
  static int checkResponse = 0;
  static char* _updateClass = nullptr;
  static uint32_t lastVersionCheck = 0;

  if( ! WiFi.isConnected()){
    return UNIXtime() + 1;
  }

  switch(state){

    case initialize: {
      log("Updater: service started. Auto-update class is %s", updateClass);
      _updateClass = charstar(updateClass);
      state = checkAutoUpdate;
      return 1;
    }

    case checkAutoUpdate: {
      if(strcmp(updateClass, _updateClass) != 0){
        delete[] _updateClass;
        _updateClass = charstar(updateClass);
        log("Updater: Auto-update class changed to %s", _updateClass);
        lastVersionCheck = 0;
        upToDate = false;
        state = getVersion;
        return 1;
      }
      else if (strcmp(_updateClass, "NONE") != 0 && UNIXtime() - lastVersionCheck > updaterServiceInterval){
        lastVersionCheck = UNIXtime();
        state = getVersion;
        return 1;
      }
      return UNIXtime() + 7;
    }

    case getVersion: {
      if( ! WiFi.isConnected() || ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      request->setDebug(false);
      String URL = String(updateURL) + updatePath;
      if( ! request->open("GET", URL.c_str())){
        HTTPrequestFree++;
        break;
      }
      request->setTimeout(10);
      request->setReqHeader("USER_AGENT","IotaWatt");
      request->setReqHeader("X_STA_MAC", WiFi.macAddress().c_str());
      request->setReqHeader("X-UPDATE-CLASS", updateClass);
      request->setReqHeader("X_CURRENT_VERSION", IOTAWATT_VERSION);
      if( ! request->send()){
        request->abort();
        HTTPrequestFree++;
        break;
      }
      state = waitVersion;
      return 1;
    }

    case waitVersion: {
      if(request->readyState() != 4){
        return UNIXtime() + 1;
      }
      HTTPrequestFree++;
      if(request->responseHTTPcode() != 200 || request->available() != 8){
        int responseCode = request->responseHTTPcode();
        delete request;
        request = nullptr;
        if( responseCode != checkResponse){
          log("Updater: Invalid response from server. HTTPcode: %d", responseCode);
        }
        checkResponse = responseCode;
        state = getVersion;
        state = checkAutoUpdate;
        if(responseCode == 403){
          lastVersionCheck = UNIXtime();
        }
        return UNIXtime() + 11 ;
      }
      checkResponse = 0;
      updateVersion = request->responseText();
      delete request;
      request = nullptr;
      if(strcmp(updateVersion.c_str(), IOTAWATT_VERSION) == 0){
        if( ! upToDate){
          log("Updater: Auto-update is current for class %s.", updateClass);
          upToDate = true;
        }
        state = checkAutoUpdate;
        return 1;
      }
      log("Updater: Update from %s to %s", IOTAWATT_VERSION, updateVersion.c_str());
      state = createFile;
      return 1;
    }

    case createFile: {
      log("Updater: download %s", updateVersion.c_str());
      deleteRecursive("download");
      if( ! SD.mkdir("download")){
        log("Cannot create download directory");
        state = checkAutoUpdate;
        break;
      }
      String filePath = "download/" + updateVersion + ".bin";
      releaseFile = SD.open(filePath.c_str(), FILE_WRITE);
      if(! releaseFile){
        log("Updater: Cannot create download file.");
        deleteRecursive("download");
        state = checkAutoUpdate;
        break;
      }
      state = download;
      return 1;
    }
      
    case download: {  
      if( ! WiFi.isConnected() || HTTPrequestFree != HTTPrequestMax){
        return UNIXtime() + 1;
      }
      HTTPrequestFree = 0;
      if( ! request){
        request = new asyncHTTPrequest;
      }
      String URL = String(updateURL) + "/firmware/bin/" + updateVersion + ".bin";
      request->setDebug(false);
      request->open("GET", URL.c_str());
      request->setTimeout(5);
      request->onData([](void* arg, asyncHTTPrequest* request, size_t available){
        uint8_t *buf = new uint8_t[500];
        while(request->available()){
          size_t read = request->responseRead(buf, 500);
          releaseFile.write(buf, read);
        }
        delete[] buf;
        });
      request->send();

          // Writing to the SD in async handler can cause problems. If we return
          // and keep sampling, the onData handler could interupt another service
          // in the middle of SDcard work.  So we will go synchronous here and wait
          // for the entire update blob to be transfered and written to SD.
          // Takes about 5-10 seconds.

      while(request->readyState() != 4){
        yield();
      }
      HTTPrequestFree = HTTPrequestMax;
      size_t fileSize = releaseFile.size();
      releaseFile.close();
      if(request->responseHTTPcode() != 200){
        log("Updater: Download failed HTTPcode %s", request->responseHTTPcode());
        delete request;
        request = nullptr;
        deleteRecursive("download");
        state = getVersion;
        break;
      }
      log("Updater: Release downloaded %dms, size %d", request->elapsedTime(), fileSize);
      delete request;
      request = nullptr;
      state = install;
      return 1;
    }

    case install: {
      if(unpackUpdate(updateVersion)){
        if(installUpdate(updateVersion)){
          log ("Firmware updated, restarting.");
          delay(500);
          ESP.restart();
        }
      }
      state = checkAutoUpdate;
    }
  }
  return UNIXtime() + 1;
}

/**************************************************************************************************
 * bool unpackUpdate(String version)
 * 
 * unpack the release blob to create a directory of support files to be
 * installed during the next restart, as well as the firmware binary
 * file with md-5 appendage to be installed before restart.
 * 
 * After unpacking, the signature on the file is verified using the IoTaWatt public key.
 * Only release files from IotaWatt.com can be verified because the private-key is needed to
 * sign with the digital signature.
 * 
 *************************************************************************************************/

bool unpackUpdate(String version){

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

  bool binaryFound = false;
  SHA256 sha256;
  MD5Builder md5;

        // open the release file and setup to sha256

  String filePath = "download/" + version + ".bin";
  File releaseFile = SD.open((char*)filePath.c_str(), FILE_READ);
  if(! releaseFile){
    return false;
  }
  int signatureSize = 64;
  uint32_t binarySize = releaseFile.size() - signatureSize;
  sha256.reset();

        // Read and verify the header

  if(true){
    releaseFile.read(headers.header,sizeof(headers.updtHeader));
    sha256.update(headers.header,sizeof(headers.updtHeader));
    binarySize -= sizeof(headers.updtHeader);
    if((memcmp(headers.updtHeader.IotaWatt, "IotaWatt", 8) != 0) ||
       (memcmp(headers.updtHeader.release, version.c_str(), 8) != 0)) {
      log("Update file header invalid. %s %s",headers.updtHeader.IotaWatt,headers.updtHeader.release);
      releaseFile.close();
      return false;
    }
  }
        // Create the local update directory.

  deleteRecursive(String(version));
  if( ! SD.mkdir(version.c_str())){
    log("Cannot create update directory");
    releaseFile.close();
    return false;
  }
        // Read the update blob and create the various files.

  while(binarySize){
    releaseFile.read(headers.header,sizeof(headers.fileHeader));
    sha256.update(headers.header,sizeof(headers.fileHeader));
    binarySize -= sizeof(headers.fileHeader);
    if(memcmp(headers.fileHeader.file,"FILE",4) != 0) {
      log("Update file format error.");
      releaseFile.close();
      return false;
    }
    String filename = String(headers.fileHeader.name);
    bool iotawattBin = filename.equalsIgnoreCase("iotawatt.bin");
    if(iotawattBin){
      binaryFound = true;
      md5.begin();
    }
    String filePath = version + "/" + headers.fileHeader.name;
    uint32_t fileSize = headers.fileHeader.len;
    File outFile = SD.open((char*)filePath.c_str(), FILE_WRITE);
    if( ! outFile){
      log("Update: unable to create file: %s", filePath.c_str());
      releaseFile.close();
      return false;
    }
    int buffSize = 512;
    uint8_t* buff = new uint8_t [buffSize];
    while(fileSize){
      int chunk = MIN(fileSize, buffSize);
      if(chunk % 8) chunk += 8 - chunk % 8;
      releaseFile.read(buff, chunk);
      sha256.update(buff, chunk);
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
  sha256.finalize(sha,32);
  if(releaseFile.available() != 64){
    log("Updater: Update rejected, no signature.");
    releaseFile.close();
    return false;
  }
  releaseFile.read(signature, 64);
  releaseFile.close();
  
  uint8_t* key = new uint8_t[32];
  memcpy_P(key, publicKey, 32);
  if(! Ed25519::verify(signature, key, sha, 32)){
    log("Updater: Signature does not verify.");
    delete[] key;
    return false;
  }
  delete[] key;
  log("Updater: Update downloaded and signature verified");
  return binaryFound;
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
    log("Updater: update end failed. %d", update.getError());
    return false; 
  }
  SD.remove((char*)inPath.c_str());
  log("Updater: firmware upgraded to version %s", version.c_str());
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
  log("Updater: Installing update files for version %s", version.c_str());
  int buffSize = 512;
  uint8_t* buff = new uint8_t [buffSize];
  File inFile;
  while(inFile = updtDir.openNextFile()){
    log("Updater: Installing %s", inFile.name());
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
  log("Updater: Installation complete.");
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



