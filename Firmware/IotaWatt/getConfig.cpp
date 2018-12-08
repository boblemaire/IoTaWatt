#include "IotaWatt.h"

struct phaseTableEntry {
    phaseTableEntry* next;
    char modelHash[8];
    float value;
    phaseTableEntry() {next = nullptr; value = 0.0;}
    ~phaseTableEntry(){delete next;}
  }; 

bool configDevice(const char*);
bool configDST(const char* JsonStr);
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
    dropDead(LED_NO_CONFIG);
  }
  hashFile(configSHA256, ConfigFile);
  String configSummary = JsonSummary(ConfigFile, 1);
  JsonObject& Config = Json.parseObject(configSummary);
  trace(T_CONFIG,4);
  if (!Config.success()) {
    log("Config file parse failed.");
    dropDead(LED_BAD_CONFIG);
  }
  
  //************************************** Process misc first level stuff **************************
  
  delete[] updateClass;
  updateClass = charstar(Config["update"] | "NONE");

  localTimeDiff = 60.0 * Config["timezone"].as<float>();
    
  if(Config.containsKey("logdays")){ 
    log("Current log overide days: %d", currLog.setDays(Config["logdays"].as<int>()));
  }      

        //************************************ Configure device ***************************

  trace(T_CONFIG,5);
  JsonArray& deviceArray = Config["device"];
  if(deviceArray.success()){
    char* deviceStr = JsonDetail(ConfigFile, deviceArray);
    configDevice(deviceStr);
    delete[] deviceStr;
  }  

        //************************************ Configure DST rule *********************************

  trace(T_CONFIG,6);
  delete timezoneRule;
  timezoneRule = nullptr;
  JsonArray& dstruleArray = Config["dstrule"];
  if(dstruleArray.success()){
    char* dstruleStr = JsonDetail(ConfigFile, dstruleArray);
    configDST(dstruleStr);
    delete[] dstruleStr;
  }  

        //************************************ Configure input channels ***************************

  trace(T_CONFIG,7);
  JsonArray& inputsArray = Config["inputs"];
  if(inputsArray.success()){
    char* inputsStr = JsonDetail(ConfigFile, inputsArray);
    configInputs(inputsStr);
    delete[] inputsStr;
  }  
        // ************************************ configure output channels *************************
  trace(T_CONFIG,8);
  delete outputs;
  JsonArray& outputsArray = Config["outputs"];
  if(outputsArray.success()){
    char* outputsStr = JsonDetail(ConfigFile, outputsArray);
    configOutputs(outputsStr);
    delete[] outputsStr;
  }   
         // ************************************** configure Emoncms **********************************

  trace(T_CONFIG,9);
  char* EmonStr = nullptr;
  JsonArray& EmonArray = Config["emoncms"];
  if(EmonArray.success()){
    EmonStr = JsonDetail(ConfigFile, EmonArray);
  } else // Accept old format ~ 02_03_17
  {      
    JsonArray& serverArray = Config["server"];
    if(serverArray.success()){
      EmonStr = JsonDetail(ConfigFile, serverArray);
    }
  }
  if(EmonStr){
    if( ! EmonConfig(EmonStr)){
      log("EmonService: Invalid configuration.");
    }
    delete[] EmonStr;
  }   
  else {
    EmonStop = true;
  }

        // ************************************** configure influxDB **********************************

  trace(T_CONFIG,10);
  JsonArray& influxArray = Config[F("influxdb")];
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
  
      // ************************************** configure PVoutput **********************************

  trace(T_CONFIG,11);
  JsonArray& PVoutputArray = Config[F("pvoutput")];
  if(PVoutputArray.success()){
    char* PVoutputStr = JsonDetail(ConfigFile, PVoutputArray);
    if(! pvoutput){
      pvoutput = new PVoutput();
    }
    if( ! pvoutput->config(PVoutputStr)){
      log("PVoutput: Invalid configuration."); 
    } 
    delete[] PVoutputStr;
  }   
  else if(pvoutput){
    pvoutput->end();
  } 
  
  ConfigFile.close();
  trace(T_CONFIG,12);
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
  if(device.containsKey(F("name"))){
    deviceName = charstar(device[F("name")].as<char*>());
  }
  else {
    deviceName = charstar(F("IotaWatt"));
  }

  if(device.containsKey(F("version"))){
    deviceVersion = device[F("version")].as<unsigned int>();
    if(deviceVersion < 3){
      log("Device version %d no longer supported.", deviceVersion);
      dropDead();
    }
  } 
  hasRTC = true;
  VrefVolts = 2.5;
  ADC_selectPin[0] = pin_CS_ADC0;
  ADC_selectPin[1] = pin_CS_ADC1;
  int channels = 15;
      
  if(device.containsKey(F("refvolts"))){
    VrefVolts = device[F("refvolts")].as<float>();
  }  
  
          // Build or update the input channels
          
  trace(T_CONFIG,5); 
  if(device.containsKey(F("channels"))){
    channels = MIN(device[F("channels")].as<unsigned int>(),MAXINPUTS);
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

        // Override all defaults with user specification
 
  if(device.containsKey(F("chanaddr"))){
    for(int i=0; i<MIN(maxInputs,device[F("chanaddr")].size()); i++){
      inputChannel[i]->_addr = device[F("chanaddr")][i].as<unsigned int>();
    }
  }
  
  if(device.containsKey(F("chanaref"))){
    for(int i=0; i<MIN(maxInputs,device[F("chanaddr")].size()); i++){
      inputChannel[i]->_aRef = device[F("chanaref")][i].as<unsigned int>();
    }
  }
          // Get burden values.
          // If there is a burden file in the spiffs, use that.
          // else, if there is a burden object in the config, use that.
          // else, the default in the IotaInputChannel constructor prevails.

  const char burdenFileName[] = "/config/device/burden.txt";
  if(spiffsFileExists(burdenFileName)){
    String JsonBurden = spiffsRead(burdenFileName);
    JsonArray& burden = Json.parseArray(JsonBurden);
    for(int i=0; i<MIN(maxInputs,burden.size()); i++){
      inputChannel[i]->_burden = burden[i].as<float>();
    }
  }
  else if(device.containsKey(F("burden"))){
    String JsonBurden;
    device[F("burden")].printTo(JsonBurden);
    spiffsWrite(burdenFileName, JsonBurden);
    for(int i=0; i<MIN(maxInputs,device[F("burden")].size()); i++){
      inputChannel[i]->_burden = device[F("burden")][i].as<float>();
    }
  }
}

