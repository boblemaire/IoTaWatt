#include "IotaWatt.h"

struct phaseTableEntry {
    phaseTableEntry* next;
    char modelHash[8];
    float value;
    phaseTableEntry() {next = nullptr; value = 0.0;}
    ~phaseTableEntry(){delete next;}
  }; 

bool configDevice(const char*);
bool configInputs(const char*);
bool configOutputs(const char*);
void hashFile(uint8_t* sha, File file);
void buildPhaseTable(phaseTableEntry**);
void phaseTableAdd(phaseTableEntry**, JsonArray&);
String old2newScript(JsonArray& script);

boolean getConfig(void){

  DynamicJsonBuffer Json;              
  File ConfigFile;
  String ConfigFileURL = "config.txt";
      
  //************************************** Load and parse Json Config file ************************

  trace(T_CONFIG,0);
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  if(!ConfigFile) {
    log("Config file open failed.");
    return false;
  }
  hashFile(configSHA256, ConfigFile);
  String configSummary = JsonSummary(ConfigFile, 1);
  JsonObject& Config = Json.parseObject(configSummary);
  trace(T_CONFIG,4);
  if (!Config.success()) {
    log("Config file parse failed.");
    return false;
  }
  
  //************************************** Process misc first level stuff **************************
  
  delete[] updateClass;
  updateClass = charstar(Config["update"] | "NONE");

  localTimeDiff = Config["timezone"].as<signed int>() | 0;; 

  if(Config.containsKey("logdays")){ 
    log("Current log overide days: %d", currLog.setDays(Config["logdays"].as<int>()));
  }      

        //************************************ Configure device ***************************

  trace(T_CONFIG,5);
  JsonArray& deviceArray = Config["device"]     ;
  if(deviceArray.success()){
    char* deviceStr = JsonDetail(ConfigFile, deviceArray);
    configDevice(deviceStr);
    delete[] deviceStr;
  }   
        //************************************ Configure input channels ***************************

  trace(T_CONFIG,6);
  JsonArray& inputsArray = Config["inputs"]     ;
  if(inputsArray.success()){
    char* inputsStr = JsonDetail(ConfigFile, inputsArray);
    configInputs(inputsStr);
    delete[] inputsStr;
  }   
        // ************************************ configure output channels *************************
  trace(T_CONFIG,7);
  delete outputs;
  JsonArray& outputsArray = Config["outputs"]     ;
  if(outputsArray.success()){
    char* outputsStr = JsonDetail(ConfigFile, outputsArray);
    configOutputs(outputsStr);
    delete[] outputsStr;
  }   
         // ************************************** configure Emoncms **********************************

  trace(T_CONFIG,8);
  JsonArray& EmonArray = Config["server"];
  if(EmonArray.success()){
    char* EmonStr = JsonDetail(ConfigFile, EmonArray);
    if( ! EmonConfig(EmonStr)){
      log("EmonService: Invalid configuration.");
    }
    delete[] EmonStr;
  }   
  else {
    EmonStop = true;
  }
        // ************************************** configure influxDB **********************************
  trace(T_CONFIG,8);
  JsonArray& influxArray = Config["influxdb"];
  if(influxArray.success()){
    char* influxStr = JsonDetail(ConfigFile, influxArray);
    if( ! influxConfig(influxStr)){
      log("influxService: Invalid configuration.");
    }
    delete[] influxStr;
  }   
  else {
    influxStop = true;
  }
  
  ConfigFile.close();
  trace(T_CONFIG,9);
  return true;
}                                       // End of getConfig


