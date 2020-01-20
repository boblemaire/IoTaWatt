 #include "IotaWatt.h"

#define graphDir "graphs"
String hashName(const char* name);

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
  String fileName = hashName(graph["name"].as<char*>());
  filePath += "/" + fileName + ".txt";
  SD.remove(filePath);
  graphFile = SD.open(filePath, FILE_WRITE);
  if( ! graphFile){
    server.send(500, txtPlain_P, F("Couldn't create file."));
    return;
  } 
  graphFile.write(server.arg("data").c_str());
  graphFile.close();
  String response(F("{\"success\":true,\"message\":\"graph saved id:"));
  response += fileName + "\"}";
  server.send(200, appJson_P, response);  
  return;
}

void handleGraphDelete(){
  String filePath = graphDir;
  String fileName = server.arg("id");
  filePath += "/" + fileName;
  SD.remove(filePath);
  server.send(200, txtJson_P, F("{\"success\":true,\"message\":\"deleted\"}"));
}

void handleGraphGetall(){
  File directory = SD.open(graphDir);
  File graphFile;
  if( !(directory) || !(directory.isDirectory()) || !(graphFile = directory.openNextFile())){
    if(directory){
      directory.close(); 
    }
    server.send(200, appJson_P, "[]");
    return;
  }
  String response = "[";
  while(graphFile){
    uint32_t fileSize = graphFile.size();
    char* bufr = new char[fileSize];
    graphFile.read((uint8_t*)bufr, fileSize);
    bufr[fileSize-1] = 0;
    char* ptr = bufr;
    for(int i=0; i<fileSize-9; i++){
      if(memcmp(ptr++, "\"start\"", 7) == 0){
        if( ! response.endsWith("[")) response += ',';
        response += bufr;
        response += ",\"id\":\"";
        response += graphFile.name();
        response += "\"}";
        break;
      }   
    }
    delete[] bufr;
    graphFile.close();
    graphFile = directory.openNextFile();
  }
  response += ']';
  server.send(200, appJson_P, response);
  directory.close();
  return;
}

void handleGraphGetallplus(){
  File directory = SD.open(graphDir);
  File graphFile;
  if( !(directory) || !(directory.isDirectory()) || !(graphFile = directory.openNextFile())){
    if(directory){
      directory.close(); 
    }
    server.send(200, appJson_P, "[]");
    return;
  }
  String response = "[";
  while(graphFile){
    uint32_t fileSize = graphFile.size();
    char* bufr = new char[fileSize+1];
    graphFile.read((uint8_t*)bufr, fileSize);
    bufr[fileSize] = 0;
    DynamicJsonBuffer Json;
    JsonObject& graph = Json.parseObject(bufr);
    if(graph.success()){
      if( ! graph.containsKey(F("start"))){
        if( ! response.endsWith("[")) response += ',';
        response += "{\"name\":\"";
        response += graph[F("name")].as<char*>();
        response += "\",\"id\":\"";
        response += graphFile.name();
        response += "\"}";
      }
    }
    delete[] bufr;
    graphFile.close();
    graphFile = directory.openNextFile();
  }
  response += ']';
  server.send(200, appJson_P, response);
  directory.close();
  return;
}

