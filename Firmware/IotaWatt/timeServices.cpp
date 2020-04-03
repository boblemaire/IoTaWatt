#include "IotaWatt.h"

#define NTP2018 (1514796044UL + SECONDS_PER_SEVENTY_YEARS)
#define NTP2028 (1830328844UL + SECONDS_PER_SEVENTY_YEARS)

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
  static uint8_t  serverIndex = 0;     
  uint32_t sendMillis = 0;
  uint32_t origin_sec = 0;
  uint32_t origin_frac = 0;
  IPAddress timeServerIP;

  trace(T_timeSync, 0);
  if( ! started){
    log("timeSync: service started.");
    lastNTPupdate = UTCtime();
    started = true; 
  } 
 
          // The ms clock will rollover after ~49 days.  To be on the safe side,
          // restart the ESP after about 42 days to reset the ms clock.

  trace(T_timeSync, 1);
  if(millis() > 3628800000UL) {
    log("timeSync: Six week routine restart.");
    ESP.restart();
  }

          // Log if no time update for 24 hours.

  trace(T_timeSync, 2);
  if(UTCtime() - lastNTPupdate > 86400UL){ 
    log("timeSync: No time update in last 24 hours.");
    lastNTPupdate = UTCtime();
  }

  if( ! WiFi.isConnected()){ 
    trace(T_timeSync, 3);
    return UTCtime() + (RTCrunning ? 5 : 1);
  }

        // Send an SNTP request.

  trace(T_timeSync, 31);
  String serverName("time1.google.com");
  serverName[4] += (++serverIndex % 4);    
  if(WiFi.hostByName(serverName.c_str(), timeServerIP) == 1){    // get a random server from the pool
    trace(T_timeSync, 32);
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
    trace(T_timeSync, 33);
    return UTCtime() + (RTCrunning ? 60 : 5);
  }
  
        // Poll for completion
        // This is a blocking event, so limit to three seconds.

  trace(T_timeSync, 4);
  while( ! udp.parsePacket()){
    if(millis() - sendMillis > (RTCrunning ? 3000 : 10000)){
      trace(T_timeSync, 42);
      udp.stop();
      return UTCtime() + (RTCrunning ? 60 : 5);
    }
  }

        // Have a packet,
        // read and reformat to little endian and make fractions milliseconds

  trace(T_timeSync, 5);
  uint32_t recvMillis = millis();
  ntpPacket packet;
  size_t packetSize = udp.read((uint8_t*)&packet,sizeof(ntpPacket));
  udp.stop();
  packet.recv_ts_sec = littleEndian(packet.recv_ts_sec);
  packet.recv_ts_frac = littleEndian(packet.recv_ts_frac) / 4294967UL;
  packet.trans_ts_sec = littleEndian(packet.trans_ts_sec);
  packet.trans_ts_frac = littleEndian(packet.trans_ts_frac) / 4294967UL;

        // Validate packet.

  trace(T_timeSync, 6);
  if(packetSize < sizeof(ntpPacket) || recvMillis - sendMillis > (RTCrunning ? 3000 : 10000)){
    return UTCtime() + (RTCrunning ? 60 : 5);
  }

        // Check for Kiss-o'-Death packet

  trace(T_timeSync, 7);
  if(packet.stratum == 0){
    log("timesync: Kiss-o'-Death, code %c%c%c%c, ip: %s", 
    packet.referenceID[0], packet.referenceID[1], packet.referenceID[2], packet.referenceID[3], timeServerIP.toString().c_str());
    return UTCtime() + (RTCrunning ? 60 : 15);
  } 

  trace(T_timeSync, 8);
  if(packet.origin_ts_sec != origin_sec || packet.origin_ts_frac != origin_frac){
    trace(T_timeSync, 81);
    return UTCtime() + (RTCrunning ? 60 : 5);
  }
  if(packet.trans_ts_sec < NTP2018 || packet.trans_ts_sec > NTP2028){
    trace(T_timeSync, 82);
    return UTCtime() + (RTCrunning ? 60 : 5);
  }

        // compute time as NTP transmit time + 1/2 transaction duration.

  uint32_t duration = recvMillis - sendMillis;
  uint32_t current_ts_sec = packet.trans_ts_sec + ((packet.trans_ts_frac + duration / 2) / 1000);
  uint32_t current_ts_frac = (packet.trans_ts_frac + duration / 2) % 1000;

        // Check for seconds adjustment.
        // If so, do it again to verify.

  trace(T_timeSync, 9);
  uint32_t presDiff = current_ts_sec - NTPtime();
  if(presDiff != prevDiff){
    trace(T_timeSync, 91);
    prevDiff = presDiff; 
    prevIP = timeServerIP;
    return UTCtime() + (RTCrunning ? 60 : 1);
  }
  // if(prevDiff){
  //   trace(T_timeSync, 92);
  //   if(prevIP == timeServerIP){
  //     trace(T_timeSync, 93);
  //     return UTCtime() + 20;
  //   }
  //   log("IPs: %s, %s, prevDiff: %d", prevIP.toString().c_str(), timeServerIP.toString().c_str(), prevDiff);
    //log("packet sec: %u, frac: %u",packet.trans_ts_sec, packet.trans_ts_frac);
    //log("Comput sec: %u, frac: %u, duration: %d", current_ts_sec, current_ts_frac, duration);
  // } 
  prevDiff = 0;
  
        // Set/adjust internal clock

  trace(T_timeSync, 10);
  timeRefNTP = current_ts_sec - 1;
  timeRefMs = recvMillis - 1000 - current_ts_frac;
  lastNTPupdate = UTCtime();
 
        // If RTC not running, set it.

  if( ! RTCrunning) {
    trace(T_timeSync, 11);
    programStartTime = UTCtime();
    rtc.adjust(UTCtime());
    RTCrunning = true;
    log("timeSync: RTC initalized to NTP time");
    //SdFile::dateTimeCallback(dateTime);
  }

        // RTC is running, 
        // check against internal time and adjust if necessary.

  else {
    trace(T_timeSync, 12);
    int32_t timeDiff = UTCtime() - rtc.now().unixtime();
    if(timeDiff < 0){
      timeDiff += 1;
    } else if(timeDiff > 0){
      timeDiff -= 1;
    }
    if(timeDiff != 0){
      trace(T_timeSync, 13);
      if(abs(timeDiff > 1)){
        log("timeSync: adjusting RTC by %d", timeDiff);
      }
      rtc.adjust(rtc.now().unixtime() + timeDiff);
    }
  }

          // Check RTC battery.

  Wire.beginTransmission(PCF8523_ADDRESS);            // Read Control registers
  Wire.write(PCF8523_CTL_3);
  Wire.endTransmission();
  Wire.requestFrom(PCF8523_ADDRESS, 1);
  byte Control_3 = Wire.read();
  if(Control_3 & PCF8523_CTL_3_BLF){
    if( ! RTClowBat){
      log("Real Time Clock battery is low.");
    }
    RTClowBat = true;
  }

          // Go back to sleep.

  trace(T_timeSync, 14);
  return UTCtime() + timeSynchInterval;    
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

