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

  //*************************************** Check RTC   *****************************

  Wire.pins(pin_I2C_SDA, pin_I2C_SCL);
  rtc.begin();
    
  Wire.beginTransmission(PCF8523_ADDRESS);            // Read Control_3
  Wire.write((byte)2);
  Wire.endTransmission();
  Wire.requestFrom(PCF8523_ADDRESS, 1);
  uint8_t Control_3 = Wire.read();
  
  if(rtc.initialized()){
    timeRefNTP = rtc.now().unixtime() + SEVENTY_YEAR_SECONDS;
    timeRefMs = millis();
    RTCrunning = true;
    msgLog("Real Time Clock is running. Unix time: ", UNIXtime());
    if((Control_3 & 0x08) != 0){
      msgLog("Power failure detected.");
      Wire.beginTransmission(PCF8523_ADDRESS);            
      Wire.write((byte)PCF8523_CONTROL_3);
      Wire.write((byte)0);
      Wire.endTransmission();
    }
  }
  else {
    msgLog("Real Time Clock not initialized.");
  }
  programStartTime = UNIXtime();
  
  Wire.beginTransmission(PCF8523_ADDRESS);            // Set crystal load capacitance
  Wire.write((byte)0);
  Wire.write((byte)0x80);
  Wire.endTransmission();

  //**************************************** Display software version *********************************

  msgLog("Version: ", IOTAWATT_VERSION);

  //**************************************** Display the trace ****************************************

  msgLog("Reset reason: ",(char*)ESP.getResetReason().c_str());
  logTrace();
  msgLog("ESP8266 ChipID:",ESP.getChipId());

  //************************************* Process Config file *****************************************
  
  if(!getConfig()) {
    msgLog("Configuration failed");
    dropDead();
  }
  String msg = "device name: " + deviceName + ", version: " + String(deviceVersion); 
  msgLog(msg);
  msgLog("Local time zone: ",String(localTimeDiff));
//*************************************** Start the WiFi  connection *****************************

  WiFi.begin();
  WiFi.setAutoConnect(true);
  delay(3000);
  if(WiFi.status() != WL_CONNECTED){
    wifiManager.setDebugOutput(false);
    wifiManager.setTimeout(120);
    String ssid = "iota" + String(ESP.getChipId());
    String pwd = "iotawatt";
    msgLog("Connecting with WiFiManager.");
    wifiManager.autoConnect(ssid.c_str(), pwd.c_str());
  }
  if(WiFi.status() != WL_CONNECTED){
    msgLog("No WiFi connection.");
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
  NewService(updater);
  
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
      



