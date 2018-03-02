#include "IotaWatt.h"

struct phaseTableEntry {
    phaseTableEntry* next;
    char modelHash[8];
    float value;
    phaseTableEntry() {next = nullptr; value = 0.0;}
    ~phaseTableEntry(){delete next;}
  }; 

void configInputs(JsonArray& JsonInputs);
void hashFile(uint8_t* sha, File file);
uint32_t condensedJsonSize(File JsonFile);
void condenseJson(char* ConfigBuffer, File JsonFile);
String old2newScript(JsonArray& script);
void getTable(void);
void buildPhaseTable(phaseTableEntry**);
void phaseTableAdd(phaseTableEntry**, JsonArray&);

boolean getConfig(void){

  DynamicJsonBuffer Json;              
  File ConfigFile;
  String ConfigFileURL = "config.txt";
      
  //************************************** Load and parse Json Config file ************************

  trace(T_CONFIG,0);
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  if(!ConfigFile) {
    msgLog(F("Config file open failed."));
    return false;
  }
  hashFile(configSHA256, ConfigFile);
  int filesize = ConfigFile.size();
  trace(T_CONFIG,1);
  char* ConfigBuffer = new char[condensedJsonSize(ConfigFile)+1];
  trace(T_CONFIG,2);
  ConfigFile.seek(0);
  condenseJson(ConfigBuffer, ConfigFile);
  trace(T_CONFIG,3);
  ConfigFile.close();
  // ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  JsonObject& Config = Json.parseObject(ConfigBuffer);  
  // ConfigFile.close();
  trace(T_CONFIG,4);
  if (!Config.success()) {
    msgLog(F("Config file parse failed."));
    delete[] ConfigBuffer;
    return false;
  }
  
  //************************************** Process Config file *********************************
    
  JsonObject& device = Config["device"].as<JsonObject&>();

  if(device.containsKey("name")){
    deviceName = device["name"].as<String>();
    host = deviceName;
  }
  
  if(Config.containsKey("timezone")){
    localTimeDiff = Config["timezone"].as<signed int>(); 
  }

  if(Config.containsKey("update")){
    updateClass = Config["update"].as<String>();
  }

  int channels = 21;
  if(device.containsKey("version")){
    deviceVersion = device["version"].as<unsigned int>();
    if(deviceVersion < 3){
      msgLog(F("Device version no longer supported."));
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

        //************************************ Check for dataLog overide **************************

  if(Config.containsKey("logdays")){ 
    msgLog("Current log overide days: ", currLog.setDays(Config["logdays"].as<int>()));
  }      
    
        //************************************ Configure input channels ***************************

  trace(T_CONFIG,6);      
  if(Config.containsKey("inputs")){
    configInputs(Config["inputs"]);
  }   
     
        // ************************************ configure output channels *************************

  trace(T_CONFIG,7);
  delete outputs;
  JsonVariant var = Config["outputs"];
  if(var.success()){
    outputs = new ScriptSet(var.as<JsonArray>()); 
  }
      
        // Get server type
                                                  
  String serverType = Config["server"]["type"].as<String>();
  serverType.toLowerCase();
  
      // ************************************** configure EmonCMS **********************************

  trace(T_CONFIG,8);
  if(serverType.equals("emoncms")) {
    if(influxStarted) influxStop = true;
    SD.remove((char *)influxPostLogFile.c_str());
    EmonURL = Config["server"]["url"].as<String>();
    if(EmonURL.startsWith("http://")) EmonURL = EmonURL.substring(7);
    else if(EmonURL.startsWith("https://")){
      EmonURL = EmonURL.substring(8);
    }
    EmonURI = "";
    if(EmonURL.indexOf("/") > 0){
      EmonURI = EmonURL.substring(EmonURL.indexOf("/"));
      EmonURL.remove(EmonURL.indexOf("/"));
    }
    if(EmonURL.indexOf(":") > 0){
      EmonPort = EmonURL.substring(EmonURL.indexOf(":")+1).toInt();
      EmonURL.remove(EmonURL.indexOf(":"));
    }
    apiKey = Config["server"]["apikey"].as<String>();
    node = Config["server"]["node"].as<String>();
    EmonCMSInterval = Config["server"]["postInterval"].as<int>();
    EmonBulkSend = Config["server"]["bulksend"].as<int>();
    if(EmonBulkSend > 10) EmonBulkSend = 10;
    if(EmonBulkSend <1) EmonBulkSend = 1;
    EmonUsername = Config["server"]["userid"].as<String>();
    EmonSendMode EmonPrevSend = EmonSend;
    EmonSend = EmonSendGET;
    if(EmonUsername != "")EmonSend = EmonSendPOSTsecure;
    if(EmonPrevSend != EmonSend) EmonInitialize = true;  
    
    #define hex2bin(x) (x<='9' ? (x - '0') : (x - 'a') + 10)
    apiKey.toLowerCase();
    for(int i=0; i<16; i++){
      cryptoKey[i] = hex2bin(apiKey[i*2]) * 16 + hex2bin(apiKey[i*2+1]); 
    }
    delete emonOutputs;
    JsonVariant var = Config["server"]["outputs"];
    if(var.success()){
      emonOutputs = new ScriptSet(var.as<JsonArray>());
      Script* script = emonOutputs->first();
      int index = 0;
      while(script){
        if(String(script->name()).toInt() <= index){
          delete emonOutputs;
          break;
        }
        else {
          index = String(script->name()).toInt();
        }
        script = script->next();
      }
    }
    
    if( ! EmonStarted) {
      NewService(EmonService);
      EmonStarted = true;
      EmonStop = false;
    } 
  }
  
        // ************************************** configure influxDB **********************************

  else if(serverType.equals("influxdb")) {
    if(EmonStarted) EmonStop = true;
    SD.remove((char *)EmonPostLogFile.c_str());
    influxURL = Config["server"]["url"].as<String>();
    if(influxURL.startsWith("http")){
      influxURL.remove(0,4);
      if(influxURL.startsWith("s"))influxURL.remove(0,1);
      if(influxURL.startsWith(":"))influxURL.remove(0,1);
      while(influxURL.startsWith("/")) influxURL.remove(0,1);
    }
   
    if(influxURL.indexOf(":") > 0){
      influxPort = influxURL.substring(influxURL.indexOf(":")+1).toInt();
      influxURL.remove(influxURL.indexOf(":"));
    }
    influxDataBase = Config["server"]["database"].as<String>();
    influxDBInterval = Config["server"]["postInterval"].as<int>();
    influxBulkSend = Config["server"]["bulksend"].as<int>();
    if(influxBulkSend > 10) influxBulkSend = 10;
    if(influxBulkSend <1) influxBulkSend = 1;

    delete influxOutputs;
    JsonVariant var = Config["server"]["outputs"];
    if(var.success()){
      influxOutputs = new ScriptSet(var.as<JsonArray>()); 
    }
    if( ! influxStarted) {
      NewService(influxService);
      influxStarted = true;
      influxStop = false;
    }
  }

  else if(serverType.equals("none")){
    EmonStop = true;
    influxStop = true;
    SD.remove((char *)influxPostLogFile.c_str());
    SD.remove((char *)EmonPostLogFile.c_str());
  }
  
  else {
    EmonStop = true;
    influxStop = true;
    if(!serverType.equals("none")){
      msgLog("server type is not supported: ", serverType);
    }
  }

  trace(T_CONFIG,9);
  delete[] ConfigBuffer;
  return true;
}

void configInputs(JsonArray& JsonInputs){
  phaseTableEntry* phaseTable = nullptr;
  buildPhaseTable(&phaseTable);
  for(int i=0; i<MIN(maxInputs,JsonInputs.size()); i++) {
    if(JsonInputs[i].is<JsonObject>()){
      JsonObject& input = JsonInputs[i].as<JsonObject&>();
      if(i != input["channel"].as<int>()){
        msgLog("Config input channel mismatch: ", i);
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
      }
      else if (type == "CT"){
        inputChannel[i]->_type = channelTypePower;
        inputChannel[i]->_vchannel = input["vchan"].as<int>();     
        if(input.containsKey("signed")){
          inputChannel[i]->_signed = true;
        }
      }  
      else msgLog("unsupported input type: ", type);
    }
    else {
      inputChannel[i]->reset();
    }
  }
  delete phaseTable;
}

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

uint32_t condensedJsonSize(File JsonFile){
  uint32_t size = 0;
  bool inQuote = false;
  char Json;
  while(JsonFile.available()){
    Json = JsonFile.read();
    if(inQuote || (Json != ' ' && Json != 9 && Json != 10 && Json != 13)){
      size++;
    }
    if(Json == '"') inQuote = !inQuote;
  }
  return size;
}

void condenseJson(char* ConfigBuffer, File JsonFile){
  char* buffer = ConfigBuffer;
  bool inQuote = false;
  char Json;
   while(JsonFile.available()){
    Json = JsonFile.read();
    if(inQuote || (Json != ' ' && Json != 9 && Json != 10 && Json != 13)){
      *buffer++ = Json;
    }
    if(Json == '"') inQuote = !inQuote;
  }
}

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

char* charstar(const char* str){
  char* ptr = new char[strlen(str)+1];
  strcpy(ptr, str);
  return ptr;
}

char* charstar(String str){
  return charstar(str.c_str());
}

char* charstar(const char str){
  char* ptr = new char[2];
  ptr[0] = str;
  ptr[1] = 0;
  return ptr;
}