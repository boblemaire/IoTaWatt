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
  //Serial.println(F("\r\n\n\n** Restart **\r\n\n"));
  //Serial.println(F("Serial Initialized"));
  
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
  Serial.println("\r\nSPI started.");
   
  //*************************************** Initialize the SD card ************************************

  if(!SD.begin(pin_CS_SDcard)) {
    log("SD initiatization failed. Retrying.");
    setLedCycle(LED_SD_INIT_FAILURE);
    while(!SD.begin(pin_CS_SDcard, SPI_FULL_SPEED)){ 
      yield();
    }
    endLedCycle();
    digitalWrite(greenLed,HIGH); 
  }
  hasSD = true;
  log("SD initialized.");

  //*************************************** Check RTC   *****************************

  Wire.begin(pin_I2C_SDA, pin_I2C_SCL);
  rtc.begin();

  Wire.beginTransmission(PCF8523_ADDRESS);            // Read Control registers
  Wire.write(PCF8523_CTL_1);
  Wire.endTransmission();
  Wire.requestFrom(PCF8523_ADDRESS, 3);
  byte Control_1 = Wire.read();
  byte Control_2 = Wire.read();
  byte Control_3 = Wire.read();
  
  if(rtc.initialized()){
    timeRefNTP = rtc.now().unixtime() + SEVENTY_YEAR_SECONDS;
    timeRefMs = millis();
    RTCrunning = true;
    log("Real Time Clock is running. Unix time %d ", UTCtime());
    if((Control_3 & PCF8523_CTL_3_BSF) != 0){
      log("Power failure detected.");
      powerFailRestart = true;
      Wire.beginTransmission(PCF8523_ADDRESS);            
      Wire.write((byte)PCF8523_CONTROL_3);
      Wire.write((byte)0);
      Wire.endTransmission();
    }
    else if(Control_2 & PCF8523_CTL_2_WTAF){
      log("RTC watchdog timer reset");
    }
    if(Control_3 & PCF8523_CTL_3_BLF){
      log("Real Time Clock battery is low.");
      RTClowBat = true;
    }
    //SdFile::dateTimeCallback(dateTime);
  }
  else {
    log("Real Time Clock not initialized.");
  }
  programStartTime = UTCtime();
  
  Wire.beginTransmission(PCF8523_ADDRESS);            // Set crystal load capacitance
  Wire.write((byte)0);
  Wire.write((byte)PCF8523_CTL_1_CAP_SEL);
  Wire.endTransmission();

    //**************************************** Display the trace ****************************************

  log("Reset reason: %s", ESP.getResetReason().c_str());
  if( ! powerFailRestart){
      logTrace();
  }
  log("ESP8266 ChipID: %d",ESP.getChipId());

  // ****************************************** Flush the trace ************************************

  traceEntry.seq = 0;
  for(int i=0; i<32; i++) trace(0,0);

//*************************************** Process the EEPROM ****************************************

EEprom* EE = new EEprom;
uint8_t* EEbytes = (uint8_t*) EE;
size_t EEsize = sizeof(EEprom);

    // Initialize the EEprom for testing
    // Ordinarily to be done in manufacturing.

// EEPROM.begin(EEsize);
// memcpy(EE->id, "IoTaWatt", 8);
// EE->EEversion = 0;
// EE->deviceMajorVersion = 5;
// EE->deviceMinorVersion = 0;
// EE->mfgDate = 0;
// EE->mfgLot = 0;
// EE->mfgBurden = 20;
// EE->mfgRefVolts = 2500;
// for(int i=0; i<EEsize; i++){
//   EEPROM.write(i,EEbytes[i]);
// }
// EEPROM.end();

EEPROM.begin(EEsize);
for(int i=0; i<EEsize; i++){
  EEbytes[i] = EEPROM.read(i);
}
if( ! memcmp(EE->id, "IoTaWatt", 8)){
  if(EE->EEversion > 0){
    log("EEPROM unrecognized version %d", EE->EEversion);
  } else {
    deviceMajorVersion = EE->deviceMajorVersion;
    deviceMinorVersion  = EE->deviceMinorVersion;
    VrefVolts = (float)EE->mfgRefVolts / 1000.0;
  }
} 
EEPROM.end();
delete EE;
EE = nullptr;

