void setup()
{
  
  for(int i=0; i<channels; i++){
    buckets[i].value1 = 0;
    buckets[i].value2 = 0;
    buckets[i].accum1 = 0;
    buckets[i].accum1 = 0;
    buckets[i].timeThen = millis();
        
    calibration[i] = 0.0;
    phaseCorrection[i] = 0.0;
    channelType [i] = channelTypeUndefined;
    Vchannel[i] = 0;
  }

  //*************************************** Start Serial connection (if any)***************************
  
  Serial.begin(115200);
  delay(1000);
  if(Serial)Serial.println("Serial Initialized");

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
  Serial.println("SPI started.");

  //*************************************** Initialize the MCP23S17 GPIO extender *********************

  GPIO.begin(pin_CS_GPIO);
  GPIO.writePin(yellowLedPin,HIGH);
  
  //*************************************** Configure the ADCs **************************************

  setADCbits(senseADCbits());

  //*************************************** Initialize the SD card ************************************

  if(!SD.begin(pin_CS_SDcard)) {
    Serial.println("SD initiatization failed. Retrying.");
    while(!SD.begin(pin_CS_SDcard)) delay(100);
  }
  Serial.println("SD initialized.");
  hasSD = true;

  //************************************* Process Config file *****************************************
  
  GPIO.writePin(yellowLedPin,LOW); 
  if(!getConfig())
  {
    Serial.println("Configuration failed");
    dropDead();
  }
  
  //*************************************** Start the internet connection *****************************

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect(deviceName.c_str());

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  setNTPtime();
  if(timeRefNTP == 0){
    Serial.println("Failed to retrieve NTP time, retrying.");
    while(!setNTPtime());
    Serial.println("NTP time successfully retrieved.");  
  }
  
  Serial.print("UTC time:");
  printHMS(NTPtime());
  Serial.println();
  Serial.print("Unix time:");
  Serial.println(UnixTime());
  NewService(timeSync);
  
 //*************************************** Measure and report Aref voltage****************************

  Serial.print("Aref=");
  Serial.println(getAref(),3);

 //*************************************** Start the local DNS service ****************************

  if (MDNS.begin(host)) {
      MDNS.addService("http", "tcp", 80);
      DBG_OUTPUT_PORT.println("MDNS responder started");
      DBG_OUTPUT_PORT.print("You can now connect to http://");
      DBG_OUTPUT_PORT.print(host);
      DBG_OUTPUT_PORT.println(".local");
  }

 //*************************************** Start the web server ****************************

  server.on("/status",HTTP_GET, handleStatus);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

//  host = deviceName;
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

 //*************************************** Start the logging services *********************************
      
  NewService(dataLog);
  NewService(statService);
 
}


void printHex(uint32_t data)
{
  byte digit;
  digit = data >> 24;
  if (digit < 16) Serial.print("0");
  Serial.print(digit,HEX);
  digit = (data >> 16) & 0xFF;
  if (digit < 16) Serial.print("0");
  Serial.print(digit,HEX);
  digit = (data >> 8) & 0xFF;
  if (digit < 16) Serial.print("0");
  Serial.print(digit,HEX);
  digit = data & 0xFF;
  if (digit < 16) Serial.print("0");
  Serial.print(digit,HEX);
  Serial.println();
  return;
}

void dropDead()
{
  GPIO.writePin(yellowLedPin,LOW);
  while(1)
  {  
    GPIO.writePin(redLedPin,HIGH);
    delay(500);
    GPIO.writePin(redLedPin,LOW);
    delay(500);
  }
  return;
}

/**************************************************************************************
 * 
 *  senseADCbits()  IoTaWatt cann be populated with either the MCP3008 (10 bit) or
 *  MCP3208 (12 bit).  They have identical pad placement and pinout.  The only 
 *  outward difference is that they return an extra two bits of resolution. This 
 *  sense method exploits a feature of both whereby if the SPI is clocked past the 
 *  resolution bits, the same result is output in reverse (lsb first) order. Note that 
 *  only the lsb bit in the msb first stream is the also the first bit of the lsb
 *  first stream.  In other words, only 19 or 23 significant bits (10 or 12 bit ADC).
 *  Both will also tollerate more cycles beyond that, outputting zeros (MISO low). 
 *  
 *  So we will read the reference voltage channel, returning 23 significant bits,
 *  and determine the ADC bits by determining where the mirror lies.
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

  Serial.println("ADC type unknown");
  Serial.print(msbFirst10);
  Serial.print(" - ");
  Serial.println(lsbFirst10);
  Serial.print(msbFirst12);
  Serial.print(" - ");
  Serial.println(lsbFirst12);

  return 10;
}

void setADCbits(uint8_t bits)
{
  ADC_bits = bits;                            // Set counts and
  Serial.print("ADC bits=");
  Serial.println(ADC_bits);
  ADC_range = 1 << bits;                      // ADC output range
  for(int i=0; i<channels; i++)
  {
    offset[i] = ADC_range / 2;                // Bias voltage starting point
  }
  minOffset = (ADC_range * 49) / 100;         // Allow +/- 1% variation
  maxOffset = (ADC_range * 51) / 100;
  return;        
}



