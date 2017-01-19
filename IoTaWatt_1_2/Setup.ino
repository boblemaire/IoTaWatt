void setup()
{
  for(int i=0; i<channels; i++){
    buckets[i].value1 = 0;
    buckets[i].value2 = 0;
    buckets[i].accum1 = 0;
    buckets[i].accum2 = 0;
    buckets[i].timeThen = millis();
        
    calibration[i] = 0.0;
    phaseCorrection[i] = 0.0;
    channelType [i] = channelTypeUndefined;
    Vchannel[i] = 0;
  }

  //*************************************** Start Serial connection (if any)***************************
  
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("Serial Initialized");

  msgLog("Version: ", IOTAWATT_VERSION);

  String restartMessage = "Normal Power Up";
  rst_info* _resetInfo = system_get_rst_info();
  if(_resetInfo->reason != REASON_DEFAULT_RST){
           restartMessage = "Restart reason: " +
           String(_resetInfo->reason) + " " +
           formatHex(_resetInfo->exccause,4) + " " +
           formatHex(_resetInfo->epc1,4) + " " +
           formatHex(_resetInfo->epc2,4) + " " +
           formatHex(_resetInfo->epc3,4) + " " +
           formatHex(_resetInfo->excvaddr,4) + " " +
           formatHex(_resetInfo->depc,4); 
    msgLog(restartMessage); 
  }

  //*************************************** Start SPI *************************************************
    
  pinMode(pin_CS_ADC0,OUTPUT);                    // Make sure all the CS pins are HIGH
  digitalWrite(pin_CS_ADC0,HIGH);
  pinMode(pin_CS_ADC1,OUTPUT);
  digitalWrite(pin_CS_ADC1,HIGH);
  pinMode(pin_CS_SDcard,OUTPUT);
  digitalWrite(pin_CS_SDcard,HIGH);
  pinMode(pin_CS_GPIO,OUTPUT);
  digitalWrite(pin_CS_GPIO,HIGH);
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
  msgLog("SPI started.");

  //*************************************** Initialize the MCP23S17 GPIO extender *********************

  GPIO.begin(pin_CS_GPIO);
  GPIO.writePin(yellowLedPin,HIGH);
  
  //*************************************** Configure the ADCs **************************************

  setADCbits(senseADCbits());

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

  msgLog(restartMessage);

  //************************************* Process Config file *****************************************
  
  GPIO.writePin(yellowLedPin,LOW); 
  if(!getConfig())
  {
    msgLog("Configuration failed");
    dropDead();
  }
    
  //*************************************** Start the internet connection *****************************

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect(deviceName.c_str());

  msgLog("WiFi connected, IP address: ", formatIP(WiFi.localIP()));
  
  setNTPtime();
  if(timeRefNTP == 0){
    msgLog("Failed to retrieve NTP time, retrying.");
    while(!setNTPtime());
    msgLog("NTP time successfully retrieved.");  
  }
  
  msgLog("UTC time:", formatHMS(NTPtime()));
  msgLog("Unix time:", UnixTime());
  NewService(timeSync);
  programStartTime = NTPtime();
  
 //*************************************** Measure and report Aref voltage****************************

  msgLog("Aref=", String(getAref(),3));
  
 //*************************************** Start the local DNS service ****************************

  if (MDNS.begin(host)) {
      MDNS.addService("http", "tcp", 80);
      msgLog("MDNS responder started");
      msgLog("You can now connect to http://", host, ".local");
  }

 //*************************************** Start the web server ****************************

  server.on("/status",HTTP_GET, handleStatus);
  server.on("/calvt",HTTP_GET, handleCalVT);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

//  host = deviceName;
  server.begin();
  msgLog("HTTP server started");

 //*************************************** Start the logging services *********************************
      
  NewService(dataLog);
  NewService(statService);
 
}


String formatHex(uint32_t data, int len)
{
  String str;
  byte digit;
  while(len--){
    digit = (data >> (len*8)) & 0xFF;
    if (digit < 16) str += "0";
    str += String(digit,HEX);  
  }
  return str;
}

