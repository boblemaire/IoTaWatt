  /***************************************************************************************************
   * setNTPtime() - returns uint32_t bimary count of seconds since 1/1/1900 - UTC aka GMT.
   * This is the fundamental time standard used by the program. The value is obtained from one of
   * many synchronized time servers using the basic UDP internet protocol.  This function is a
   * bastard son of several similar routines that I came across while putting this together. Here,
   * the emphasis was on just making it a straightforward utility resource.  It's not well documented,
   * but any similar routine that returns the NTP value could be used.
   * 
   * The program stores the returned value along with the simultaneous value of the internal 
   * millisecond clock.  With those two values and the current millisecond clock value, an updated 
   * NTP time can be computed.  The program checks back in with NTP from time to time for a 
   * reality check.
   * 
   * Unix time is NTP time minus 70 years worth of seconds (seconds since 1/1/70 when the world began). 
   **************************************************************************************************/
  uint32_t setNTPtime() {
    WiFiUDP udp;
    const char* ntpServerName = "time.nist.gov";
    const int NTP_PACKET_SIZE = 48;
    unsigned int localPort = 2390;
    byte packetBuffer[NTP_PACKET_SIZE] = {0xE3, 0, 6, 0xEC, 0,0,0,0,0,0,0,0, 49, 0x4E, 49, 52};
    const byte packet[16] = {0xE3, 0, 6, 0xEC, 0,0,0,0,0,0,0,0, 49, 0x4E, 49, 52};
    udp.begin(localPort);
    IPAddress timeServerIP;
    WiFi.hostByName(ntpServerName, timeServerIP);       // get a random server from the pool
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    for(int i=0; i<16; i++) packetBuffer[i] = packet[i];
    udp.beginPacket(timeServerIP, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);           // send an NTP packet to a time server
    udp.endPacket();
    
    int maxWait = 10;                                   // We'll wait 10 100ms intervals
    while(maxWait--){
      if(udp.parsePacket()){
        udp.read(packetBuffer,NTP_PACKET_SIZE);
        udp.stop();
        timeRefNTP = (((((packetBuffer[40] << 8) | packetBuffer[41]) << 8) | packetBuffer[42]) << 8) | packetBuffer[43];
        timeRefMs = millis();
        return timeRefNTP;
      }
      delay(100);
    }
    udp.stop();
    return 0;
  }

uint32_t NTPtime() {return (timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000);}
  
uint32_t UnixTime() {return (timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000) - 2208988800UL;}

int timeSync(struct serviceBlock* _serviceBlock){
  static boolean firstCall = true;
  if(firstCall){
    Serial.println("timeSync service started.");
    firstCall = false;
  }
  timeRefNTP = NTPtime();                 
  timeRefMs = millis();
  if(!setNTPtime()) Serial.println("getNTPtime failed");
  _serviceBlock->callTime = (uint32_t) timeSynchInterval * ( 1 + NTPtime() / timeSynchInterval);
  AddService(_serviceBlock);
}

void printHMS(uint32_t epoch) {

// format the hour, minute and second:

    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print(epoch % 60); // print the second
    return;
  }
