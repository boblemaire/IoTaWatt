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
  JsonObject& Config = Json.parseObject(ConfigFile);  
  ConfigFile.close();
  
  if (!Config.success()) {
    msgLog("Config file parse failed.");
    return false;
  }
  
  //************************************** Process Config file *********************************
    
  JsonObject& device = Config["device"].as<JsonObject&>();

  if(device.containsKey("name")){
    deviceName = device["name"].asString();
  }
  deviceName.toCharArray(host,9);
  host[8] = 0;
  String msg = "device name: " + deviceName + ", version: " + Config["device"]["version"].asString();
  msgLog(msg);

  if(Config.containsKey("timezone")){
    localTimeDiff = Config["timezone"].as<signed int>();
    msgLog("Local time zone: ",String(localTimeDiff));
  }
    
  if(device.containsKey("chanaddr")){
    for(int i=0; i<device["chanaddr"].size(); i++){
      chanAddr[i] = device["chanaddr"][i].as<unsigned int>();
    }
  }
  
  if(device.containsKey("chanaref")){
    for(int i=0; i<device["chanaref"].size(); i++){
      chanAref[i] = device["chanaref"][i].as<unsigned int>();
    }
  }

  if(device.containsKey("refvolts")){
    VrefVolts = device["refvolts"].as<float>();
  } 

  if(device.containsKey("channels")){
    maxInputs = device["channels"].as<unsigned int>();
  }
  
  if(Config["device"]["version"].asString()[0] < '2'){
    hasRTC = false;
  } else {
    hasRTC = true;
  }
    
        //************************************ Configure input channels ***************************
        
  uint16_t channelsCount = Config["inputs"].size();
  for(int i=0; i<channelsCount; i++)
  {
    JsonObject& input = Config["inputs"][i].as<JsonObject&>();
    int16_t channel = input["channel"].as<int>();
          if(channel >= maxInputs){
      msgLog("Unsupported channel configured: ", channel);
      continue;
    }
    if(inputChannel[channel] != NULL){
      msgLog ("Duplicate input channel definition for channel: ", channel);
      delete inputChannel[channel];
    }
    IotaInputChannel* newChannel = new IotaInputChannel(channel, chanAddr[channel], chanAref[channel], ADC_BITS);
    inputChannel[channel] = newChannel;
    String type = input["type"].asString();
    String nombre = input["name"];
    if(nombre == "") nombre = String("chan: ") + String(channel);
    newChannel->_name = new char[nombre.length()+1];
    strcpy(newChannel->_name, nombre.c_str());
    newChannel->_model = new char[sizeof(input["model"])+1];
    strcpy(newChannel->_model, input["model"]);
    newChannel->_calibration = input["cal"].as<float>();
    newChannel->_phase = input["phase"].as<float>();
    if(type == "VT") {
      newChannel->_type = channelTypeVoltage;
    } 
    else if (type == "CT"){
      newChannel->_type = channelTypePower;
      newChannel->_vchannel = input["vchan"].as<int>();     
      if(input.containsKey("signed")){
        newChannel->_signed = true;
      }
    }  
    else msgLog("unsupported input type: ", type);
  }

        // Get server type
                                                  
  String serverType = Config["server"]["type"].asString();

      // ************************************ configure output channels ***************************

  if(Config.containsKey("outputs")){
    for(int i=0; i<Config["outputs"].size(); i++){
      JsonObject& outputObject = Config["outputs"][i].as<JsonObject&>();
      if(outputObject.containsKey("name") &&
         outputObject.containsKey("script")) {
             IotaOutputChannel* output = new IotaOutputChannel(outputObject["name"], outputObject["script"]);
             output->_channel = i+100;
             outputList.insertTail(output, output->_name);
         }
    }
    Serial.print("output Channels: ");
    IotaOutputChannel* outputChannel = (IotaOutputChannel*)outputList.findFirst();
    while(outputChannel != NULL){
      Serial.print(outputChannel->_name);
      Serial.print(" ");
      outputChannel = (IotaOutputChannel*)outputList.findNext(outputChannel);
    }
    Serial.println();
  }
  
      //************************************** configure eMonCMS **********************************

  if(serverType.equals("emoncms"))
  {
    msg = "server is: eMonCMS";
    eMonURL = Config["server"]["url"].asString();
    msg += ", url: " + eMonURL;
    apiKey = Config["server"]["apikey"].asString();
    node = Config["server"]["node"];
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
    msgLog(msg);
    NewService(eMonService);
  }
  else if(!serverType.equals("none")){
    msgLog("server type is not supported: ", serverType);
    return false;
  }
  
  return true;
}


