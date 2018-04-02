#include "IotaWatt.h"

bool   unpackUpdate(String updateVersion);

/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock) {
  enum states {initialize, getVersion, waitVersion, download, waitDownload, install};
  static states state = initialize;
  static asyncHTTPrequest* request;
  static String updateVersion;
  static File releaseFile;

  if( ! WiFi.isConnected()){
    return UNIXtime() + 1;
  }

  switch(state){

    case initialize: {
      msgLog(F("Updater: started."));
      state = getVersion;
      return 1;
    }

    case getVersion: {
      if(updateClass == "NONE"){
        break;
      }
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }
      if( ! HTTPrequestFree){
        return UNIXtime() + 1;
      }
      HTTPrequestFree--;
      request = new asyncHTTPrequest;
      request->setDebug(false);
      if( ! request->open("GET", (updateURL + updatePath).c_str())){
        break;
      }
      request->setTimeout(10);
      request->setReqHeader("USER_AGENT","IotaWatt");
      request->setReqHeader("X_STA_MAC", WiFi.macAddress().c_str());
      request->setReqHeader("X-UPDATE-CLASS", updateClass.c_str());
      request->setReqHeader("X_CURRENT_VERSION", IOTAWATT_VERSION);
      if( ! request->send()){
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
        msgLog(F("checkUpdate: Invalid response from server."));
        delete request;
        state = getVersion;
        break;
      }
      updateVersion = request->responseText();
      delete request;
      if(strcmp(updateVersion.c_str(), IOTAWATT_VERSION) == 0){
        state = getVersion;
        return UNIXtime() + updaterServiceInterval;
      }
      String msg = "Update from " + String(IOTAWATT_VERSION) + " to " + updateVersion;
      msgLog("Updater: ", msg);
      state = download;
      return 1;
    }

    case download: {
      msgLog("Updater: download ", updateVersion);
      deleteRecursive("download");
      if( ! SD.mkdir("download")){
        msgLog(F("Cannot create download directory"));
        state = getVersion;
        break;
      }
      String filePath = "download/" + updateVersion + ".bin";
      releaseFile = SD.open(filePath.c_str(), FILE_WRITE);
      if(! releaseFile){
        msgLog(F("Updater: Cannot create download file."));
        deleteRecursive("download");
        state = getVersion;
        break;
      }
      if( ! WiFi.isConnected()){
        return UNIXtime() + 1;
      }

        // for download, hog all requests.

      if(HTTPrequestFree != HTTPrequestMax){
        return UNIXtime() + 1;
      }
      HTTPrequestFree = 0;
      request = new asyncHTTPrequest;
      String URL = updateURL + "/firmware/bin/" + updateVersion + ".bin";
      request->setDebug(false);
      request->open("GET", URL.c_str());
      request->setTimeout(5);
      request->onData([](void* arg, asyncHTTPrequest* request, size_t available){
        uint8_t *buf = new uint8_t[512];
        while(request->available()){
          size_t read = request->responseRead(buf, 512);
          releaseFile.write(buf, read);
        }
        delete[] buf;
        });
      request->send();
      state = waitDownload;
      return UNIXtime() + 1;
    }

    case waitDownload: {
      if(request->readyState() != 4){
        return 1;
      }
      HTTPrequestFree = HTTPrequestMax;
      size_t fileSize = releaseFile.size();
      releaseFile.close();
      if(request->responseHTTPcode() != 200){
        msgLog("Updater: Download failed HTTPcode ", request->responseHTTPcode());
        delete request;
        deleteRecursive("download");
        state = getVersion;
        break;
      }
      String msg = String(request->elapsedTime()).c_str();
      msg += "ms, size " + String(fileSize);
      msgLog("Updater: Release downloaded ", msg);
      delete request;
      state = install;
      
      return 1;
    }

    case install: {
      if(unpackUpdate(updateVersion)){
        if(installUpdate(updateVersion)){
          msgLog (F("Firmware updated, restarting."));
          delay(500);
          ESP.restart();
        }
      }
      state = getVersion;
    }
  }

  return UNIXtime() + updaterServiceInterval;
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
      msgLog("Update file header invalid.",headers.updtHeader.IotaWatt,headers.updtHeader.release);
      releaseFile.close();
      return false;
    }
  }
        // Create the local update directory.

  deleteRecursive(String(version));
  if( ! SD.mkdir(version.c_str())){
    msgLog(F("Cannot create update directory"));
    releaseFile.close();
    return false;
  }
        // Read the update blob and create the various files.

  while(binarySize){
    releaseFile.read(headers.header,sizeof(headers.fileHeader));
    sha256.update(headers.header,sizeof(headers.fileHeader));
    binarySize -= sizeof(headers.fileHeader);
    if(memcmp(headers.fileHeader.file,"FILE",4) != 0) {
      msgLog(F("Update file format error."));
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
      msgLog("Update: unable to create file: ", filePath);
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
    msgLog(F("Updater: Update rejected, no signature."));
    releaseFile.close();
    return false;
  }
  releaseFile.read(signature, 64);
  releaseFile.close();
  
  if(! Ed25519::verify(signature, publicKey, sha, 32)){
    msgLog(F("Updater: Signature does not verify."));
    return false;
  }
  msgLog(F("Updater: Update downloaded and signature verified"));
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