void dropDead(void){dropDead(1);}
void dropDead(int secs){
  msgLog("Program halted.");GPIO.writePin(yellowLedPin,LOW);
  while(1){
    GPIO.writePin(redLedPin,LOW);
    delay(secs*1000);
    yield();
    
    GPIO.writePin(redLedPin,HIGH);
    delay(secs*1000);
    yield();   
  }  
}

/**************************************************************************************
 * 
 *  senseADCbits()  IoTaWatt cann be populated with either the MCP3008 (10 bit) or
 *  MCP3208 (12 bit).  They have identical pad placement and pinout.  The only 
 *  outward difference is that they return an extra two bits of resolution. This 
 *  sense method exploits a feature of both whereby if the SPI is clocked past the 
 *  resolution bits, the same result is output in reverse (lsb first) order. Note that 
 *  the lsb bit in the msb first stream is the also the first bit of the lsb
 *  first stream.  In other words, only 19 or 23 significant bits (10 or 12 bit ADC).
 *  Both will also tollerate more cycles beyond that, outputting zeros (MISO low). 
 *  
 *  So we this reads the reference voltage channel, returning 23 significant bits,
 *  and determines the ADC bits by determining where the mirror lies.
 * 
 **************************************************************************************/
uint8_t senseADCbits(){
 
  uint32_t align = 0;               // SPI requires out and in to be word aligned                                                                 
  uint8_t ADC_out [4] = {0, 0, 0, 0};
  uint8_t ADC_in  [4] = {0, 0, 0, 0};  
  uint8_t ADCselectPin;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
  ADCselectPin = pin_CS_ADC0;
  if(VrefChan > 7) ADCselectPin = pin_CS_ADC1;
    
  ADC_out[0] = (0x18 | (VrefChan & 0x07)) << 2;      // leave two null bits in first byte      

  digitalWrite(ADCselectPin, LOW);                  // Lower the chip select
  SPI.transferBytes(ADC_out, ADC_in, 4);            // Initiate sample and read 24 bits from the ADC
  digitalWrite(ADCselectPin, HIGH);                 // Raise the chip select to deselect and reset

  uint16_t msbFirst10 = word(ADC_in[1], ADC_in[2]) >> 6;
  uint16_t msbFirst12 = word(ADC_in[1], ADC_in[2]) >> 4;
  uint16_t lsbFirst10 = word(ADC_in[2] & 0x7F, ADC_in[3]) >> 5;
  uint16_t lsbFirst12 = word(ADC_in[2] & 0x1F, ADC_in[3]) >> 1;

  boolean is10Bit = true;
  for(int i=0; i<10; i++){
    if(bitRead(msbFirst10,i) != bitRead(lsbFirst10,(9-i))){
      is10Bit = false;
      break;  
    } 
  }

  boolean is12Bit = true;
  for(int i=0; i<12; i++){
    if(bitRead(msbFirst12,i) != bitRead(lsbFirst12,(11-i))){
      is12Bit = false;
      break;  
    } 
  }

  if(is10Bit and !is12Bit) return 10;
  if(is12Bit && !is10Bit) return 12;

  String msg = "ADC type unknown: ";
  msg += String(msbFirst10) + " - " + String(lsbFirst10);
  msg += ", " + String(msbFirst12) + " - " + String(lsbFirst12);
  msgLog(msg);
  return 10;
}

void setADCbits(uint8_t bits)
{
  ADC_bits = bits;                            // Set counts and
  msgLog("ADC bits=", ADC_bits);
  ADC_range = 1 << bits;                      // ADC output range
  for(int i=0; i<channels; i++)
  {
    offset[i] = ADC_range / 2;                // Bias voltage starting point
  }
  minOffset = (ADC_range * 49) / 100;         // Allow +/- 1% variation
  maxOffset = (ADC_range * 51) / 100;
  return;        
}