// void dateTime(uint16_t* date, uint16_t* time) {
   
//   // return date using FAT_DATE macro to format fields
//   *date = FAT_DATE(DateTime(localTime()).year(),
//                    DateTime(localTime()).month(),
//                    DateTime(localTime()).day());

//   // return time using FAT_TIME macro to format fields
//   *time = FAT_TIME(DateTime(localTime()).hour(), 
//                    DateTime(localTime()).minute(),
//                    DateTime(localTime()).second());
// }

/********************************************************************************************
 * 
 *  uint32_t NTPtime() - Return the current time in NTP format
 *  uint32_t Unixtime() - Return the current time in Unix format
 *  uint32_t localTime() - Return the current time adjusted for local time (not true unix time)
 *  Both return zero if the clock is not set//
 *  uint32_t MillisAtUNIXtime - Return the local Millis() value corresponding to the UnixTime
 * 
 *******************************************************************************************/

uint32_t NTPtime() {
  return timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000;
 }
  
uint32_t UTCtime() {
  return timeRefNTP + ((uint32_t)(millis() - timeRefMs)) / 1000 - SECONDS_PER_SEVENTY_YEARS;
}

uint32_t UTCtime(uint32_t localtime){
  return local2UTC(localtime);
}

uint32_t localTime() {
  return UTC2Local(UTCtime());
}

uint32_t localTime(uint32_t utctime){
  return UTC2Local(utctime);
}
 
uint32_t millisAtUTCTime(uint32_t UnixTime){                  
  return (uint32_t)timeRefMs + 1000 * (UnixTime + SECONDS_PER_SEVENTY_YEARS - timeRefNTP);
 }

uint32_t UTC2Local(uint32_t UTCtime){
    uint32_t result = UTCtime + localTimeDiff * 60;
    if( ! timezoneRule) return result;

    if(timezoneRule->begPeriod.month <= timezoneRule->endPeriod.month) {
      if((testRule(timezoneRule->useUTC ? UTCtime : result, timezoneRule->begPeriod) &&
        !testRule(timezoneRule->useUTC ? UTCtime : result, timezoneRule->endPeriod)) &&
        !testRule(timezoneRule->useUTC ? UTCtime : result+timezoneRule->adjMinutes*60, timezoneRule->endPeriod)){ 
        result += timezoneRule->adjMinutes * 60;
      }
    } else {
      result += timezoneRule->adjMinutes * 60;
      if(testRule(timezoneRule->useUTC ? UTCtime : result, timezoneRule->endPeriod)){
        result -= timezoneRule->adjMinutes * 60;
      } 
      if(testRule(timezoneRule->useUTC ? UTCtime : result, timezoneRule->begPeriod)){
        result += timezoneRule->adjMinutes * 60;
      } 
    }
    return result;
}

uint32_t  local2UTC(uint32_t localTime){
  uint32_t trialUTC = localTime - localTimeDiff * 60;
  uint32_t testLocal = UTC2Local(trialUTC);
  return trialUTC - testLocal + localTime;
}

bool testRule(uint32_t standardTime, dateTimeRule dtrule){
    uint8_t daysInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    DateTime now = DateTime(standardTime);
    if(now.month() < dtrule.month) return false;
    if(now.month() > dtrule.month) return true;
    int fwdm = ((standardTime-(now.day()-1)*86400)/86400+4)%7+1;     // weekday of first day in month
    int lwdm = (fwdm+daysInMonth[now.month()-1]-2)%7+1;         // weekday of last day in month
    int startDay;
    if(dtrule.instance > 0){
        startDay = (dtrule.weekday-fwdm+7)%7+1+(dtrule.instance-1)*7;
    } else {
        startDay = daysInMonth[now.month()-1]-(lwdm-dtrule.weekday+7)%7+dtrule.instance*7+7;
    }
    if(now.day() < startDay) return false;
    if(now.day() > startDay) return true;
    return (now.hour()*60+now.minute()) >= dtrule.time;
}