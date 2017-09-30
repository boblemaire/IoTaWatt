#include "IotaWatt.h"


String formatHex(uint32_t data);
void dropDead(void);
void dropDead(const char*);
void setLedCycle(const char* pattern);
void endLedCycle();
void ledBlink();
void setLedState();

void setup()
{
  //*************************************** Start Serial connection (if any)***************************
   
  Serial.begin(115200);
  delay(250);
  Serial.println(F("\r\n\n\n** Restart **\r\n\n"));
  Serial.println(F("Serial Initialized"));
  
  //*************************************** Start SPI *************************************************
    
  pinMode(pin_CS_ADC0,OUTPUT);                    // Make sure all the CS pins are HIGH
  digitalWrite(pin_CS_ADC0,HIGH);
  pinMode(pin_CS_ADC1,OUTPUT);
  digitalWrite(pin_CS_ADC1,HIGH);
  pinMode(pin_CS_SDcard,OUTPUT);
  digitalWrite(pin_CS_SDcard,HIGH);
  
  pinMode(redLed,OUTPUT);
  digitalWrite(redLed,LOW);

  //*************************************** Initialize SPI *******************************************
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
  msgLog(F("SPI started."));
   
  //*************************************** Initialize the SD card ************************************

  if(!SD.begin(pin_CS_SDcard)) {
    msgLog(F("SD initiatization failed. Retrying."));
    setLedCycle("G.R.R...");
    while(!SD.begin(pin_CS_SDcard, SPI_FULL_SPEED)){ 
      yield();
    }
    endLedCycle();
    digitalWrite(greenLed,HIGH); 
  }
  msgLog(F("SD initialized."));
  hasSD = true;

  //*************************************** Check RTC   *****************************

  Wire.begin(pin_I2C_SDA, pin_I2C_SCL);
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
      msgLog(F("Power failure detected."));
      Wire.beginTransmission(PCF8523_ADDRESS);            
      Wire.write((byte)PCF8523_CONTROL_3);
      Wire.write((byte)0);
      Wire.endTransmission();
    }
    SdFile::dateTimeCallback(dateTime);
  }
  else {
    msgLog(F("Real Time Clock not initialized."));
  }
  programStartTime = UNIXtime();
  
  Wire.beginTransmission(PCF8523_ADDRESS);            // Set crystal load capacitance
  Wire.write((byte)0);
  Wire.write((byte)0x80);
  Wire.endTransmission();

  //**************************************** Display software version *********************************

  msgLog("Version: ", IOTAWATT_VERSION);

  copyUpdate(String(IOTAWATT_VERSION));
  
  //**************************************** Display the trace ****************************************

  msgLog("Reset reason: ",(const char*)ESP.getResetReason().c_str());
  logTrace();
  msgLog("ESP8266 ChipID:",ESP.getChipId());

//************************************* Process Config file *****************************************
  
  if(!getConfig()) {
    msgLog(F("Configuration failed"));
    dropDead();
  }
  String msg = "device name: " + deviceName + ", version: " + String(deviceVersion); 
  msgLog(msg);
  msgLog("Local time zone: ",String(localTimeDiff));

//*************************************** Start the WiFi  connection *****************************
  
  WiFi.setAutoConnect(true);
  WiFi.hostname(host);
  WiFi.begin();
  uint32_t autoConnectTimeout = millis() + 3000UL;
  while(WiFi.status() != WL_CONNECTED){
    if(millis() > autoConnectTimeout){
      setLedCycle("R.G.G...");
      wifiManager.setDebugOutput(false);
      wifiManager.setConfigPortalTimeout(180);
      String ssid = "iota" + String(ESP.getChipId());
      String pwd = deviceName;
      msgLog(F("Connecting with WiFiManager."));

      wifiManager.autoConnect(ssid.c_str(), pwd.c_str());
      endLedCycle();
      while(WiFi.status() != WL_CONNECTED && RTCrunning == false){
        msgLog(F("RTC not running, waiting for WiFi."));
        setLedCycle("R.R.G...");
        wifiManager.setConfigPortalTimeout(3600);
        wifiManager.autoConnect(ssid.c_str(), pwd.c_str());
        endLedCycle();
      }
      break;
    }
    yield();
  }
  if(WiFi.status() != WL_CONNECTED){
    msgLog(F("No WiFi connection."));
  }

  //**************************************** Check for pending update ********************************

  if(checkUpdate()){
    msgLog(F("Firmware updated, restarting"));
    delay(500);
    ESP.restart();
  }  
    
  //*************************************** Start the local DNS service ****************************

  if (MDNS.begin(host.c_str())) {
      MDNS.addService("http", "tcp", 80);
      msgLog(F("MDNS responder started"));
      msgLog(String("You can now connect to http://" + String(host) + ".local"));
  }
   
 //*************************************** Start the web server ****************************

  server.on("/status",HTTP_GET, handleStatus);
  server.on("/vcal",HTTP_GET, handleVcal);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/config",HTTP_GET, handleGetConfig);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, returnOK, handleFileUpload);
  server.onNotFound(handleNotFound);

  SdFile::dateTimeCallback(dateTime);

  server.begin();
  msgLog(F("HTTP server started"));
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

void dropDead(void){dropDead("R.R.R...");}
void dropDead(const char* pattern){
  msgLog(F("Program halted."));
  setLedCycle(pattern);
  while(1){
    delay(1000);   
  }  
}

void setLedCycle(const char* pattern){
  ledCount = 0;
  for(int i=0; i<13; i++){
    ledColor[i] = pattern[i];
    if(pattern[i] == 0) break;
  }
  ticker.attach(0.5, ledBlink);
}

void endLedCycle(){
  ticker.detach();
  setLedState();
}

void ledBlink(){
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
  if(ledColor[ledCount] == 0) ledCount = 0;
  if(ledColor[ledCount] == 'R')digitalWrite(redLed, HIGH);
  else if(ledColor[ledCount] == 'G')digitalWrite(greenLed, HIGH);
  ledCount++;
}

void setLedState(){
  digitalWrite(greenLed, HIGH);
  digitalWrite(redLed, LOW);
  if( !RTCrunning || WiFi.status() != WL_CONNECTED){
    digitalWrite(redLed, HIGH);
  }
}

      



