boolean getConfig(void)
{
  File TableFile;
  char* TableBuffer = NULL;
  File ConfigFile;
  char* ConfigBuffer = NULL;
  StaticJsonBuffer<3000> Json;
  
  //************************************** Load and parse Json Table file *************************

  TableFile = SD.open("iotawatt/tables.json");
  if(TableFile) {
    uint16_t filesize = TableFile.size();
    TableBuffer = new char [filesize];
    if(TableBuffer == NULL)
    {
      Serial.println("Buffer allocation failed: Table file.");
      return false;
    }
    for(int i=0; i<filesize; i++) TableBuffer[i] = TableFile.read();
  }
  else
  {
    Serial.println("table file open failed.");
    return false;
  }
  TableFile.close();
  
  JsonObject& Table = Json.parseObject(TableBuffer);
  
  if (!Table.success()) {
    Serial.println("Table file parse failed.");
    Serial.println(TableBuffer);
    return false;
  }
  
  //************************************** Load and parse Json Config file ************************
  
  ConfigFile = SD.open("iotawatt/config.json");
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
  Serial.println(" ");
  ConfigFile.close();
  
  JsonObject& Config = Json.parseObject(ConfigBuffer);
  
  if (!Config.success()) {
    Serial.println("Config file parse failed.");
    Serial.println(ConfigBuffer);
    return false;
  }

  //************************************** Process Config file *********************************
  
  Serial.print("device:");
  Serial.write(Config["device"]["model"].asString());
  Serial.print(", version:");
  Serial.print(Config["device"]["version"].asString());
  ADC_bits = 10;
  String adc = Config["device"]["adc"].asString();
  Serial.print(", ADC=");
  Serial.print(adc);
  if(adc == "MCP3208") ADC_bits = 12;
  setADC_bits(ADC_bits);
  Serial.println();

  //************************************************ Configure channels ***************************

  for(int i=0; i<16; i++)
  {
    channelActive[i] = false;
  }
  
  uint16_t channelsCount = Config["inputs"].size();
  for(int i=0; i<channelsCount; i++)
  {
    int16_t channel = Config["inputs"][i]["channel"].as<int>();
    String type = Config["inputs"][i]["type"].asString();
    String model = Config["inputs"][i]["model"].asString();
    Serial.print("channel ");
    Serial.print(channel);
    Serial.print(": ");
    Serial.print(type);
    Serial.print(" ");
    Serial.print(model);
    Serial.print(" ");

    if(type == "vt")
    {
      calibration[channel] = Config["inputs"][i]["cal"].as<float>();
      phaseCorrection[channel] = Config["inputs"][i]["phase"].as<float>();     
      channelType[channel] = channelTypeVoltage;
    }
    
    else if (type == "ct")
    {
      String ctType = Table["ct"][model]["type"].asString();
      if(ctType == "c")
      {
        float burden = Config["inputs"][i]["burden"].as<float>();
        calibration[channel] = Table["ct"][model]["turns"].as<float>() / burden; 
      }    
      else if(ctType == "v")
      {
        calibration[channel] = Table["ct"][model]["cal"].as<float>();
      }
      else
      {
        Serial.println("Model not in Table File.");
        continue;
      }
      phaseCorrection[channel] = Table["ct"][model]["phase"].as<float>();
      Vchannel[channel] = Config["inputs"][i]["Vchannel"].as<int>();
      channelType[channel] = channelTypePower;
    }
    
    else if (type == "4-20ma") Serial.println("Not yet implimented."); 
    else if (type == "temp") Serial.print("Not yet implimented.");
    else if (type == "switch") Serial.print("Not yet implimented.");
    else Serial.print("Unrecognized type.");
    
    Serial.println();
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
    apikey = Config["server"]["apikey"].asString();
    Serial.print(", apikey: ");
    Serial.print(apikey);
    node = Config["server"]["node"];
    Serial.print(", node: ");
    Serial.print(node);
    post_interval_sec = Config["server"]["postInterval"].as<int>();
    post_interval_ms = post_interval_sec * 1000;
    Serial.print(", post interval: ");
    Serial.print(post_interval_sec);
    Serial.println();
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

void setADC_bits(uint8_t bits)
{
  ADC_bits = bits;                            // Set counts and
  ADC_range = 1 << bits;                      // ADC output range
  for(int i=0; i<16; i++)
  {
    offset[i] = ADC_range / 2;                // Bias voltage starting point
  }
  minOffset = (ADC_range * 49) / 100;         // Allow +/- 1% variation
  maxOffset = (ADC_range * 51) / 100;
  return;        
}