//********************************** configure DST *********************************************

bool configDST(const char* JsonStr){
  DynamicJsonBuffer Json;
  JsonVariant dstRule = Json.parse(JsonStr);
  if( ! dstRule.success()){
    log("DST: Json parse failed");
    return false;
  }
  timezoneRule = new tzRule;
  timezoneRule->useUTC = dstRule["utc"].as<bool>();
  timezoneRule->adjMinutes = dstRule["adj"].as<int>();
  timezoneRule->begPeriod.month = dstRule["begin"]["month"].as<int>();
  timezoneRule->begPeriod.weekday = dstRule["begin"]["weekday"].as<int>();
  timezoneRule->begPeriod.instance = dstRule["begin"]["instance"].as<int>();
  timezoneRule->begPeriod.time = dstRule["begin"]["time"].as<int>();
  timezoneRule->endPeriod.month = dstRule["end"]["month"].as<int>();
  timezoneRule->endPeriod.weekday = dstRule["end"]["weekday"].as<int>();
  timezoneRule->endPeriod.instance = dstRule["end"]["instance"].as<int>();
  timezoneRule->endPeriod.time = dstRule["end"]["time"].as<int>();

  /******************************************************************************************************************************
   * 
   *    Activate this code to test a dst rule.  Will run through ten years at one minute intervals and
   *    log changes in offset between local and UTC.
   * 
    
  Serial.printf("timezoneRule: minutes %d, UTC %s\r\n", timezoneRule->adjMinutes, timezoneRule->useUTC ? "true" : "false");
  Serial.printf("  begin: month %d, weekday %d, instance %d, time %d\r\n", timezoneRule->begPeriod.month, 
                 timezoneRule->begPeriod.weekday, timezoneRule->begPeriod.instance, timezoneRule->begPeriod.time);
  Serial.printf("    end: month %d, weekday %d, instance %d, time %d\r\n", timezoneRule->endPeriod.month, 
                 timezoneRule->endPeriod.weekday, timezoneRule->endPeriod.instance, timezoneRule->endPeriod.time);

  uint32_t utime = 1451606400UL;   // 01/01/2016 00:00:00
  uint32_t ltime = utime;
  int32_t adj = 0;
  for(uint32_t days=1; days<=365*10; days++){
    for(uint32_t min=0; min<(60*24); min++){
      utime+=60;
      ltime = UTC2LocalTimeutime);
      if(ltime - utime != adj){
        adj = ltime - utime;
        Serial.printf("UTC %s, local %s, offset %f\r\n", datef(utime).c_str(), datef(ltime).c_str(), float(adj/3600.0));
      }
    }
    yield();
  }              
********************************************************************************************************************************/
} 


//********************************** configInputs ***********************************************
bool configInputs(const char* JsonStr){
  DynamicJsonBuffer Json;
  JsonVariant JsonInputs = Json.parse(JsonStr);
  if( ! JsonInputs.success()){
    log("inputs: Json parse failed");
    return false;
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
      inputChannel[i]->_turns = input["turns"].as<float>();
      inputChannel[i]->_calibration = input["cal"].as<float>();
      if(inputChannel[i]->_turns && inputChannel[i]->_burden){
        inputChannel[i]->_calibration = inputChannel[i]->_turns / inputChannel[i]->_burden;
      }
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
      inputChannel[i]->_reverse = input["reverse"] | false;
      if(type == "VT") {
        inputChannel[i]->_type = channelTypeVoltage;
        inputChannel[i]->_vchannel = i;
      }
      else if (type == "CT"){
        inputChannel[i]->_type = channelTypePower;
        inputChannel[i]->_vchannel = input["vchan"].as<int>();
        inputChannel[i]->_signed = input["signed"] | false;
        inputChannel[i]->_double = input["double"] | false;
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