//************************************** configDevice() ********************************************
bool configDevice(const char* JsonStr){
  DynamicJsonBuffer Json;
  JsonObject& device = Json.parseObject(JsonStr);
  if( ! device.success()){
    log("device: Json parse failed");
  }
  delete[] deviceName;
  if(device.containsKey("name")){
    deviceName = charstar(device["name"].as<char*>());
  }
  else {
    deviceName = charstar(F("IotaWatt"));
  }

  int channels = 21;
  if(device.containsKey("version")){
    deviceVersion = device["version"].as<unsigned int>();
    if(deviceVersion < 3){
      log("Device version no longer supported.");
      dropDead();
    }
  } 
  hasRTC = true;
  VrefVolts = 2.5;
  ADC_selectPin[0] = pin_CS_ADC0;
  ADC_selectPin[1] = pin_CS_ADC1;
  channels = 15;
      
  if(device.containsKey("refvolts")){
    VrefVolts = device["refvolts"].as<float>();
  }  
  
          // Build or update the input channels
          
  trace(T_CONFIG,5); 
  if(device.containsKey("channels")){
    channels = MIN(device["channels"].as<unsigned int>(),MAXINPUTS);
  }
  
  if(maxInputs != channels) {
    IotaInputChannel* *newList = new IotaInputChannel*[channels];
    for(int i=0; i<MIN(channels,maxInputs); i++){
      newList[i] = inputChannel[i];
    }
    for(int i=MIN(channels,maxInputs); i<maxInputs; i++){
      delete inputChannel[i];
    }
    for(int i=MIN(channels,maxInputs); i<channels; i++){
      newList[i] = new IotaInputChannel(i);
      String name = "Input(" + String(i) + ")";
      newList[i]->_name = charstar(name);
    }
    delete[] inputChannel;
    inputChannel = newList;
    maxInputs = channels;
  }

        // Override V3 defaults if V2 device

  if(deviceVersion == 2){
    for(int i=0; i<MIN(maxInputs,device["chanaddr"].size()); i++){
      inputChannel[i]->_addr = int(i / 7) * 8 + i % 7;
      inputChannel[i]->_aRef = int(i / 7) * 8 + 7;
    }
  }

        // Override all defaults with user specification
 
  if(device.containsKey("chanaddr")){
    for(int i=0; i<MIN(maxInputs,device["chanaddr"].size()); i++){
      inputChannel[i]->_addr = device["chanaddr"][i].as<unsigned int>();
    }
  }
  
  if(device.containsKey("chanaref")){
    for(int i=0; i<MIN(maxInputs,device["chanaddr"].size()); i++){
      inputChannel[i]->_aRef = device["chanaref"][i].as<unsigned int>();
    }
  }
 

   if(device.containsKey("burden")){
    for(int i=0; i<MIN(maxInputs,device["burden"].size()); i++){
      inputChannel[i]->_burden = device["burden"][i].as<float>();
    }
  }
}

//********************************** configInputs ***********************************************
bool configInputs(const char* JsonStr){
  DynamicJsonBuffer Json;
  JsonVariant JsonInputs = Json.parse(JsonStr);
  if( ! JsonInputs.success()){
    log("inputs: Json parse failed");
  }
  phaseTableEntry* phaseTable = nullptr;
  buildPhaseTable(&phaseTable);
  for(int i=0; i<MIN(maxInputs,JsonInputs.size()); i++) {
    if(JsonInputs[i].is<JsonObject>()){
      JsonObject& input = JsonInputs[i].as<JsonObject&>();
      if(i != input["channel"].as<int>()){
        log("Config input channel mismatch: %d", i);
        continue;
      }
      delete inputChannel[i]->_name;
      inputChannel[i]->_name = charstar(input["name"].as<char*>());
      delete inputChannel[i]->_model;
      inputChannel[i]->_model = charstar(input["model"].as<char*>());
      inputChannel[i]->_calibration = input["cal"].as<float>();
      inputChannel[i]->_phase = input["phase"].as<float>();
      inputChannel[i]->_vphase = input["vphase"].as<float>();
      inputChannel[i]->_vchannel = input.containsKey("vref") ? input["vref"].as<int>() : 0;
      inputChannel[i]->active(true);
      String type = input["type"];
      String _hashName = hashName(input["model"].as<char*>());
      phaseTableEntry* entry = phaseTable;
      while(entry){
        if(memcmp(entry->modelHash, _hashName.c_str(), 8) == 0){
          inputChannel[i]->_phase = entry->value;
          break;
        }
        entry = entry->next;
      }
      if(type == "VT") {
        inputChannel[i]->_type = channelTypeVoltage;
        inputChannel[i]->_vchannel = i;
      }
      else if (type == "CT"){
        inputChannel[i]->_type = channelTypePower;
        inputChannel[i]->_vchannel = input["vchan"].as<int>();
        inputChannel[i]->_signed = input["signed"] | false;
      }  
      else{
        log("unsupported input type: %s", type.c_str());
      } 
    }
    else {
      inputChannel[i]->reset();
    }
  }
  delete phaseTable;
  return true;
}

