#include "IotaWatt.h"

#define graphDir "graphs"
String hashFileName(const char* name);

void handleGraphCreate(){
  File graphFile = SD.open(graphDir);
  if(graphFile){
    if( ! graphFile.isDirectory()){
      graphFile.close();
      SD.remove(graphDir);
      SD.mkdir(graphDir);
    }
  }
  else {
    SD.mkdir(graphDir);
  }
  
  trace(T_uploadGraph,11);
  DynamicJsonBuffer Json;
  JsonObject& graph = Json.parseObject(server.arg("data"));
  String filePath = graphDir;
  String fileName = hashFileName(graph["name"].as<char*>());
  filePath += "/" + fileName + ".txt";
  SD.remove(filePath);
  graphFile = SD.open(filePath, FILE_WRITE);
  if( ! graphFile){
    returnFail("Couldn't create file.");
    return;
  }
  graphFile.write(server.arg("data").c_str());
  graphFile.close();
  String response = "{\"success\":true,\"message\":\"graph saved id:" + fileName + "\"}";
  server.send(200, "text/json", response);  
  return;
}

void handleGraphDelete(){
  String filePath = graphDir;
  String fileName = server.arg("id");
  filePath += "/" + fileName + ".txt";
  SD.remove(filePath);
  server.send(200, "text/json", "{\"success\":true,\"message\":\"deleted\"}");
}

void handleGraphGetall(){
  File directory = SD.open(graphDir);
  if(! (directory or directory.isDirectory())){
    server.send(200, "application/json", String("[]"));
    return;
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
  server.sendContent("[");
  bool first = true;
  File graphFile;
  uint8_t* bufr;
  while(graphFile = directory.openNextFile()){
    if( ! first){
      server.sendContent(",");
    }
    first = false;
    uint32_t fileSize = graphFile.size();
    bufr = new uint8_t[fileSize+17];
    graphFile.read(bufr, fileSize);
    strcpy((char*)bufr+fileSize-1,",\"id\":\"filename\"}");
    memcpy((char*)bufr+fileSize+6, graphFile.name(),8);
    server.sendContent(String((char*)bufr));
    graphFile.close();
    delete[] bufr;
  }
  server.sendContent("]");
  server.sendContent("");
  directory.close();
  return;
}

String hashFileName(const char* name){
  uint8_t hash[6];
  sha256.reset();
  sha256.update(name, strlen(name));
  sha256.finalize(hash, 6);
  return base64encode(hash, 6);
}