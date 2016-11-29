void setup()
{

  //*************************************** Start Serial connection (if any)***************************
  
  Serial.begin(115200);
  delay(1000);
  if(Serial)Serial.println("Serial Initialized");

  //*************************************** Start SPI *************************************************
    
  pinMode(pin_CS_ADC0,OUTPUT);
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
  
 

  //*************************************** Initialize the SD card ************************************

  while (!SD.begin(pin_CS_SDcard)) {
    Serial.println("SD initialization failed!");
    Serial.println("insert SD card");
    return;
  }

  Serial.println("SD initialized");

  //*************************************** Start the internet connection *****************************

  WiFiManager wifiManager;
  wifiManager.autoConnect("IoTaWatt","xyz");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  
  //*************************************** Start the clock *******************************************
  
  last_sample_time = millis();
  last_post_time = last_sample_time;
 
  //************************************* Process Config file *****************************************
  
  GPIO.writePin(yellowLedPin,LOW);
    
  if(!getConfig())
  {
    Serial.println("Configuration failed");
    dropDead();
  }
  
 //*************************************** Measure and report Aref voltage****************************

  Serial.print("Aref=");
  Serial.println(getAref(),3);
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




