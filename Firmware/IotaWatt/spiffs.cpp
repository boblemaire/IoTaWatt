#include "Arduino.h"
#include "FS.h"
#include "spiffs.h"
#include <ArduinoJson.h>

/********************************************************************************************************************
 * 
 *      SPIFFS FS access routines.
 * 
 *      SD and Spiffs are mutually exclusive in the same namespace.
 *      Do all the spiffs work exclusively in this module. It's different enough from
 *      the SD file system that it makes sense to have a layer between anyway.  This way
 *      a directory structure similar to FAT can be emulated somewhat.
 * 
 * ****************************************************************************************************************/

bool spiffsMounted = false;

//*************************************Format the SPIFFS FS ****************************************************
bool spiffsFormat(){
    uint32_t startTime = millis();
    bool created = SPIFFS.format();
    return created;
}
//************************************* Mount the SPIFFS FS ****************************************************
bool spiffsBegin(){
    if(spiffsMounted) return true;
    return spiffsMounted = SPIFFS.begin();
}
//************************************* Write a file to the SPIFFS****************************************************
size_t spiffsWrite(const char* path, String contents, bool append){
    return spiffsWrite(path, (uint8_t*)contents.c_str(), contents.length(), append);
}

size_t spiffsWrite(const char* path, uint8_t* buf, size_t len, bool append){
    if( ! spiffsBegin()) return 0;
    File file = SPIFFS.open(path, append ? "a" : "w");
    if(file){
        size_t written = file.write(buf, len);
        file.close();
        return written;     
    }
    return 0;
}
//************************************* read a file from the SPIFFS ****************************************************
String spiffsRead(const char* path){
    if( ! spiffsBegin()) return String("");
    File file = SPIFFS.open(path, "r");
    String contents;
    if(file){
       contents = file.readString();
       file.close(); 
    }
    return contents;
}
//************************************* remove a file from the SPIFFS ****************************************************
bool spiffsRemove(const char* path){
    if( ! spiffsBegin()) return false;
    Dir dir = SPIFFS.openDir(path);
    while(dir.next()){
        SPIFFS.remove(dir.fileName());
    }
    return true;
}
//************************************* return size of a SPIFFS file ****************************************************
size_t spiffsFileSize(const char* path){
    if(! spiffsFileExists(path)) return 0;
    File file = SPIFFS.open(path,"r");
    size_t size = file.size();
    file.close();
    return size;
}
//************************************* does a SPIFFS path exist? ****************************************************
bool spiffsFileExists(const char* path){
   if( ! spiffsBegin()) return false;
   return SPIFFS.exists(path); 
}
//************************************* return a Json array with pseudo directory elements *******************************
String spiffsDirectory(String path){
    if( ! spiffsBegin()) return String("[]");
    Dir dir = SPIFFS.openDir(path);
    DynamicJsonBuffer jsonBuffer;
    JsonArray& array = jsonBuffer.createArray();
    while(dir.next()){
      String name = dir.fileName().substring(path.length()+1);

            // Skip duplicate "directories"

      if(name.indexOf('/') > 0){                    
        int i = 0;  
        for(; i<array.size(); i++){
            if(name.substring(0,name.indexOf('/')).equals(array[i]["name"].as<char*>())) break; 
        }
        if(i != array.size()) continue;   
      }

            // Add a new entry

      JsonObject& object = jsonBuffer.createObject();
      if(name.indexOf('/') > 0){
        object["type"] = "dir";
        object["name"] = name.substring(0,name.indexOf('/'));
      } else {
        object["type"] = "file";
        object["name"] = name;
      }
      array.add(object);
    }
    String response;
    array.printTo(response);
    return response;
}
