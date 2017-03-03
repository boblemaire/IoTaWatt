void setup()
{
  for(int i=0; i<MAXCHANNELS; i++){                     // Start with a clean slate.
    buckets[i].value1 = 0;
    buckets[i].value2 = 0;
    buckets[i].accum1 = 0;
    buckets[i].accum2 = 0;
    buckets[i].timeThen = millis();
        
    calibration[i] = 0.0;
    phaseCorrection[i] = 0.0;
    channelType [i] = channelTypeUndefined;
    channelName [i] = "";
    Vchannel[i] = 0;
    offset[i] = ADC_range / 2;
    CTreversed[i] = false;
    CTsigned[i] = false;
  }

  //*************************************** Start Serial connection (if any)***************************
  
  Serial.begin(115200);
  delay(250);
  Serial.println("\r\n\n\n** Restart **\r\n\n");
  Serial.println("Serial Initialized");
  
  msgLog("Version: ", IOTAWATT_VERSION);

  //*************************************** Start SPI *************************************************
    
  pinMode(pin_CS_ADC0,OUTPUT);                    // Make sure all the CS pins are HIGH
  digitalWrite(pin_CS_ADC0,HIGH);
  pinMode(pin_CS_ADC1,OUTPUT);
  digitalWrite(pin_CS_ADC1,HIGH);
  pinMode(pin_CS_ADC2,OUTPUT);
  digitalWrite(pin_CS_ADC2,HIGH);
  pinMode(pin_CS_SDcard,OUTPUT);
  digitalWrite(pin_CS_SDcard,HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
  msgLog("SPI started.");
  
  //*************************************** Initialize the SD card ************************************

  if(!SD.begin(pin_CS_SDcard)) {
    msgLog("SD initiatization failed. Retrying.");
    while(!SD.begin(pin_CS_SDcard)){
      delay(100); 
      yield();
    }
  }
  msgLog("SD initialized.");
  hasSD = true;

  msgLog("Reset reason: ",(char*)ESP.getResetReason().c_str());
  logTrace();
  msgLog("ESP8266 ChipID:",ESP.getChipId());

  //************************************* Process Config file *****************************************
  
  if(!getConfig())
  {
    msgLog("Configuration failed");
    dropDead();
  }

  //*************************************** Start the internet connection *****************************

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect(deviceName.c_str());

  msgLog("WiFi connected, IP address: ", formatIP(WiFi.localIP()));

  //*************************************** Initialize timer and time of day *************************

  initTime();
  msgLog("Unix time:", UNIXtime());
  NewService(timeSync);
  programStartTime = UNIXtime();

 //*************************************** Start the local DNS service ****************************

  if (MDNS.begin(host)) {
      MDNS.addService("http", "tcp", 80);
      msgLog("MDNS responder started");
      msgLog("You can now connect to http://", host, ".local");
  }

 //*************************************** Start the web server ****************************

  server.on("/status",HTTP_GET, handleStatus);
  server.on("/vcal",HTTP_GET, handleVcal);
  server.on("/pcal",HTTP_GET, handlePcal);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

  SdFile::dateTimeCallback(dateTime);

  server.begin();
  msgLog("HTTP server started");

 //*************************************** Start the logging services *********************************
   
  NewService(dataLog);
  NewService(statService);
}

/***************************************** End of Setup **********************************************/

String formatHex(uint32_t data){
  const char* hexDigits = "0123456789ABCDEF";
  String str = "00000000";
  uint32_t _data = data;
  for(int i=7; i>=0; i--){
    str[i] = hexDigits[_data % 16];
    _data /= 16;
  }
  return str;
}

void dropDead(void){dropDead(1);}
void dropDead(int secs){
  msgLog("Program halted.");
  while(1){
    digitalWrite(pin_RED_LED, HIGH);
    delay(secs*1000);
    yield();
    
    digitalWrite(pin_RED_LED, LOW);
    delay(secs*1000);
    yield();   
  }  
}

   



