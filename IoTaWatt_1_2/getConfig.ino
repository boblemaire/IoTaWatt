boolean getConfig(void)
{
  File TableFile;
  String tableFileURL = "tables.txt";
  char* TableBuffer = NULL;
  File ConfigFile;
  String ConfigFileURL = "config.txt";
  char* ConfigBuffer = NULL;
  StaticJsonBuffer<3000> Json;
    
  //************************************** Load and parse Json Config file ************************
  ConfigFile = SD.open(ConfigFileURL, FILE_READ);
  if(ConfigFile) {
    uint16_t filesize = ConfigFile.size();
    ConfigBuffer = new char [filesize];
    if(ConfigBuffer == NULL)
    {
      Serial.println("Buffer allocation failed: Config file.");
      return false;
    }
    for(int i=0; i<filesize; i++) ConfigBuffer[i] = ConfigFile.read();
  }
  else
  {
    Serial.println("Config file open failed.");
    return false;
  }
  ConfigFile.close();
  JsonObject& Config = Json.parseObject(ConfigBuffer);
  
  if (!Config.success()) {
    Serial.println("Config file parse failed.");
    Serial.println(ConfigBuffer);
    return false;
  }
  
  
  //************************************** Load and parse Json Table file *************************
  TableFile = SD.open(tableFileURL);
  if(TableFile) {
    uint16_t filesize = TableFile.size();
    TableBuffer = new char [filesize];
    if(TableBuffer == NULL){
      Serial.println("Buffer allocation failed: Table file.");
      return false;
    }
    Serial.print("Reading ");
    Serial.println(filesize);
    for(int i=0; i<filesize; i++) {
      TableBuffer[i] = TableFile.read();
    }
  }
  else
  {
    Serial.println("table file open failed.");
    return false;
  }
  TableFile.close();
  JsonObject& table = Json.parseObject(TableBuffer);
  
  if (!table.success()) {
    Serial.println("Table file parse failed.");
    Serial.println(TableBuffer);
    return false;
  }


  //************************************** Process Config file *********************************
  deviceName = Config["device"]["name"].asString();
  if(deviceName.length() == 0) deviceName = "IoTaWatt";
  deviceName.toCharArray(host,9);
  host[8] = 0;
  Serial.print("device name:");
  Serial.print(deviceName);
  Serial.print(", version:");
  Serial.print(Config["device"]["version"].asString());
  Serial.println();

  //************************************************ Configure channels ***************************
  for(int i=0; i<channels; i++) channelType[i] = channelTypeUndefined;
  uint16_t channelsCount = Config["inputs"].size();
  for(int i=0; i<channelsCount; i++)
  {
    int16_t channel = Config["inputs"][i]["channel"].as<int>();
    if(channel >= channels){
      Serial.print("Unsupported channel configured: ");
      Serial.println(channel);
      continue;
    }
    String type = Config["inputs"][i]["type"].asString();
    String model = Config["inputs"][i]["model"].asString();
//    Serial.print("channel ");
//    Serial.print(channel);
//    Serial.print(": ");
//    Serial.print(type);
//    Serial.print(" ");
//    Serial.print(model);
//    Serial.print(" ");

    int tableIndex = -1;
    for(int j=0; j<table[type].size(); j++) {
      if(model == table[type][j]["model"]){
        tableIndex = j;
        break;
      }
    }
    if(tableIndex == -1){
      Serial.print("Unrecognized model.");
      continue;
    }
    if(type == "VT") {
      channelType[channel] = channelTypeVoltage;
      String vtType = table["VT"][tableIndex]["type"].asString();
      if(vtType == "U") {
        calibration[channel] = Config["inputs"][i]["cal"].as<float>();
        phaseCorrection[channel] = Config["inputs"][i]["phase"].as<float>();     
      }
      else {
        calibration[channel] = table["VT"][tableIndex]["cal"].as<float>();
        phaseCorrection[channel] = table["VT"][tableIndex]["phase"].as<float>();
      }
      
//      Serial.print(" cal:");
//      Serial.print(calibration[channel],1);
//      Serial.print(", phase:");
//      Serial.print(phaseCorrection[channel],2);
    }
    
    else if (type == "CT")
    {
      channelType[channel] = channelTypePower;
      String ctType = table["CT"][tableIndex]["type"].asString();
      if(ctType == "C")
      {
        float burden = Config["inputs"][i]["burden"].as<float>();
        calibration[channel] = table["CT"][tableIndex]["turns"].as<float>() / burden;
        phaseCorrection[channel] = table["CT"][tableIndex]["phase"].as<float>(); 
      }    
      else if(ctType == "V")
      {
        calibration[channel] = table["CT"][tableIndex]["cal"].as<float>();
        phaseCorrection[channel] = table["CT"][tableIndex]["phase"].as<float>(); 
      }
      else if(ctType == "U"){
        calibration[channel] = Config["inputs"][i]["cal"].as<float>();
        phaseCorrection[channel] = Config["inputs"][i]["phase"].as<float>();
      } 
      else
      {
        Serial.println("Config entry invalid.");
        continue;
      }
      
      Vchannel[channel] = Config["inputs"][i]["Vchannel"].as<int>();
//      Serial.print(" cal:");
//      Serial.print(calibration[channel],1);
//      Serial.print(", phase:");
//      Serial.print(phaseCorrection[channel],2);
      
    }
    
    else if (type == "4-20ma") Serial.println("Not yet implimented."); 
    else if (type == "temp") Serial.print("Not yet implimented.");
    else if (type == "switch") Serial.print("Not yet implimented.");
    else Serial.print("Unrecognized type.");
    
//    Serial.println();
  }

  String serverType = Config["server"]["type"].asString();
  
  //************************************** configure eMonCMS **********************************

  if(serverType.equals("emoncms"))
  {
    Serial.print("cloud server is: ");
    Serial.print("emoncms");
    cloudURL = Config["server"]["url"].asString();
    Serial.print(", url: ");
    Serial.print(cloudURL);
    apiKey = Config["server"]["apikey"].asString();
//    Serial.print(", apikey: ");
//    Serial.print(apiKey);
    node = Config["server"]["node"];
    Serial.print(", node: ");
    Serial.print(node);
    eMonCMSInterval = Config["server"]["postInterval"].as<int>();
    Serial.print(", post interval: ");
    Serial.print(eMonCMSInterval);
    Serial.println();
    NewService(postEmonCMS);
  }
  else
  {
    Serial.print("server type ");
    Serial.print(serverType);
    Serial.println(" is not supported.");
    return false;
  }

  return true;
}


