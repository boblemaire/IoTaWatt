#include "IotaWatt.h"
uint32_t littleEndian(uint32_t);

#define ntpPort 2390 
  struct ntpPacket {  /* courtesy Eugene Ma */
    uint8_t flags;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint8_t referenceID[4];
    uint32_t ref_ts_sec;
    uint32_t ref_ts_frac;
    uint32_t origin_ts_sec;
    uint32_t origin_ts_frac;
    uint32_t recv_ts_sec;
    uint32_t recv_ts_frac;
    uint32_t trans_ts_sec;
    uint32_t trans_ts_frac;
    ntpPacket()
    :flags(0xE3)
    ,stratum(0)
    ,poll(6)
    ,precision(0xEC)
    ,root_delay(0)
    ,root_dispersion(0)
    ,referenceID{49, 0x4E, 49, 52}
    ,ref_ts_sec(0)
    ,ref_ts_frac(0)
    ,origin_ts_sec(0)
    ,origin_ts_frac(0)
    ,recv_ts_sec(0)
    ,recv_ts_frac(0)
    ,trans_ts_sec(0)
    ,trans_ts_frac(0)
    {};
}; 

  /***************************************************************************************************
   * getNTPtime() - returns uint32_t binary count of seconds since 1/1/1900 - UTC aka GMT.
   * This is the fundamental time reference used by the program. The value is obtained from one of
   * many synchronized time servers using the basic UDP internet protocol.
   * 
   * The program stores the returned value along with the simultaneous value of the internal 
   * millisecond clock.  With those two values and the current millisecond clock value, an updated 
   * NTP time can be computed.  The program checks back in with NTP from time to time for a 
   * reality check.
   * 
   * NTP time is the number of seconds since 1/1/1900 (when the world began)
   * Unix time is NTP time minus 70 years worth of seconds (seconds since 1/1/70).
   * 
   * This program uses UNIXtime with a resolution of 1 second as the epoch. 
   **************************************************************************************************/ 

  // Following is a blocking routine to get NTP time that is used during startup when the RTC is not running.
  // Periodic synchronization of the internal millisecond clock is done asynchronously in the
  // timeSync() Service with minimal blocking. 

  uint32_t getNTPtime() {
    if(WiFi.isConnected()) {
      WiFiUDP udp;
      const char* ntpServerName = "time.nist.gov";
      ntpPacket packet;
      udp.begin(ntpPort);
      IPAddress timeServerIP;
      int dnsRetry = 0;
      while(WiFi.hostByName(ntpServerName, timeServerIP) != 1){   // get a random server from the pool
        if(++dnsRetry > 3) return 0;
      }
      udp.beginPacket(timeServerIP, 123);
      udp.write((uint8_t*)&packet, sizeof(ntpPacket));                  // send an NTP packet to a time server
      udp.endPacket();
      
      int maxWait = 10;                                   // We'll wait 5 100ms intervals
      while(maxWait--){
        if(udp.parsePacket()){
          udp.read((uint8_t*)&packet,sizeof(ntpPacket));
          udp.stop();
          timeRefNTP = littleEndian(packet.trans_ts_sec);
          timeRefMs = millis();
          return timeRefNTP;
        }
        yield();
        delay(100);
      }
      udp.stop();
    }
    return 0;
  }

/********************************************************************************************
 * 
 *  uint32_t NTPtime() - Return the current time in NTP format
 *  uint32_t Unixtime() - Return the current time in Unix format
 *  Both return zero if the clock is not set//
 *  uint32_t MillisAtUNIXtime - Return the local Millis() value corresponding to the UnixTime
 * 
 *******************************************************************************************/

uint32_t NTPtime() {
  return timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000;
 }
  
uint32_t UNIXtime() {
  return timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000 - SEVENTY_YEAR_SECONDS;
 }
 
uint32_t MillisAtUNIXtime(uint32_t UnixTime){                  
  return (uint32_t)timeRefMs + 1000 * (UnixTime - SEVENTY_YEAR_SECONDS - timeRefNTP);
 }

/********************************************************************************************
 * timeSync is a SERVICE that periodically (timeSynchInterval seconds) attempts to get
 * the NTP time from the internet, synchronize to that time and update the RTC.
 * No heroics to compensate for internet latency at this time.
 * The NTP request is mostly non-blocking, so no penalty for repeated attempts.
 * Failed attempts are just retried.... forever. 
 * Service logs when no reality check after 24 hours.
 *******************************************************************************************/
 