//**************************************** Display software version *********************************

log("IoTaWatt %d.%s, Firmware version %s", deviceMajorVersion, deviceMajorVersion < 5 ? "x" : String(deviceMinorVersion).c_str(), IOTAWATT_VERSION);

//**************************************** Install any pending updates ******************************

copyUpdate(String(IOTAWATT_VERSION));

//*************************************** Mount the SPIFFS ******************************************

if(spiffsBegin()){
  log("SPIFFS mounted.");
} else {
  if(spiffsFormat()){
    log("SPIFFS formated");
    if(spiffsBegin()){
      log("SPIFFS mounted.");
    }
  } else {
    log("SPIFFS format failed");
  }
}

//************************************* Process Config file *****************************************
  deviceName = charstar(F(DEVICE_NAME));
  updateClass = charstar(F("NONE"));
  validConfig = getConfig("config.txt");
  log("Local time zone: %+d:%02d", (int)localTimeDiff/60, (int)localTimeDiff%60);
  if(timezoneRule){
    log("Using Daylight Saving Time (BST) when in effect.");
  }
  log("device name: %s", deviceName); 

//************************************* Load passwords *******************************************

  authLoadPwds();  

//*************************************** Start the WiFi  connection *****************************
  
  WiFi.hostname(deviceName);
  WiFi.setAutoConnect(true);
  WiFi.begin();

        // If the RTC is not running or power fail restart
        // Use the WiFi Manager.

  if( ! RTCrunning || powerFailRestart){
    uint32_t autoConnectTimeout = millis() + 3000UL;
    while(WiFi.status() != WL_CONNECTED){
      if(millis() > autoConnectTimeout){
        setLedCycle(LED_CONNECT_WIFI);
        WiFiManager wifiManager;
        wifiManager.setDebugOutput(false);
        wifiManager.setConfigPortalTimeout(180);
        String ssid = "iota" + String(ESP.getChipId());
        String pwd = deviceName;
        log("Connecting with WiFiManager.");
        wifiManager.autoConnect(ssid.c_str(), deviceName);
        endLedCycle();
        while(WiFi.status() != WL_CONNECTED && RTCrunning == false){
          log("RTC not running, waiting for WiFi.");
          setLedCycle(LED_CONNECT_WIFI_NO_RTC);
          wifiManager.setConfigPortalTimeout(3600);
          wifiManager.autoConnect(ssid.c_str(), pwd.c_str());
          endLedCycle();
        }
        break;
      }
      yield();
    }
  }

      //*************************************** Startup the Zeroconfig responders *********************

  if (MDNS.begin(deviceName)) {
    MDNS.addService("http", "tcp", 80);
    log("MDNS responder started for hostname %s", deviceName);
  }
  if (LLMNR.begin(deviceName)){
    log("LLMNR responder started for hostname %s", deviceName);
  } 
  
 //*************************************** Start the web server ****************************

  server.on(F("/edit"), HTTP_POST, returnOK, handleFileUpload);
  server.onNotFound(handleRequest);
  const char * headerkeys[] = {"X-configSHA256"};
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  log("HTTP server started");
  WiFi.mode(WIFI_STA);
  
 //*************************************** Start the logging services *********************************

  NewService(WiFiService, T_WiFi); 
  NewService(timeSync, T_timeSync);
  NewService(statService, T_stats);
  NewService(updater, T_UPDATE);
  NewService(dataLog, T_datalog);
  NewService(historyLog, T_history);

  if(! validConfig){
    setLedCycle(LED_BAD_CONFIG);
  }
  
}  // setup()
/***************************************** End of Setup **********************************************/

void dropDead(void){dropDead(LED_HALT);}
void dropDead(const char* pattern){
  log("Program halted.");
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
  if(validConfig){
    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, LOW);
    if( !RTCrunning || WiFi.status() != WL_CONNECTED){
      digitalWrite(redLed, HIGH);
    }
  }
} 