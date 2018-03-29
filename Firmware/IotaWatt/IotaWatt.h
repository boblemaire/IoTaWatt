#ifndef IotaWatt_h
#define IotaWatt_h

   /***********************************************************************************
    IotaWatt Electric Power Monitor System
    Copyright (C) <2017>  <Bob Lemaire, IoTaWatt, Inc.>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.  
   
***********************************************************************************/
#define IOTAWATT_VERSION "02_03_03"

#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);
#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiClient.h>
//#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
//#include <ESP8266httpUpdate.h>
#include <ESPAsyncTCP.h>
#include <asyncHTTPrequest.h>

#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Ticker.h>

#include "IotaLog.h"
#include "IotaInputChannel.h"
#include "IotaScript.h"

#include <Crypto.h>
#include <AES.h>
#include <CBC.h>
#include <SHA256.h>
#include <Ed25519.h>

#include "utilities.h"
#include "msgLog.h"
#include "webServer.h"
#include "updater.h"
#include "samplePower.h"
#include "influxDB.h"
#include "Emonservice.h"

      // Declare instances of major classes

extern WiFiClient WifiClient;
extern WiFiManager wifiManager;
extern ESP8266WebServer server;
extern DNSServer dnsServer;
extern IotaLog currLog;
extern IotaLog histLog;
extern RTC_PCF8523 rtc;
extern Ticker ticker;

#define MS_PER_HOUR   3600000UL
#define SEVENTY_YEAR_SECONDS  2208988800UL

      // Declare filename Strings of system files.

extern String deviceName;
extern String IotaLogFile;
extern String historyLogFile;
extern String IotaMsgLog;
extern String EmonPostLogFile;
extern String influxPostLogFile;
extern const char* ntpServerName;
extern uint16_t deviceVersion;

        // Define the hardware pins

#define pin_CS_ADC0 0                       // Define the hardware SPI chip select pins
#define pin_CS_ADC1 2
#define pin_CS_SDcard 15

#define pin_I2C_SDA 4                       // I2C for rtc.  Wish it were SPI.
#define pin_I2C_SCL 5

#define redLed 16                           // IoTaWatt overusage of pins
#define greenLed 0

extern uint8_t ADC_selectPin[2];            // indexable reference for ADC select pins

      // Trace context and work area

union traceUnion {
      uint32_t    traceWord;
      struct {
            uint16_t    seq;
            uint8_t     mod;
            uint8_t     id;
      };
};

extern traceUnion traceEntry;

      // Identifiers used to construct id numbers for graph API

#define QUERY_VOLTAGE  1
#define QUERY_POWER  2
#define QUERY_ENERGY 3
#define QUERY_OTHER 4

     // RTC trace trace module values by module. (See trace routines in Loop tab)

#define T_LOOP 1           // Loop
#define T_LOG 2            // dataLog
#define T_Emon 3           // EmonService
#define T_GFD 4            // GetFeedData
#define T_UPDATE 5         // updater
#define T_SETUP 6          // Setup
#define T_influx 7         // influxDB
#define T_SAMP 8           // sampleCycle
#define T_POWER 9          // Sample Power
#define T_WEB 10           // (30)Web server handlers
#define T_CONFIG 11        //  Get Config
#define T_encryptEncode 12 //  base64encode and encryptData in EmonService
#define T_uploadGraph 13 
#define T_history 14
#define T_base64 15        // base 64 encode
#define T_EmonConfig 16    // Emon configuration                  

      // ADC descriptors

#define ADC_BITS 12
#define ADC_RANGE 4096      // 2^12

extern uint32_t lastCrossMs;           // Timestamp at last zero crossing (ms) (set in samplePower)
extern uint32_t nextCrossMs;           // Time just before next zero crossing (ms) (computed in Loop)
extern uint32_t nextChannel;           // Next channel to sample (maintained in Loop)

enum priorities: byte {priorityLow=3, priorityMed=2, priorityHigh=1};

struct serviceBlock {                  // Scheduler/Dispatcher list item (see comments in Loop)
  serviceBlock* next;                  // Next serviceBlock in list
  uint32_t callTime;                   // Time (in NTP seconds) to dispatch
  priorities priority;                 // All things equal tie breaker
  uint32_t (*service)(serviceBlock*);  // the SERVICE
  serviceBlock(){next=NULL; callTime=0; priority=priorityMed; service=NULL;}
};

