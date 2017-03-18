boolean getConfig(void)
{
  File ConfigFile;
  String ConfigFileURL = "config.txt";
  DynamicJsonBuffer Json;
    
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
    maxChannels = device["channels"].as<unsigned int>();
  }
  
  if(Config["device"]["version"].asString()[0] < '2'){
    hasRTC = false;
  } else {
    hasRTC = true;
  }
    
  //************************************************ Configure channels ***************************
  
  uint16_t channelsCount = Config["inputs"].size();
  channels = 0;
  JsonObject* input;
  for(int i=0; i<channelsCount; i++)
  {
    input = &Config["inputs"][i].as<JsonObject&>();
    int16_t channel = Config["inputs"][i]["channel"].as<int>();
    if(channel >= maxChannels){
      msgLog("Unsupported channel configured: ", channel);
      continue;
    }
    channels = channel+1;
    channelName[channel] = Config["inputs"][i]["name"].asString();
    if(channelName[channel] == "") channelName[channel] = String("chan: ") + String(channel);
    String type = Config["inputs"][i]["type"].asString();
    String model = Config["inputs"][i]["model"].asString();
    calibration[channel] = Config["inputs"][i]["cal"].as<float>();
    phaseCorrection[channel] = Config["inputs"][i]["phase"].as<float>();
    if(type == "VT") {
      channelType[channel] = channelTypeVoltage;
    } 
    else if (type == "CT"){
      channelType[channel] = channelTypePower;      
      Vchannel[channel] = Config["inputs"][i]["vchan"].as<int>();
      if(input->containsKey("signed")){
        CTsigned[channel] = true;
      }
    }  
    else msgLog("unsupported input type: ", type);
  }
  
  String serverType = Config["server"]["type"].asString();
  
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
    //if(Config["server"]["bulksend"].is<unsigned int>()){
      eMonBulkSend = Config["server"]["bulksend"].as<int>();
      if(eMonBulkSend > 10) eMonBulkSend = 10;
      if(eMonBulkSend <1) eMonBulkSend = 1; 
    //}
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