//********************************** configOutputs ***********************************************
bool configOutputs(const char* JsonStr){
  DynamicJsonBuffer Json;
  JsonArray& outputsArray = Json.parseArray(JsonStr);
  if( ! outputsArray.success()){
    log("outputs: Json parse failed");
    return false;
  }
  outputs = new ScriptSet(outputsArray);
  return true;
} 

//********************************** hashFile() ***********************************************
void hashFile(uint8_t* sha, File file){
  SHA256 sha256;
  int buffSize = 256;
  uint8_t* buff = new uint8_t[buffSize];
  file.seek(0);
  sha256.reset();
  while(file.available()){
    int bytesRead = file.read(buff,MIN(file.available(),buffSize));
    sha256.update(buff, bytesRead); 
  }
  delete[] buff;
  sha256.finalize(sha,32);
  file.seek(0);
}

//********************************** old2newScript ***********************************************
String old2newScript(JsonArray& script){
  String newScript = "";
  for(int i=0; i<script.size(); i++){
    if(script[i]["oper"] == "const"){
      newScript += "#" + script[i]["value"].as<String>();
    }
    else if(script[i]["oper"] == "input"){
      newScript += "@" + script[i]["value"].as<String>();
    }
    else if(script[i]["oper"] == "binop"){
      newScript += script[i]["value"].as<String>(); 
    } 
    else if(script[i]["oper"] == "push"){
      newScript += "(";
    }
    else if(script[i]["oper"] == "pop"){
      newScript += ")";
    }
    else if(script[i]["oper"] == "abs"){
      newScript += "|";
    }   
  }
  return newScript;
}

//********************************** buildPhaseTable ***********************************************
void buildPhaseTable(phaseTableEntry** phaseTable){
  DynamicJsonBuffer Json;              
  File TableFile;
  String TableFileURL = "tables.txt";
  TableFile = SD.open(TableFileURL, FILE_READ);
  if(!TableFile) return;
  JsonObject& Table = Json.parse(TableFile);
  TableFile.close();
  if(!Table.success()) return;
  if(Table.containsKey("VT")) phaseTableAdd(phaseTable, Table["VT"]);
  if(Table.containsKey("CT")) phaseTableAdd(phaseTable, Table["CT"]);
  return;  
}

void phaseTableAdd(phaseTableEntry** phaseTable, JsonArray& table){
  int size = table.size();
  char modelHash[9];
  for(int i=0; i<size; i++){
    if(table[i].is<JsonObject>()) {
      JsonObject& entry = table[i].as<JsonObject&>();
      if(memcmp(entry["model"].as<char*>(),"generic",7) == 0) continue;
      phaseTableEntry* tableEntry = new phaseTableEntry;
      tableEntry->next = *phaseTable;
      *phaseTable = tableEntry;
      tableEntry->value = entry["phase"].as<float>();
      String _hashName = hashName(entry["model"].as<char*>());
      memcpy(tableEntry->modelHash, _hashName.c_str(), 8);
    }
  }
}