extern serviceBlock* serviceQueue;     // Head of ordered list of services

      // Define maximum number of input channels.
      // Create pointer for array of pointers to incidences of input channels
      // Initial values here are defaults for IotaWatt 2.1.
      // VrefVolts is the declared value of the voltage reference shunt,
      // Can be specified in config.device.aref
      // Voltage adjustments are the values for AC reference attenuation in IotaWatt 2.1.

#define MAXINPUTS 15                          // Compile time input channels, can't be changed easily 
extern IotaInputChannel* *inputChannel;       // -->s to incidences of input channels (maxInputs entries)
extern uint8_t  maxInputs;                    // channel limit based on configured hardware (set in Config)
extern float    VrefVolts;                    // Voltage reference shunt value used to calibrate
                                              // the ADCs. (can be specified in config.device.refvolts)
#define Vadj_3 13                             // Voltage channel attenuation ratio

      // ****************************************************************************
      // statService maintains current averages of the channel values
      // so that current values can be displayed by web clients
      // statService runs at low frequency but is reved up by the web server
      // handlers if the statistics are used.

extern float   frequency;                             // Split the difference to start
extern float   samplesPerCycle;                       // Here as well
extern float   cycleSampleRate;
extern int16_t cycleSamples;
extern IotaLogRecord statRecord;

      // ****************************** list of output channels **********************

extern ScriptSet* outputs;

      // ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
extern String   host;
extern bool     hasSD;
extern File     uploadFile;
extern SHA256*  uploadSHA;  
extern boolean  serverAvailable;          // Set false when asynchronous handler active to avoid new requests
extern boolean  wifiConnected;
extern uint8_t  configSHA256[32];         // Hash of config file

      // ****************************** Timing and time data *************************
#define  SEVENTY_YEAR_SECONDS 2208988800UL
extern int      localTimeDiff;
extern uint32_t programStartTime;;               // Time program started (UnixTime)
extern uint32_t timeRefNTP;  // Last time from NTP server (NTPtime)
extern uint32_t timeRefMs;                     // Internal MS clock corresponding to timeRefNTP
extern uint32_t timeSynchInterval;           // Interval (sec) to roll NTP forward and try to refresh
extern uint32_t EmonCMSInterval;               // Interval (sec) to invoke EmonCMS
extern uint32_t influxDBInterval;              // Interval (sec) to invoke inflexDB
extern uint32_t statServiceInterval;           // Interval (sec) to invoke statService
extern uint32_t updaterServiceInterval;     // Interval (sec) to check for software updates

extern bool     hasRTC;
extern bool     RTCrunning;

extern char     ledColor[12];                         // Pattern to display led, each char is 500ms color - R, G, Blank
extern uint8_t  ledCount;                             // Current index into cycle

      // ****************************** Firmware update ****************************
extern String   updateURL;
extern String   updatePath;
extern String   updateClass;            // NONE, MAJOR, MINOR, BETA, ALPHA, TEST
extern uint8_t  publicKey[32];

      // ************************ ADC sample pairs ************************************

#define MAX_SAMPLES 900
extern int16_t samples;                           // Number of samples taken in last sampling
extern int16_t Vsample [MAX_SAMPLES];             // voltage/current pairs during sampling
extern int16_t Isample [MAX_SAMPLES];

      // ************************ Declare global functions
void      setup();
void      loop();
void      trace(const uint8_t module, const uint8_t id);
void      logTrace(void);

void      NewService(uint32_t (*serviceFunction)(struct serviceBlock*));
void      AddService(struct serviceBlock*);
uint32_t  dataLog(struct serviceBlock*);
uint32_t  historyLog(struct serviceBlock*);
uint32_t  statService(struct serviceBlock*);
uint32_t  EmonService(struct serviceBlock*);
uint32_t  influxService(struct serviceBlock*);
uint32_t  timeSync(struct serviceBlock*);
uint32_t  updater(struct serviceBlock*);
uint32_t  WiFiService(struct serviceBlock*);
uint32_t  getFeedData(struct serviceBlock*);

uint32_t  logReadKey(IotaLogRecord* callerRecord);

void      setLedCycle(const char*);
void      endLedCycle();
void      ledBlink();
void      setLedState();

void      dropDead(void);
void      dropDead(const char*);

uint32_t  NTPtime();
uint32_t  UNIXtime();
uint32_t  MillisAtUNIXtime(uint32_t);
void      dateTime(uint16_t* date, uint16_t* time);
String    timeString(int value);

boolean   getConfig(void);

void      sendChunk(char* bufr, uint32_t bufrPos);

#endif
