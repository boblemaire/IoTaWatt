boolean getConfig(void)
{
  File ConfigFile;
  String ConfigFileURL = "config.txt";
  char* ConfigBuffer = NULL;
  DynamicJsonBuffer Json;
    
  //************************************** Load and parse Json Config file ************************
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  if(ConfigFile) {
    uint16_t filesize = ConfigFile.size();
    ConfigBuffer = new char [filesize];
    if(ConfigBuffer == NULL){
      msgLog("Buffer allocation failed: Config file.");
      return false;
    }
    for(int i=0; i<filesize; i++) ConfigBuffer[i] = ConfigFile.read();
  }
  else
  {
    msgLog("Config file open failed.");
    return false;
  }
  ConfigFile.close();
  JsonObject& Config = Json.parseObject(ConfigBuffer);
  
  if (!Config.success()) {
    msgLog("Config file parse failed.");
    Serial.println(ConfigBuffer);
    return false;
  }
  
  //************************************** Process Config file *********************************
  deviceName = Config["device"]["name"].asString();
  if(deviceName.length() == 0) deviceName = "IoTaWatt";
  deviceName.toCharArray(host,9);
  host[8] = 0;
  String msg = "device name: " + deviceName + ", version: " + Config["device"]["version"].asString();
  msgLog(msg);

  if(Config["device"]["version"].asString()[0] >= '2'){
    hasRTC = true;
  }
  
  //************************************************ Configure channels ***************************
  for(int i=0; i<channels; i++) channelType[i] = channelTypeUndefined;
  uint16_t channelsCount = Config["inputs"].size();
  for(int i=0; i<channelsCount; i++)
  {
    int16_t channel = Config["inputs"][i]["channel"].as<int>();
    if(channel >= channels){
      msgLog("Unsupported channel configured: ", channel);
      continue;
    }
    channelName[channel] = Config["inputs"][i]["name"].asString();
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
    if(secure == "secure"){
      eMonSecure = true;
      msg += ", HTTPS protocol";
    }
    msgLog(msg);
    NewService(eMonService);
  }
  else
  {
    msgLog("server type is not supported: ", serverType);
    return false;
  }
  delete[] ConfigBuffer;
  return true;
}


