#include "IotaWatt.h"
uint32_t littleEndian(uint32_t);
#define NTP2018 (1514796044UL + SEVENTY_YEAR_SECONDS)
#define NTP2028 (1830328844UL + SEVENTY_YEAR_SECONDS)

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
 * The NTP request is blocking.
 * Failed attempts are just retried.... forever. 
 * Service logs when no reality check after 24 hours.
 *******************************************************************************************/
 
uint32_t timeSync(struct serviceBlock* _serviceBlock) {
  WiFiUDP udp;
  static uint32_t lastNTPupdate = 0;
  static bool started = false;
  static uint32_t prevDiff = 0;
  static IPAddress prevIP;
  uint32_t sendMillis = 0;
  uint32_t origin_sec = 0;
  uint32_t origin_frac = 0;
  IPAddress timeServerIP;

  if( ! started){
    log("timeSync: service started.");
    lastNTPupdate = UNIXtime();
    started = true; 
  }
 
          // The ms clock will rollover after ~49 days.  To be on the safe side,
          // restart the ESP after about 42 days to reset the ms clock.

  if(millis() > 3628800000UL) {
    log("timeSync: Six week routine restart.");
    ESP.restart();
  }

          // Log if no time update for 24 hours.

  if(UNIXtime() - lastNTPupdate > 86400UL){ 
    log("timeSync: No time update in last 24 hours.");
    lastNTPupdate = UNIXtime();
  }

        // Send an SNTP request.

  if(WiFi.isConnected()){
    if(WiFi.hostByName(ntpServerName, timeServerIP) == 1){    // get a random server from the pool
      ntpPacket packet;
      sendMillis = millis();
      packet.trans_ts_sec = origin_sec = sendMillis / 1000;
      packet.trans_ts_frac = origin_frac = sendMillis % 1000;
      udp.begin(ntpPort);
      udp.beginPacket(timeServerIP, 123);
      udp.write((uint8_t*)&packet, sizeof(ntpPacket));        // send an NTP packet to a time server
      udp.endPacket();
    } 
    else {
      return UNIXtime() + RTCrunning ? 60 : 0;
    }
  }
  
        // Poll for completion
        // This is a blocking event, so limit to three seconds.

  while( ! udp.parsePacket()){
    if(millis() - sendMillis > 3000){
      udp.stop();
      return UNIXtime() + (RTCrunning ? 60 : 0);
    }
  }

        // Have a packet,
        // read and reformat to little endian and make fractions milliseconds

  uint32_t recvMillis = millis();
  ntpPacket packet;
  size_t packetSize = udp.read((uint8_t*)&packet,sizeof(ntpPacket));
  udp.stop();
  packet.recv_ts_sec = littleEndian(packet.recv_ts_sec);
  packet.recv_ts_frac = littleEndian(packet.recv_ts_frac) / 4294967UL;
  packet.trans_ts_sec = littleEndian(packet.trans_ts_sec);
  packet.trans_ts_frac = littleEndian(packet.trans_ts_frac) / 4294967UL;

        // Validate packet.

  if(packetSize < sizeof(ntpPacket) || recvMillis - sendMillis > 3000){
    return UNIXtime() + (RTCrunning ? 60 : 0);
  }

        // Check for Kiss-o'-Death packet

  if(packet.stratum == 0){
    //log("timesync: Kiss-o'-Death, code %c%c%c%c, ip: %s", 
    //packet.referenceID[0], packet.referenceID[1], packet.referenceID[2], packet.referenceID[3], timeServerIP.toString().c_str());
    return UNIXtime() + 30;
  } 

  if(packet.origin_ts_sec != origin_sec || packet.origin_ts_frac != origin_frac){
    return UNIXtime() + (RTCrunning ? 60 : 0);
  }
  if(packet.trans_ts_sec < NTP2018 || packet.trans_ts_sec > NTP2028){
    return UNIXtime() + (RTCrunning ? 60 : 0);
  }

        // compute time as NTP transmit time + 1/2 transaction duration.

  uint32_t duration = recvMillis - sendMillis;
  uint32_t current_ts_sec = packet.trans_ts_sec + ((packet.trans_ts_frac + duration / 2) / 1000);
  uint32_t current_ts_frac = (packet.trans_ts_frac + duration / 2) % 1000;

        // Check for seconds adjustment.
        // If so, do it again to verify.

  uint32_t presDiff = current_ts_sec - NTPtime();
  if(presDiff != prevDiff){
    prevDiff = presDiff; 
    prevIP = timeServerIP;
    return UNIXtime() + (RTCrunning ? 60 : 0);
  }
  if(prevDiff){
    if(prevIP == timeServerIP){
      return UNIXtime() + (RTCrunning ? 60 : 0);
    }
    //log("IPs: %s, %s, prevDiff: %d", prevIP.toString().c_str(), timeServerIP.toString().c_str(), prevDiff);
    //log("packet sec: %u, frac: %u",packet.trans_ts_sec, packet.trans_ts_frac);
    //log("Comput sec: %u, frac: %u, duration: %d", current_ts_sec, current_ts_frac, duration);
  } 
  prevDiff = 0;
  
        // Set/adjust internal clock

  timeRefNTP = current_ts_sec - 1;
  timeRefMs = recvMillis - 1000 - current_ts_frac;
  lastNTPupdate = UNIXtime();
 
        // If RTC not running, set it.

  if( ! RTCrunning) {
    programStartTime = UNIXtime();
    rtc.adjust(UNIXtime());
    RTCrunning = true;
    log("timeSync: RTC initalized to NTP time");
  }

        // RTC is running, 
        // check against internal time and adjust if necessary.

  else {
    int32_t timeDiff = UNIXtime() - rtc.now().unixtime();
    if(timeDiff < 0){
      timeDiff += 1;
    } else if(timeDiff > 0){
      timeDiff -= 1;
    }
    if(timeDiff != 0){
      //log("UNIXtime: %u, RTC: %u, timeDiff: %d", UNIXtime(), rtc.now().unixtime(), timeDiff);
      log("timeSync: adjusting RTC by %d", timeDiff);
      rtc.adjust(rtc.now().unixtime() + timeDiff);
    }
  }

          // Go back to sleep.

  return UNIXtime() + timeSynchInterval;    
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