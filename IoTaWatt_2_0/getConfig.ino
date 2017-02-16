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

  // Initialize hardware specific parameters

  if(Config["device"]["version"].asString()[0] < '2'){
    for(int i=1; i<MAXCHANNELS; i++){
      chanAddr[i] = i - 1;
      chanAref[i] = 15;
    }
    maxChannels = 15;
  }
  else {
    hasRTC = true;
    VrefVolts = 1.0;
    for(int i=0; i<MAXCHANNELS; i++){
      chanAddr[i] = i + i / 7;
      chanAref[i] = chanAddr[i] | 0x07;
    }
    String msg = "";
    for(int i=0; i<3; i++){
      msg = "ADC: " + String(i);
      ADCvoltage[i] = getAref(i*7);
      if(ADCvoltage[i] == 0){
        msg += ", not detected.";
      }
      else {
        msg += ", Voltage: " + String(ADCvoltage[i],3);
        maxChannels = (i + 1) * 7;
      }
      msgLog(msg);
    }
  }
    
  //************************************************ Configure channels ***************************
  
  uint16_t channelsCount = Config["inputs"].size();
  channels = 0;
  for(int i=0; i<channelsCount; i++)
  {
    int16_t channel = Config["inputs"][i]["channel"].as<int>();
    if(channel >= maxChannels){
      msgLog("Unsupported channel configured: ", channel);
      continue;
    }
    channels = channel+1;
    channelName[channel] = Config["inputs"][i]["name"].asString();
    String type = Config["inputs"][i]["type"].asString();
    String model = Config["inputs"][i]["model"].asString();
    calibration[channel] = Config["inputs"][i]["cal"].as<float>();
    phaseCorrection[channel] = Config["inputs"][i]["phase"].as<float>();
    if(type == "VT") {
      channelType[channel] = channelTypeVoltage;
      if(!channel % 7){
        voltageAdapt[i] = voltageAdapt3;
        if(ADCvoltage[chanAddr[channel] >> 3] < 2){
          voltageAdapt[i] = voltageAdapt1;
        }
      }
      
      else {
        calibration[channel] *= Vcal3volt;  
      }
      Serial.println(calibration[channel]);
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


