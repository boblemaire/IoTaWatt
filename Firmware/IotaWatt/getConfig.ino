boolean getConfig(void)
{
  DynamicJsonBuffer Json;              
  File ConfigFile;
  String ConfigFileURL = "config.txt";
    
  //************************************** Load and parse Json Config file ************************
  
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  if(!ConfigFile) {
    msgLog("Config file open failed.");
    return false;
  }
  int filesize = ConfigFile.size();
  char* ConfigBuffer = new char[condensedJsonSize(ConfigFile)+1];
  ConfigFile.seek(0);
  condenseJson(ConfigBuffer, ConfigFile);
  ConfigFile.close();
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  JsonObject& Config = Json.parseObject(ConfigBuffer);  
  ConfigFile.close();
  
  if (!Config.success()) {
    msgLog("Config file parse failed.");
    delete[] ConfigBuffer;
    return false;
  }
  
  //************************************** Process Config file *********************************
    
  JsonObject& device = Config["device"].as<JsonObject&>();

  if(device.containsKey("name")){
    deviceName = device["name"].asString();
  }
  deviceName.toCharArray(host,9);
  host[8] = 0;
  
  if(Config.containsKey("timezone")){
    localTimeDiff = Config["timezone"].as<signed int>(); 
  }

  int channels = 21;
  if(device.containsKey("version")){
    deviceVersion = device["version"].as<unsigned int>();
    if(deviceVersion == 2){
      hasRTC = true;
      VrefVolts = 1.0;
      ADC_selectPin[0] = 0;
      ADC_selectPin[1] = 16;
      ADC_selectPin[2] = 2;
    }
    else if(deviceVersion == 3){
      hasRTC = true;
      VrefVolts = 2.5;
      ADC_selectPin[0] = 0;
      ADC_selectPin[1] = 2;
      channels = 15;
    }
  }
  
  if(device.containsKey("refvolts")){
    VrefVolts = device["refvolts"].as<float>();
  }  

          // Build or update the input channels
   
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
      newList[i]->_name = "Input(" + String(i) + ")";
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
    
        //************************************ Configure input channels ***************************
  if(Config.containsKey("inputs")){
    configInputs(Config["inputs"]);
  }   
     
        // ************************************ configure output channels *************************

  if(Config.containsKey("outputs")){
    configOutputs(Config["outputs"]);
  }
  
        // Get server type
                                                  
  String serverType = Config["server"]["type"].as<String>();
  serverType.toLowerCase();
      
      //************************************** configure eMonCMS **********************************

  if(serverType.equals("emoncms")) {
    String msg = "server is: Emoncms";
    eMonURL = Config["server"]["url"].asString();
    if(eMonURL.startsWith("http://")) eMonURL = eMonURL.substring(7);
    else if(eMonURL.startsWith("https://")){
      eMonURL = eMonURL.substring(8);
      eMonSecure = true;
    }
    msg += ", url: " + eMonURL;
    apiKey = Config["server"]["apikey"].asString();
    node = Config["server"]["node"].as<String>();
    msg += ", node: " + String(node);
    eMonCMSInterval = Config["server"]["postInterval"].as<int>();
    msg += ", post interval: " + String(eMonCMSInterval);
    String secure = Config["server"]["secure"].asString();
    eMonBulkSend = Config["server"]["bulksend"].as<int>();
    if(eMonBulkSend > 10) eMonBulkSend = 10;
    if(eMonBulkSend <1) eMonBulkSend = 1;    
    if(secure == "secure"){
      eMonSecure = true;
      msg += ", HTTPS protocol";
    }
    if( ! eMonStarted) {
      msgLog(msg);
      NewService(eMonService);
      eMonStarted = true;
      eMonStop = false;
    }
   
  }
  else {
    eMonStop = true;
    if(!serverType.equals("none")){
      msgLog("server type is not supported: ", serverType);
    }
  }

  delete[] ConfigBuffer;
  return true;
}

void configOutputs(JsonArray& JsonOutputs){
  while(outputList.size()){
    IotaOutputChannel* output = (IotaOutputChannel*) outputList.findFirst();
    outputList.remove(output);
    delete output;
  }
  for(int i=0; i<JsonOutputs.size(); i++){
    JsonObject& outputObject = JsonOutputs[i].as<JsonObject&>();
    if(outputObject.containsKey("name") &&
       outputObject.containsKey("units") &&
       outputObject.containsKey("script")) {
           IotaOutputChannel* output = new IotaOutputChannel(outputObject["name"], outputObject["units"], outputObject["script2"]);
           output->_channel = i+100;
           outputList.insertTail(output, output->_name);
       }
  }
  IotaOutputChannel* outputChannel = (IotaOutputChannel*)outputList.findFirst();
  while(outputChannel != NULL){
    outputChannel = (IotaOutputChannel*)outputList.findNext(outputChannel);
  }
}

void configInputs(JsonArray& JsonInputs){
  for(int i=0; i<MIN(maxInputs,JsonInputs.size()); i++) {
    if(JsonInputs[i].is<JsonObject>()){
      JsonObject& input = JsonInputs[i].as<JsonObject&>();
      if(i != input["channel"].as<int>()){
        msgLog("Config input channel mismatch: ", i);
        continue;
      }
      inputChannel[i]->_name = input["name"].as<String>();
      inputChannel[i]->_model = input["model"].as<String>();
      inputChannel[i]->_calibration = input["cal"].as<float>();
      inputChannel[i]->_phase = input["phase"].as<float>();
      inputChannel[i]->_vchannel = input.containsKey("vref") ? input["vref"].as<int>() : 0;
      inputChannel[i]->active(true);
      String type = input["type"]; 
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

