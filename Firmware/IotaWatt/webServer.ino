/*
  This WebServer code is incorporated with very little modification.
  Very simple yet powerful.

  A few new handlers were added at the end, and appropriate server.on
  declarations define them in the Setup code.

  The server supports reading and writing files to/from the SDcard.
  It also serves up HTML files to a browser.  The configuration utility is
  index.htm in the root directory of the SD card.
  The server also came with a great editor utility which, if placed on the SD,
  can be used to edit the web pages or any other text file on the SDcard.
  
  Small parts of the code are imbedded elsewhere as needed in the preamble and Setup sections.
  and "handleClient()" is invoked as often as practical in Loop to keep it running.
  
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
  if(path == "/edit" ||
     path == "/graph"){
      path += ".htm";
     }

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js"))  dataType = "application/javascript";
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

  if(server.hasArg("textpos")){
    sendMsgFile(dataFile, server.arg("textpos").toInt());
    return true;
  }

  else {
    if (server.streamFile(dataFile, dataType) != dataFile.size()) {
      msgLog("Server: Sent less data than expected!");
    }
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
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  DynamicJsonBuffer jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  dir.rewindDirectory();
  File entry;
  while(entry = dir.openNextFile()){
    JsonObject& object = jsonBuffer.createObject();
    object["type"] = (entry.isDirectory()) ? "dir" : "file";
    object["name"] = String(entry.name());
    array.add(object);
    entry.close();
  }  
  String response = "";
  array.printTo(response);
  server.send(200, "application/json", response);
  dir.close();
}

void handleNotFound(){
  String serverURI = server.uri();
  if(serverURI.startsWith("//")) serverURI.remove(0,1);   // fix eMonCMS graph bug
  if(serverURI.startsWith("/feed/list")){
    handleGetFeedList();
    return;
  }
  if(serverURI == "/graph/getall"){
    handleGraphGetall();
    return;
  }
  if(serverURI.startsWith("/feed/data")){
    Serial.println(serverURI);
    serverAvailable = false;
    NewService(handleGetFeedData);
    return;
  }
  if(loadFromSdCard(server.uri())) return;
  String message = "Not found: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += ", URI: ";
  message += server.uri();
  server.send(404, "text/plain", message);
  Serial.println(message);
}

/************************************************************************************************
 * 
 * Following handlers added to WebServer for IotaWatt specific requests
 * 
 **********************************************************************************************/

void handleStatus(){ 
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject(); 
  
  if(server.hasArg("stats")){
    JsonObject& stats = jsonBuffer.createObject();
    stats.set("cyclerate", samplesPerCycle,0);
    stats.set("chanrate",cycleSampleRate,1);
    stats.set("runseconds", UNIXtime()-programStartTime);
    stats.set("stack",ESP.getFreeHeap());
    stats.set("version",IOTAWATT_VERSION);
    stats.set("frequency",frequency);
    root.set("stats",stats);
  }
  
  if(server.hasArg("inputs")){
    JsonArray& channelArray = jsonBuffer.createArray();
    for(int i=0; i<maxInputs; i++){
      if(inputChannel[i]->isActive()){
        JsonObject& channelObject = jsonBuffer.createObject();
        channelObject.set("channel",inputChannel[i]->_channel);
        if(inputChannel[i]->_type == channelTypeVoltage){
          channelObject.set("Vrms",statBucket[i].volts,1);
          channelObject.set("Hz",statBucket[i].Hz,1);
        }
        else if(inputChannel[i]->_type == channelTypePower){
          if(statBucket[i].watts < 0 && statBucket[i].watts > -.5) statBucket[i].watts = 0;
          channelObject.set("Watts",String(statBucket[i].watts,0));
          channelObject.set("Irms",String(statBucket[i].amps,3));
          if(statBucket[i].watts > 10){
            channelObject.set("Pf",statBucket[i].watts/(statBucket[i].amps*statBucket[inputChannel[i]->_vchannel].volts),4);
          } 
          if(inputChannel[i]->_reversed){
            channelObject.set("reversed","true");
          }
        }
        channelArray.add(channelObject);
      }
    }
    root["inputs"] = channelArray;
  }

  if(server.hasArg("outputs")){
    JsonArray& outputArray = jsonBuffer.createArray();
    IotaOutputChannel* _output = (IotaOutputChannel*)outputList.findFirst();
    while(_output){
      JsonObject& channelObject = jsonBuffer.createObject();
      channelObject.set("name",_output->_name);
      channelObject.set("units",_output->_units);
      double value = _output->runScript([](int i)->double {return statBucket[i].value1;});
      channelObject.set("value",value);
      channelObject.set("Watts",value);  // depricated 3.04
      outputArray.add(channelObject);
      _output = (IotaOutputChannel*)outputList.findNext(_output);
    }
    root["outputs"] = outputArray;
  }

  if(server.hasArg("voltage")){
    int Vchan = server.arg("channel").toInt();
    root.set("voltage", statBucket[Vchan].volts,1);
  }
  String response = "";
  root.printTo(response);
  server.send(200, "text/json", response);  
}

