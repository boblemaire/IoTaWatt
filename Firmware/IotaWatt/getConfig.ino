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

  if(Config.containsKey("update")){
    updateClass = Config["update"].as<String>();
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

  delete outputs;
  JsonVariant var = Config["outputs"];
  if(var.success()){
    outputs = new ScriptSet(var.as<JsonArray>()); 
  }
      
        // Get server type
                                                  
  String serverType = Config["server"]["type"].as<String>();
  serverType.toLowerCase();
  
      // ************************************** configure EmonCMS **********************************

  if(serverType.equals("emoncms")) {
    EmonURL = Config["server"]["url"].asString();
    if(EmonURL.startsWith("http://")) EmonURL = EmonURL.substring(7);
    else if(EmonURL.startsWith("https://")){
      EmonURL = EmonURL.substring(8);
    }
    EmonURI = "";
    if(EmonURL.indexOf("/") > 0){
      EmonURI = EmonURL.substring(EmonURL.indexOf("/"));
      EmonURL.remove(EmonURL.indexOf("/"));
    }
    apiKey = Config["server"]["apikey"].asString();
    node = Config["server"]["node"].as<String>();
    EmonCMSInterval = Config["server"]["postInterval"].as<int>();
    EmonBulkSend = Config["server"]["bulksend"].as<int>();
    if(EmonBulkSend > 10) EmonBulkSend = 10;
    if(EmonBulkSend <1) EmonBulkSend = 1;
    EmonUsername = Config["server"]["username"].as<String>();
    EmonSendMode EmonPrevSend = EmonSend;
    EmonSend = EmonSendGET;
    if(EmonUsername != "")EmonSend = EmonSendPOSTsecure;
    if(EmonPrevSend != EmonSend) EmonInitialize = true;  
    if( ! EmonStarted) {
      NewService(EmonService);
      EmonStarted = true;
      EmonStop = false;
    }
    #define hex2bin(x) (x<='9' ? (x - '0') : (x - 'a') + 10)
    apiKey.toLowerCase();
    for(int i=0; i<16; i++){
      cryptoKey[i] = hex2bin(apiKey[i*2]) * 16 + hex2bin(apiKey[i*2+1]); 
    }
  }
  else {
    EmonStop = true;
    if(!serverType.equals("none")){
      msgLog("server type is not supported: ", serverType);
    }
  }

  delete[] ConfigBuffer;
  return true;
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

