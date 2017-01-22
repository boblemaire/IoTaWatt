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
  msgLog("device name: ", deviceName);
  msgLog("version:", Config["device"]["version"].asString());
  
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
    msgLog("cloud server is: eMonCMS");
     eMonURL = Config["server"]["url"].asString();
    msgLog("url: ",  eMonURL);
    apiKey = Config["server"]["apikey"].asString();
    node = Config["server"]["node"];
    msgLog("node: ", node);
    eMonCMSInterval = Config["server"]["postInterval"].as<int>();
    msgLog("post interval: ", eMonCMSInterval);
    String secure = Config["server"]["secure"].asString();
    if(secure == "secure"){
      eMonSecure = true;
    }
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


