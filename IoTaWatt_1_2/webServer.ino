/*
  This WebServer code is incorporated with little modification
  to facilitate editing the Json configuration files. Currently used
  only to read and write those files, the capability would also support
  querying the log and reading a text event log.

  Small parts of the code are imbedded elsewhere as needed in the preamble and Init sections.
  and "handleClient()" is invoked in Loop.
  
  The author's copyright and license info follows:

  --------------------------------------------------------------------------------------------------
 
  SDWebServer - Example WebServer with SD Card backend for esp8266

  Copyright (c) 2015  . All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Have a FAT Formatted SD Card connected to the SPI port of the ESP8266
  The web root is the SD Card root folder
  File extensions with more than 3 charecters are not supported by the SD Library
  File Names longer than 8 charecters will be truncated by the SD library, so keep filenames shorter
  index.htm is the default index (works on subfolders as well)

  upload the contents of SdRoot to the root of the SDcard and access the editor by going to http://esp8266sd.local/edit

  ----------------------------------------------------------------------------------------------------------------

*/

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload(){
  // if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  Serial.print("print dir");
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
}



void handleNotFound(){
  if(hasSD && loadFromSdCard(server.uri())) return;
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DBG_OUTPUT_PORT.print(message);
}

/************************************************************************************************
 * 
 * Following handlers added to WebServer for IoTaWatt specific requests
 * 
 **********************************************************************************************/

void handleStatus(){  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  // WiFiClient client = server.client(); 
  String message = "{";
  boolean firstArg = true;

  if(server.hasArg("stats")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    message += "\"stats\":{\"cyclerate\":" + String(samplesPerCycle,0);
    message += ",\"chanrate\":" + String(cycleSampleRate,1);
    message += "}";
    statServiceInterval = 2;
  }
  
  if(server.hasArg("channels")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    message += "\"channels\":[";
    boolean firstChan = true;
    for(int i=0; i<channels; i++){
      if(channelType[i] == channelTypeUndefined) continue;
      if(!firstChan){
        message += ","; 
      }
      firstChan = false;
      message += "{\"channel\":" + String(i);

      if(channelType[i] == channelTypeVoltage){
        message += ",\"Vrms\":" + String(statBuckets[i].volts,1);
        message += ",\"Hz\":" + String(statBuckets[i].hz,1);
      }
      else if(channelType[i] == channelTypePower){
        message += ",\"Watts\":" + String(statBuckets[i].watts,0);
        message += ",\"Irms\":" + String(statBuckets[i].amps,3);
        if(statBuckets[i].watts > 10){
          message += ",\"Pf\":" + String(statBuckets[i].watts/(statBuckets[i].amps*statBuckets[Vchannel[i]].volts),2);
        }       
      }
      message += "}";
    }
    message += "]";
  }

  if(server.hasArg("voltage")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    int Vchan = server.arg("channel").toInt();
    message += "\"voltage\":" + String(buckets[Vchan].volts,1);
  }

  if(server.hasArg("calcomplete")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    if(calibrationMode) {
      message += "\"calcomplete\":\"no\"";
    }
    else {
      message += "\"calcomplete\":\"yes\"";
      message += ",\"cal\":" + String(calibrationCal,4);
      message += ",\"phase\":" + String(calibrationPhase,2);
    }
  }
  
  message += "}";
  // Serial.println(message);
  server.sendContent(message);
}

void handleCommand(){

  if(server.hasArg("restart")) {
    server.send(200, "text/plain", "ok");
    DBG_OUTPUT_PORT.print("Restart command received.");
    delay(500);
    ESP.restart();
  } 
  if(server.hasArg("calibrate")){
    Serial.print("calibrate:");
    Serial.print(server.arg("calibrate").toInt());
    Serial.print(", ref:");
    Serial.print(server.arg("ref").toInt());
    Serial.println();
    server.send(200, "text/plain", "ok");
    calibrationVchan = server.arg("calibrate").toInt();
    calibrationRefChan = server.arg("ref").toInt();
    calibrationMode = true;
  }
}