uint32_t timeSync(struct serviceBlock* _serviceBlock) {
  enum states {start, getNTPtime, waitNTPtime, setRTC};
  static states state = start;
  static WiFiUDP* udp = nullptr;;
  static uint32_t startTime;
  static uint32_t lastNTPupdate = 0;
 
  switch(state){

    case start: { 
      log("timeSync: service started.");
      lastNTPupdate = UNIXtime();  
      state = getNTPtime;
      return UNIXtime() + 60;    
    }

    case getNTPtime: {
      if(UNIXtime() - lastNTPupdate > 86400UL){ 
        log("timeSync: No time update in last 24 hours.");
        lastNTPupdate = UNIXtime();
      }
      if(WiFi.isConnected() && HTTPrequestFree){
        HTTPrequestFree--;
        IPAddress timeServerIP;
        startTime = millis();
        if(WiFi.hostByName(ntpServerName, timeServerIP) == 1){    // get a random server from the pool
          if( ! udp) udp = new WiFiUDP;
          ntpPacket packet;
          udp->begin(ntpPort);
          udp->beginPacket(timeServerIP, 123);
          udp->write((uint8_t*)&packet, sizeof(ntpPacket));        // send an NTP packet to a time server
          udp->endPacket();
          state = waitNTPtime;
          return 1;
        }
        else HTTPrequestFree++;
      }
      return UNIXtime() + RTCrunning ? 60 : 0;
      break;
    }  
    
    case waitNTPtime: {
      if(udp->parsePacket()){
        ntpPacket packet;
        udp->read((uint8_t*)&packet,sizeof(ntpPacket));
        udp->stop();
        HTTPrequestFree++;
        delete udp;
        udp = nullptr;
        timeRefNTP = littleEndian(packet.trans_ts_sec);
        timeRefMs = millis();
        lastNTPupdate = UNIXtime();
        state = setRTC;
        return 1;
      }
      else if(millis() - startTime > 3000){
        udp->stop();
        HTTPrequestFree++;
        state = getNTPtime;
        return UNIXtime() + RTCrunning ? 60 : 0;
      }
      return 1;
    }

    case setRTC: {
      if( ! RTCrunning) {
        programStartTime = UNIXtime();
        rtc.adjust(UNIXtime());
        log("timeSync: RTC initalized to NTP time");
      }
      else {
        int32_t timeDiff = UNIXtime() - rtc.now().unixtime();
        if(timeDiff < -1 || timeDiff > 1){
          log("timeSync: adjusting RTC by %d", timeDiff);
          rtc.adjust(UNIXtime());
        }
      }
      
          // The ms clock will rollover after ~49 days.  To be on the safe side,
          // restart the ESP after about 42 days to reset the ms clock.

      if(millis() > 3628800000UL) {
        log("timeSync: Six week routine restart.");
        ESP.restart();
      }


      state = getNTPtime;
      return UNIXtime() + timeSynchInterval;  
    }
  }    
}

//  This can be a little mind-bogling. The ESP stores words in little-endian format,
//  so internally the bytes are backasswards.  Although this would appear to do nothing
//  in fact it reverses the byte order to make it work in the ESP.  I don't fully 
//  understand it right now either, but it works.

uint32_t littleEndian(uint32_t in){
  uint8_t* bytes = (uint8_t*)&in; 
  return (((bytes[0] << 8 | bytes[1]) << 8) | bytes[2]) << 8 | bytes[3];
}

/********************************************************************************
 *   dateTime callback for SD so it can maintain dates in the directory.
 ********************************************************************************/

void dateTime(uint16_t* date, uint16_t* time) {
  uint32_t localUnixTime = UNIXtime() + localTimeDiff*3600;
  
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(DateTime(localUnixTime).year(),
                   DateTime(localUnixTime).month(),
                   DateTime(localUnixTime).day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(DateTime(localUnixTime).hour(), 
                   DateTime(localUnixTime).minute(),
                   DateTime(localUnixTime).second());
}

/********************************************************************************************
 * Just a handy little formatter.  
 *******************************************************************************************/
String formatHMS(uint32_t epoch) {
    String result = String((epoch  % 86400L) / 3600) + ":";
    if ( ((epoch % 3600) / 60) < 10 ) result += "0";
    result += String((epoch  % 3600) / 60) + ":";
    if ( (epoch % 60) < 10 ) result += "0";
    result += String(epoch % 60);
    return result;
  }

  

  
        
   
    
  