void handleVcal(){
  if( ! (server.hasArg("channel") && server.hasArg("cal"))){
    server.send(400, "text/json", "Missing parameters");
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  int channel = server.arg("channel").toInt();
  float Vrms = sampleVoltage(channel, server.arg("cal").toFloat());
  root.set("vrms",Vrms,1);
  String response = "";
  root.printTo(response);
  server.send(200, "text/json", response);  
}

void handleCommand(){

  if(server.hasArg("restart")) {
    server.send(200, "text/plain", "ok");
    msgLog("Restart command received.");
    delay(500);
    ESP.restart();
  }
  if(server.hasArg("vtphase")){
    uint16_t chan = server.arg("vtphase").toInt();
    int refChan = 0;
    if(server.hasArg("refchan")){
      refChan = server.arg("refchan").toInt();
    }
    uint16_t shift = 0;
    if(server.hasArg("shift")){
      shift = server.arg("shift").toInt();
    }
    String response = "Calculated shift: " + String(samplePhase(refChan, chan, shift),2);
    server.send(200, "text/plain", response);
    return; 
  }
  if(server.hasArg("sample")){
    uint16_t chan = server.arg("sample").toInt();
    samplePower(chan,0);
    String response = String(samples) + "\n\r";
    for(int i=0; i<samples; i++){
      response += String(Vsample[i]) + "," + String(Isample[i]) + "\n";
    }
    server.send(200, "text/plain", response);
    return; 
  }
  server.send(400, "text/json", "Unrecognized request");
}

void handleGetFeedList(){ 
  DynamicJsonBuffer jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  for(int i=0; i<maxInputs; i++){
    if(inputChannel[i]->isActive()){
      if(inputChannel[i]->_type == channelTypeVoltage){
        JsonObject& voltage = jsonBuffer.createObject();
        voltage["id"] = String(inputChannel[i]->_channel*10+QUERY_VOLTAGE);
        voltage["tag"] = "Voltage";
        voltage["name"] = inputChannel[i]->_name;
        array.add(voltage);
      } 
      else
        if(inputChannel[i]->_type == channelTypePower){
        JsonObject& power = jsonBuffer.createObject();
        power["id"] = String(inputChannel[i]->_channel*10+QUERY_POWER);
        power["tag"] = "Power";
        power["name"] = inputChannel[i]->_name;
        array.add(power);
        JsonObject& energy = jsonBuffer.createObject();
        energy["id"] = String(inputChannel[i]->_channel*10+QUERY_ENERGY);
        energy["tag"] = "Energy";
        energy["name"] = inputChannel[i]->_name;
        array.add(energy);
      }
    }
  }
  
  IotaOutputChannel* _output = (IotaOutputChannel*)outputList.findFirst();
  while(_output){
    JsonObject& power = jsonBuffer.createObject();
    power["id"] = String(_output->_channel*10+QUERY_POWER);
    power["tag"] = "Power";
    power["name"] = _output->_name;
    array.add(power);
    _output = (IotaOutputChannel*)outputList.findNext(_output);
  }
  
  String response = "";
  array.printTo(response);
  server.send(200, "application/json", response);
}

void handleGraphGetall(){                   // Stub to appease eMonCMS graph app
  return;
  server.send(200, "ok", "{}");
}

// Had to roll our own streamFile function so we can set the actual partial
// file length rather than the total file length.  Safari won't work otherwise.
// No big deal.  BTW/ This instance of Client.send is depricated in the newer
// ESP8266WiFiClient, so probably change at some point. (Remove buffer size parameter).

void sendMsgFile(File &dataFile, int32_t relPos){
    int32_t absPos = relPos;
    if(relPos < 0) absPos = dataFile.size() + relPos;
    dataFile.seek(absPos);
    while(dataFile.available()){
      if(dataFile.read() == '\n') break;
    }
    absPos = dataFile.position();
    server.setContentLength(dataFile.size() - absPos);
    server.send(200, "text/plain", "");
    WiFiClient _client = server.client();
    _client.write(dataFile, 1460);
}

void handleGetConfig(){
  if(server.hasArg("update")){
    if(server.arg("update") == "restart"){
      server.send(200, "text/plain", "OK");
      msgLog("Restart command received.");
      delay(500);
      ESP.restart();
    }
    else if(server.arg("update") == "reload"){
      getConfig(); 
      server.send(200, "text/plain", "OK");
      return;  
    }
  }
  server.send(400, "text/plain", "Bad Request.");
}

