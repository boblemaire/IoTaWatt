#include "IotaWatt.h"

bool configDevice(const char*);
bool configDST(const char* JsonStr);
bool configInputs(const char*);
void configPhaseShift();
bool configMasterPhaseArray();
bool configOutputs(const char*);
void hashFile(uint8_t* sha, File file);
bool exportLogConfig(const char *configObj);

// Handy diagnostic macro to investigate heap requirements.
//#define heap(where) Serial.print(#where); Serial.print(' '); Serial.println(ESP.getFreeHeap());

boolean getConfig(const char* configPath){
  DynamicJsonBuffer Json;              
        
  //************************************** Load and parse Json Config file ************************

  trace(T_CONFIG,0);
  File ConfigFile = SD.open(configPath, FILE_READ);
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

  localTimeDiff = 60.0 * Config["timezone"].as<float>();
    
  if(Config.containsKey("logdays")){ 
    log("Current log overide days: %d", Current_log.setDays(Config["logdays"].as<int>()));
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

        //************************************ Lookup phase shift in tables ***********************

  configMasterPhaseArray();

    // Print the inputs

  // for(int i=0; i<MAXINPUTS; i++){
  //   IotaInputChannel* input = inputChannel[i];
  //   if(input->_active){
  //       Serial.printf("Name %s, Model %s\r\nphase %.2f\r\n", input->_name, input->_model, input->_phase );
  //       if(input->_p60){
  //         Serial.print("p60");
  //         int16_t* array = input->_p60;
  //         while(*(array+1)){
  //           Serial.printf(" (%.2f, %.2f),",(float)*array/100.0, (float)*(array+1)/100.0);
  //           array += 2;
  //         } 
  //         Serial.printf(" (%.2f)\r\n",(float)*array/100.0);
  //       }
  //       if(input->_p50){
  //         Serial.print("p50");
  //         int16_t* array = input->_p50;
  //         while(*(array+1)){
  //           Serial.printf(" (%.2f, %.2f),",(float)*array/100.0, (float)*(array+1)/100.0);
  //           array += 2;
  //         } 
  //         Serial.printf(" (%.2f)\r\n",(float)*array/100.0);
  //       }
  //   }
  // }
    

        // ************************************ configure output channels *************************

  {      
    trace(T_CONFIG,8);
    delete outputs;
    outputs = nullptr;
    JsonArray& outputsArray = Config["outputs"];
    char* outputsStr;
    if(outputsArray.success()){
      outputsStr = JsonDetail(ConfigFile, outputsArray);
    } else {
      outputsStr = charstar("[]");
    }
    configOutputs(outputsStr);
    delete[] outputsStr;

  }

         // ************************************** configure Emoncms **********************************

  {
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
  }

        // ************************************** configure influxDB **********************************

  {
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
  }

        // ************************************** configure influxDB2 **********************************

  {
    trace(T_CONFIG,10);
    JsonArray& influx2Array = Config[F("influxdb2")];
    if(influx2Array.success()){
      char* influx2Str = JsonDetail(ConfigFile, influx2Array);
      if(! influxDB_v2){
        influxDB_v2 = new influxDB_v2_uploader;
      }
      if( ! influxDB_v2->config(influx2Str)){
        log("influxDB_v2: Invalid configuration.");
        influxDB_v2->end();
        influxDB_v2 = nullptr;
      }
      delete[] influx2Str;
    }   
    else if(influxDB_v2){
      influxDB_v2->end();
      influxDB_v2 = nullptr;
    }
  }
      // ************************************** configure PVoutput **********************************

  {
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
    
  }

      // ************************************** configure Export log **********************************

  {
    trace(T_CONFIG,12);
    JsonArray& exportArray = Config[F("exportlog")];
    if(exportArray.success()){
      char* exportStr = JsonDetail(ConfigFile, exportArray);
      if( ! exportLogConfig(exportStr)){
        log("Exportlog: Invalid configuration.");
      }
      delete[] exportStr;
    }   
  }

  ConfigFile.close();
  trace(T_CONFIG,12);
  return true;

}                                       // End of getConfig


//************************************** configDevice() ********************************************
bool configDevice(const char* JsonStr){

  hasRTC = true;
  VrefVolts = 2.5;
  ADC_selectPin[0] = pin_CS_ADC0;
  ADC_selectPin[1] = pin_CS_ADC1;

  int channels = 15; 
  DynamicJsonBuffer Json;
  JsonObject& device = Json.parseObject(JsonStr);
  if( ! device.success()){
    log("device: Json parse failed");
  }
  if(device.containsKey(F("name"))){
    delete[] deviceName;
    deviceName = charstar(device[F("name")].as<char*>());
  }
      
  if(device.containsKey(F("refvolts"))){
    VrefVolts = device[F("refvolts")].as<float>();
  } 
          
  trace(T_CONFIG,5);
  channels = MIN((device[F("channels")].as<unsigned int>() | MAXINPUTS), MAXINPUTS);
  
            // If first time after restart
            // Instantiate the inputs

  if(maxInputs == 0) {
    inputChannel = new IotaInputChannel*[channels];
    for(int i=0; i<channels; i++){
      inputChannel[i] = new IotaInputChannel(i);
      String name = "Input(" + String(i) + ")";
      inputChannel[i]->_name = charstar(name);
    }
    maxInputs = channels;
  }

        // If channels has changed, restart.
        // Too many downsteam dependencies, safer to just restart.

  if(channels != maxInputs){
    log("Channels changing from %d to %d, restarting.", maxInputs, channels);
    delay(500);
    ESP.restart();
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

  delete[] HTTPSproxy;
  HTTPSproxy = nullptr;
  if(device.containsKey(F("httpsproxy"))){
    HTTPSproxy = charstar(device[F("httpsproxy")].as<const char *>());
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

  for(int i=0; i<MIN(maxInputs,JsonInputs.size()); i++) {
    if(JsonInputs[i].is<JsonObject>()){
      JsonObject& input = JsonInputs[i].as<JsonObject&>();
      if(i != input["channel"].as<int>()){
        log("Config input channel mismatch: %d", i);
        continue;
      }
      delete inputChannel[i]->_name;
      inputChannel[i]->_name = charstar(input[F("name")].as<char*>());
      delete inputChannel[i]->_model;
      inputChannel[i]->_model = charstar(input[F("model")].as<char*>());
      // Fix name change that removed (USA) in table
      if(strcmp_P(inputChannel[i]->_model,PSTR("TDC DA-10-09(USA)")) == 0){
        inputChannel[i]->_model[12] = 0;
      }
      inputChannel[i]->_turns = input[F("turns")].as<float>();
      inputChannel[i]->_calibration = input[F("cal")].as<float>();
      if(inputChannel[i]->_turns && inputChannel[i]->_burden){
        inputChannel[i]->_calibration = inputChannel[i]->_turns / inputChannel[i]->_burden;
      }
      inputChannel[i]->_phase = input[F("phase")].as<float>();
      inputChannel[i]->_vphase = input[F("vphase")].as<float>();
      inputChannel[i]->_vchannel = input.containsKey("vref") ? input["vref"].as<int>() : 0;
      inputChannel[i]->active(true);
      String type = input[F("type")];
      inputChannel[i]->_reverse = input[F("reverse")] | false;
      if(type == "VT") {
        inputChannel[i]->_type = channelTypeVoltage; 
        inputChannel[i]->_vchannel = i;
      }
      else if (type == "CT"){
        inputChannel[i]->_type = channelTypePower;
        inputChannel[i]->_vchannel = input[F("vchan")].as<int>();
        inputChannel[i]->_signed = input[F("signed")] | false;
        inputChannel[i]->_double = input[F("double")] | false;
      }  
      else{
        log("unsupported input type: %s", type.c_str());
      }
      if(input.containsKey(F("vmult"))){
        inputChannel[i]->_vmult = input[F("vmult")].as<float>();
      } 
      else {
        inputChannel[i]->_vmult = 1.0;
      }
    }
    else {
      inputChannel[i]->reset();
    }
  }
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
  // outputs->sort([](Script* a, Script* b)->int{
  //   int res = strcmp(a->name(), b->name());
  //   Serial.printf("%s, %s, %d\r\n",a->getUnits(), b->getUnits(), res);
  //   return res;
  // });
  return true;
} 

/********************************** configmasterPhaseArray ********************************************
 * 
 * Phase shift can be specified as an array of shift value steps dependent on Voltage (VT), or
 * current (CT) as well as frequency (50Hz or 60Hz).  These values go into the calculation of
 * net phase shift along with shift attributable to derived reference and timing in the sampling
 * code.  The net is used to shift the voltage array prior to calculating real power.
 * 
 * This function will:
 * 
 * 1) Build a working table of each unique model configured.
 * 2) Search the table.txt file for each model and the existence of either dynamic phase array.
 * 3) allocate a single array to contain each of the unique arrays.
 * 4) Parse each of the unique model/Hz arrays and add to the master array, zero delimited.
 * 5) Set pointers in each iotaInputChannel to their corresponding dynamic phase arrays
 * 
 * **********************************************************************************************/

struct  modelTable {
  char*     model;
  int16_t   p50_pos;
  int16_t   p60_pos;
  int16_t*  p50_ptr;
  int16_t*  p60_ptr;
};

int arraySize(File tableFile){
    int commas = 0;
    char in = tableFile.read();
    while(tableFile.available() && in != ']'){
      if(in == ',') commas++;
      in = tableFile.read();
    }
    return commas + 2;
}

bool arrayCopy(File tableFile, int16_t** entry){
    char buf[100];
    if( ! tableFile.find('[')){
      return false;
    }
    int count = 0;
    int len = tableFile.readBytesUntil(']', buf, 100);
    buf[len] = 0;
    char* pos = buf;
    while(*pos != 0){
      float num = strtof(pos, &pos);
      **entry = (num * 100) + 0.5;
      *entry += 1;
      pos = strchr(pos, ',');
      if(pos == nullptr) break;
      pos++;
    }
    **entry = 0;
    *entry += 1;
    return true;
}

bool configMasterPhaseArray(){

// Allocate a table for unique models

  modelTable table[maxInputs];
  int models = 0;
  for(int i=0; i<maxInputs; i++){
    if(inputChannel[i]->isActive()){
      int t;
      for(t=0; t<models; t++){
        if(inputChannel[i]->_model == table[t].model){
          break;
        }
      }
      if(t == models){
        table[t].model = inputChannel[i]->_model;
        table[t].p50_pos = 0;
        table[t].p60_pos = 0;
        table[t].p50_ptr = nullptr;
        table[t].p60_ptr = nullptr;
        models++;
      }
    }
  }

      // Lookup models in table file.
      // Count total masterPhaseArray entries required

  File tableFile;
  String tableFileURL = "tables.txt";
  tableFile = SD.open(tableFileURL, FILE_READ);
  if(!tableFile) return false;

  int totalEntries = 0;
  for(int t=0; t<models; t++){
    tableFile.seek(0);
    if(tableFile.find(table[t].model)){
      int pos = tableFile.position();
      if(tableFile.findUntil("\"p50\":", 6, "}", 1)){
        table[t].p50_pos = tableFile.position();
        totalEntries += arraySize(tableFile);
      }
      tableFile.seek(pos);
      if(tableFile.findUntil("\"p60\":", 6, "}", 1)){
        table[t].p60_pos = tableFile.position();
        totalEntries += arraySize(tableFile);
      }
    }
  }

        // allocate master array to contain all of the individual arrays.
        // Pointers will be set to subarrays in master;

  delete[] masterPhaseArray;
  masterPhaseArray = new int16_t[totalEntries];
  int16_t* nextArray = masterPhaseArray;

        // Copy arrays into master and save pointers to them.

  for(int t=0; t<models; t++){
    if(table[t].p50_pos){
      table[t].p50_ptr = nextArray;
      tableFile.seek(table[t].p50_pos);
      if( ! arrayCopy(tableFile, &nextArray)){
        table[t].p50_ptr = nullptr;
      }
    }
    if(table[t].p60_pos){
      table[t].p60_ptr = nextArray;
      tableFile.seek(table[t].p60_pos);
      if( ! arrayCopy(tableFile, &nextArray)){
        table[t].p60_ptr = nullptr;
      }
    }
  }

        // Set array pointers in inputs

  for(int i=0; i<maxInputs; i++){
    IotaInputChannel* input = inputChannel[i];
    input->_p50 = nullptr;
    input->_p60 = nullptr;
    if(input->isActive()){
      for(int t=0; t<models; t++){
        if(input->_model == table[t].model){
          input->_p50 = table[t].p50_ptr;
          input->_p60 = table[t].p60_ptr;
          break;
        }
      }
    }
  }
    
  // for(int t=0; t<models; t++){
  //   Serial.printf("%s p50pos %d, p60pos %d\n", table[t].model, table[t].p50_pos, table[t].p60_pos);
  //   int16_t* ptr = table[t].p50_ptr;
  //   if(ptr){
  //     Serial.print("  p50");
  //     while(*ptr){
  //       Serial.print(", ");
  //       Serial.print((float)*ptr / 100.0);
  //       ptr++;
  //     }
  //   }
    
  //   ptr = table[t].p60_ptr;
  //   if(ptr){
  //     Serial.print("  p60");
  //     while(*ptr){
  //       Serial.print(", ");
  //       Serial.print((float)*ptr / 100.0);
  //       ptr++;
  //     }
  //   }
  //   Serial.println();
  // }

  tableFile.close();
  return true;
}