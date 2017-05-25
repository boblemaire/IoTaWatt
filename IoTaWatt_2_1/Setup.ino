void setup()
{
  //*************************************** Start Serial connection (if any)***************************
  
  Serial.begin(115200);
  delay(250);
  Serial.println("\r\n\n\n** Restart **\r\n\n");
  Serial.println("Serial Initialized");
  
  //*************************************** Start SPI *************************************************
    
  pinMode(pin_CS_ADC0,OUTPUT);                    // Make sure all the CS pins are HIGH
  digitalWrite(pin_CS_ADC0,HIGH);
  pinMode(pin_CS_ADC1,OUTPUT);
  digitalWrite(pin_CS_ADC1,HIGH);
  pinMode(pin_CS_ADC2,OUTPUT);
  digitalWrite(pin_CS_ADC2,HIGH);
  pinMode(pin_CS_SDcard,OUTPUT);
  digitalWrite(pin_CS_SDcard,HIGH);
  
  pinMode(redLed,OUTPUT);
  digitalWrite(redLed,LOW);

  //*************************************** Set the clock if RTC running *****************************

  Wire.pins(pin_I2C_SDA, pin_I2C_SCL);
    rtc.begin();

    if(rtc.initialized()){
      timeRefNTP = rtc.now().unixtime() + SEVENTY_YEAR_SECONDS;
      timeRefMs = millis();
      RTCrunning = true;
      msgLog("Real Time Clock is running.");
      msgLog("Unix time:", UNIXtime());
    }
    else {
      msgLog("Real Time Clock not initialized.");
    }
    programStartTime = UNIXtime();

  
  
  //*************************************** Initialize SPI *******************************************
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
  msgLog("SPI started.");
   
  //*************************************** Initialize the SD card ************************************

  if(!SD.begin(pin_CS_SDcard)) {
    msgLog("SD initiatization failed. Retrying.");
    while(!SD.begin(pin_CS_SDcard, SPI_FULL_SPEED)){
      ledMsg(redLed, redLed, greenLed);
      yield();
    }
    digitalWrite(greenLed,HIGH); 
  }
  msgLog("SD initialized.");
  hasSD = true;

  //**************************************** Display software version *********************************

  msgLog("Version: ", IOTAWATT_VERSION);

  //**************************************** Display the trace ****************************************

  msgLog("Reset reason: ",(char*)ESP.getResetReason().c_str());
  logTrace();
  msgLog("ESP8266 ChipID:",ESP.getChipId());

  //************************************* Process Config file *****************************************
  
  if(!getConfig())
  {
    msgLog("Configuration failed");
    dropDead();
  }
//*************************************** Start the WiFi  connection *****************************

  WiFi.begin();
  WiFi.setAutoConnect(true);
  delay(3000);
  if(WiFi.status() != WL_CONNECTED){
    wifiManager.setTimeout(120);
    String ssid = "iota" + String(ESP.getChipId());
    String pwd = "iotawatt";
    wifiManager.autoConnect(ssid.c_str(), pwd.c_str());
  }
  
 //*************************************** Start the local DNS service ****************************

  if (MDNS.begin(host)) {
      MDNS.addService("http", "tcp", 80);
      msgLog("MDNS responder started");
      msgLog("You can now connect to http://", host, ".local");
  }
  
 //*************************************** Start the web server ****************************

  server.on("/status",HTTP_GET, handleStatus);
  server.on("/vcal",HTTP_GET, handleVcal);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/config",HTTP_GET, handleGetConfig);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){returnOK(); }, handleFileUpload);
  server.on("/disconnect",HTTP_GET, handleDisconnect);
  server.onNotFound(handleNotFound);

  SdFile::dateTimeCallback(dateTime);

  server.begin();
  msgLog("HTTP server started");
  WiFi.mode(WIFI_STA);
  

 //*************************************** Start the logging services *********************************
   
  NewService(dataLog);
  NewService(statService);
  NewService(timeSync);
  NewService(WiFiService);
  
}  // setup()
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
    ledMsg(redLed, redLed, redLed);
    yield();   
  }  
}

void ledMsg(int first, int second, int third){
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
  digitalWrite(first, HIGH);
  delay(500);
  digitalWrite(first, LOW);
  delay(500);
  digitalWrite(second, HIGH);
  delay(500);
  digitalWrite(second, LOW);
  delay(500);
  digitalWrite(third, HIGH);
  delay(500);
  digitalWrite(third, LOW);
  delay(2000);  
}
      



