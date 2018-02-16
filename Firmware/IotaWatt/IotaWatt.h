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
#define IOTAWATT_VERSION "02_02_30"

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
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
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

#include "msgLog.h"
#include "webServer.h"
#include "updater.h"
#include "samplePower.h"

      // Declare instances of major classes

extern WiFiClient WifiClient;
extern WiFiManager wifiManager;
extern ESP8266WebServer server;
extern DNSServer dnsServer;
extern IotaLog currLog;
extern IotaLog histLog;
extern RTC_PCF8523 rtc;
extern Ticker ticker;
extern CBC<AES128> cypher;
extern SHA256 sha256;
extern HTTPClient http;
extern MD5Builder md5;

#define MS_PER_HOUR   3600000UL
#define SEVENTY_YEAR_SECONDS  2208988800UL

      // Declare filename Strings of system files.

extern String deviceName;
extern String IotaLogFile;
extern String historyLogFile;
extern String IotaMsgLog;
extern String EmonPostLogFile;
extern String influxPostLogFile;
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

      // Identifiers used to construct id numbers for graph API

#define QUERY_VOLTAGE  1
#define QUERY_POWER  2
#define QUERY_ENERGY 3

     // RTC trace trace module values by module. (See trace routines in Loop tab)

#define T_LOOP 10           // Loop
#define T_LOG 20            // dataLog
#define T_Emon 30           // EmonService
#define T_GFD 40            // GetFeedData
#define T_UPDATE 50         // updater
#define T_SETUP 60          // Setup
#define T_influx 70         // influxDB
#define T_SAMP 80           // sampleCycle
#define T_POWER 90          // Sample Power
#define T_WEB 100           // (30)Web server handlers
#define T_CONFIG 130        //  Get Config
#define T_encryptEncode 140 //  base64encode and encryptData in EmonService
#define T_uploadGraph 150 
#define T_history 160           

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

#define MAXINPUTS 15                          // Arbitrary compile time limit for input channels
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
extern dataBuckets statBucket[MAXINPUTS];

      // ****************************** list of output channels **********************

extern ScriptSet* outputs;

      // ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
extern String   host;
extern bool     hasSD;
extern File     uploadFile;
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
extern String   updateURI;
extern String   updateClass;            // NONE, MAJOR, MINOR, BETA, ALPHA, TEST
extern uint8_t  publicKey[32];

      // *********************** EmonCMS configuration stuff *************************
      // Note: nee dto move out to a class and change for dynamic configuration
      // Start stop is a kludge for now.

extern bool     EmonStarted;                      // set true when Service started
extern bool     EmonStop;                         // set true to stop the Service
extern bool     EmonInitialize;                   // Initialize or reinitialize EmonService
extern String   EmonURL;                          // These are set from the config file
extern uint16_t  EmonPort;
extern String   EmonURI;
extern String   apiKey;
extern uint8_t  cryptoKey[16];
extern String   node;
extern boolean  EmonSecure;
extern String   EmonUsername;
extern int16_t  EmonBulkSend;
enum EmonSendMode {
  EmonSendGET = 1,
  EmonSendPOSTsecure = 2
};
extern EmonSendMode EmonSend;
extern ScriptSet* emonOutputs;

      //********************** influxDB configuration stuff *****************************//
      // again, need to move this stuff to a class.

extern bool     influxStarted;                    // set true when Service started
extern bool     influxStop;                       // set true to stop the Service
extern bool     influxInitialize;                 // Initialize or reinitialize
extern String   influxURL;
extern uint16_t influxPort;
extern String   influxDataBase;
extern int16_t  influxBulkSend;
extern ScriptSet* influxOutputs;

      // ************************ ADC sample pairs ************************************

#define MAX_SAMPLES 1000
extern int16_t samples;                           // Number of samples taken in last sampling
extern int16_t Vsample [MAX_SAMPLES];             // voltage/current pairs during sampling
extern int16_t Isample [MAX_SAMPLES];

      // ************************ Declare global functions
void      setup();
void      loop();
void      trace(uint32_t, int);
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

boolean   getConfig(void);

void      sendChunk(char* bufr, uint32_t bufrPos);
String    base64encode(const uint8_t* in, size_t len);

String    hashName(const char* name);
String    formatHex(uint32_t);
#endif
