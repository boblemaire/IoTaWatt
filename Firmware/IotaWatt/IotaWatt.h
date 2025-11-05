#ifndef IotaWatt_h
#define IotaWatt_h

   /***********************************************************************************
    IotaWatt Electric Power Monitor System
    Copyright (C) <2021>  <Bob Lemaire, IoTaWatt, Inc.>

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
#define IOTAWATT_VERSION "02_08_04"
#define DEVICE_NAME "IotaWatt"

#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);
#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)
#define RANGE(x,min,max) (x<=min?min:(x>=max?max:x))

#include <Arduino.h>
#include <time.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESPAsyncTCP.h>
#include <asyncHTTPrequest.h>

#include <SPI.h>
#include <RTC.h>
#include <SD.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Ticker.h>

#include "trace.h"
#include "IotaLog.h"
#include "IotaInputChannel.h"
#include "IotaScript.h"

#include <Crypto.h>
#include <AES.h>
#include <SHA256.h>
#include <Ed25519.h>

#include "messageLog.h"
#include "utilities.h"
#include "webServer.h"
#include "updater.h"
#include "samplePower.h"
#include "integrator.h"
#include "auth.h"
#include "spiffs.h"
#include "timeServices.h"
#include "CSVquery.h"
#include "xbuf.h"
#include "xurl.h"
#include "simSolar.h"

      // Declare global instances of classes

extern WiFiClient WifiClient;
extern WiFiManager wifiManager;
extern ESP8266WebServer server;
extern IotaLog Current_log;
extern IotaLog History_log;
extern IotaLog *Export_log;
extern RTC rtc;
extern Ticker Led_timer;
extern messageLog Message_log;
extern simSolar *simsolar;

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_SEVENTY_YEARS 2208988800UL

#define MS_PER_HOUR   3600000UL

      // Declare filename Strings of system files.

#define IOTA_SYSTEM_DIR       "/iotawatt/"      
#define IOTA_EXPORT_LOG_PATH  "/iotawatt/export.log"
#define IOTA_CURRENT_LOG_PATH "/iotawatt/iotalog.log"
#define IOTA_HISTORY_LOG_PATH "/iotawatt/histlog.log"
#define IOTA_MESSAGE_LOG_PATH "/iotawatt/iotamsgs.txt"
#define IOTA_AUTH_PATH        "/iotawatt/auth.txt"
#define IOTA_CONFIG_PATH      "/config.txt"
#define IOTA_CONFIG_NEW_PATH  "/config+1.txt"
#define IOTA_CONFIG_OLD_PATH  "/config-1.txt"
#define IOTA_TABLE_PATH       "/tables.txt"
#define IOTA_NEW_TABLE_PATH   "/table+1.txt"
#define IOTA_INTEGRATIONS_DIR "/iotawatt/integrations/"
#define IOTA_UPDATE_HOST      "iotawatt.com"
#define IOTA_VERSIONS_PATH    "/firmware/versions.json"
#define IOTA_VERSIONS_DIR     "/firmware/bin/"     
#define IOTA_TABLE_DIR        "/download/tables/"

extern char* deviceName;

        // Define the hardware pins


#define pin_CS_ADC0 0                       // Define the hardware SPI chip select pins
#define pin_CS_ADC1 2
#define pin_CS_SDcard 15

#define pin_I2C_SDA 4                       // I2C for rtc.  Wish it were SPI.
#define pin_I2C_SCL 5

#define redLed 16                           // IoTaWatt overusage of pins
#define greenLed 0

extern uint8_t ADC_selectPin[2];            // indexable reference for ADC select pins

      // Structure of EEPROM

struct EEprom {
      char        id[8];                  // Valid EE area identifier "IoTaWatt"
      uint8_t     EEversion;              // Version of this EE (higher is superset)
      uint8_t     deviceMajorVersion;     // Major version of PCB as "4" in 4.8
      uint8_t     deviceMinorVersion;     // Minor version of PCB as "8" in 4.8
      uint8_t     mfgBurden;              // Burden resistor value at manufacture
      uint16_t    mfgRefVolts;            // Voltage reference at manufacture in mV
      uint16_t    reserved;               // alignment
      uint32_t    mfgDate;                // Unix time          
      uint32_t    mfgLot;                 // Manufacturing lot
      };

      // Identifiers used to construct id numbers for graph API

#define QUERY_VOLTAGE  1
#define QUERY_POWER  2
#define QUERY_ENERGY 3
#define QUERY_OTHER 4

      // LED codes

#define LED_CONNECT_WIFI            "R.G.G..."              // Connecting to WiFi, AP active
#define LED_CONNECT_WIFI_NO_RTC     "R.R.G..."              // Connecting to WiFi, AP active, nofail
#define LED_SD_INIT_FAILURE         "G.R.R..."              // SD initialization failed
#define LED_DUMPING_LOG             "R.G.R..."              // Dtatlog damage, creating diagnostic file
#define LED_HALT                    "R.R.R..."              // Fatal error, IoTaWatt halted
#define LED_NO_CONFIG               "G.R.R.R..."            // No configuration file found
#define LED_BAD_CONFIG              "G.R.R.G..."            // Could not parse config file
#define LED_UPDATING                "R.G."                  // Downloading new release

      // ADC descriptors

#define ADC_BITS 12
#define ADC_RANGE 4096      // 2^12

extern uint32_t firstCrossUs;          // Time cycle at usec resolution for phase calculation
extern uint32_t lastCrossUs;
extern uint32_t bingoTime;

enum priorities: byte { priorityLow=2, 
                        priorityLM=3, 
                        priorityML=4,
                        priorityMed=5,
                        priorityMH=6,
                        priorityHM=7,
                        priorityHigh=8
                      };
typedef std::function<uint32_t(struct serviceBlock*)> Service;
struct serviceBlock {                  // Scheduler/Dispatcher list item (see comments in Loop)
  serviceBlock* next;                  // Next serviceBlock in list
  uint32_t scheduleTime;               // Time in millis to dispatch
  Service service;                     // the Service function
  void *serviceParm;                   // Service specific parameter   
  priorities priority;                 // All things equal tie breaker
  uint8_t   taskID;
  serviceBlock(){next=NULL; scheduleTime=1; priority=priorityMed; service=NULL; taskID=0;}
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
extern uint8_t  deviceMajorVersion;           // Major version of hardware 
extern uint8_t  deviceMinorVersion;           // Minor version of hardware 
extern float    VrefVolts;                    // Voltage reference shunt value used to calibrate
                                              // the ADCs. (can be specified in config.device.refvolts)
extern int16_t* masterPhaseArray;             // Single array containing all individual phase shift arrays    
#define Vadj_3 13                             // Voltage channel attenuation ratio

      // ****************************************************************************
      // statService maintains current averages of the channel values
      // so that current values can be displayed by web clients
      // statService runs at low frequency but is reved up by the web server
      // handlers if the statistics are used.

extern float   frequency;                             // Split the difference to start
extern float   configFrequency;                       // Frequency at last config (phase corrrection basis)         
extern float   samplesPerCycle;                       // Here as well
extern float   cycleSampleRate;
extern int16_t cycleSamples;
extern float   heapMs;
extern uint32_t heapMsPeriod;
extern IotaLogRecord statRecord;

      // ************* lists of output channels, integrations and integrators **************

extern ScriptSet *outputs;
extern ScriptSet *integrations;

// ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
extern bool     hasSD;
extern File     uploadFile;
extern SHA256*  uploadSHA;  
extern boolean  serverAvailable;          // Set false when asynchronous handler active to avoid new requests
extern uint32_t wifiConnectTime;          // Time of connection (zero if disconnected)
extern uint8_t  configSHA256[32];         // Hash of config file
extern bool     getNewConfig;             // Set to update config after running WebServer

#define HTTPrequestMax 1                  // Maximum number of concurrent HTTP requests  
extern int16_t  HTTPrequestFree;          // Request semaphore
extern uint32_t HTTPrequestStart[HTTPrequestMax]; // request start time tokens
extern uint16_t HTTPrequestId[HTTPrequestMax];    // Module ID of requestor
extern uint32_t HTTPlock;                 // start time token of locking request  

      // ************************** HTTPS proxy host ******************************************

extern char *HTTPSproxy;                  // Host for nginx (or other) reverse HTTPS proxy server
extern int32_t uploaderBufferLimit;       // Dynamic limit to try to control overload during recovery
extern int32_t uploaderBufferTotal;       // Total aggregate target of uploader buffers       

      // ******************* WiFi connection  *************************************

extern uint32_t subnetMask;
extern uint32_t gatewayIP;
extern uint32_t localIP;

// ******************* Password and authorization data *************************************

extern uint8_t*   adminH1;                // H1 digest md5("admin":"admin":password) 
extern uint8_t*   userH1;                 // H1 digest md5("user":"user":password)
extern authSession* authSessions;         // authSessions list head; 
extern uint16_t   authTimeout;            // Timeout interval of authSession in seconds
extern bool       localAccess;            // true if password not required for local access

// ****************************** Timing and time data *************************
#define  SECONDS_PER_SEVENTY_YEARS 2208988800UL
extern int32_t  localTimeDiff;                 // Local time Difference in minutes
extern tzRule*  timezoneRule;                  // Rule for DST 
extern uint32_t programStartTime;;             // Time program started (UnixTime)
extern uint32_t timeRefNTP;                    // Last time from NTP server (NTPtime)
extern uint32_t timeRefMs;                     // Internal MS clock corresponding to timeRefNTP
extern uint32_t timeSynchInterval;             // Interval (sec) to roll NTP forward and try to refresh
extern uint32_t statServiceInterval;           // Interval (sec) to invoke statService
extern uint32_t updaterServiceInterval;        // Interval (sec) to check for software updates

extern bool     hasRTC;
extern bool     RTCrunning;
extern bool     powerFailRestart;               // Set true on power fail restart (detected by RTC)
extern bool     validConfig;                    // Config exists and Json parses first level     
extern bool     RTClowBat;                      // Set true when battery is low
extern bool     sampling;                       // All channels have been sampled  

extern char     ledColor[12];                   // Pattern to display led, each char is 500ms color - R, G, Blank
extern uint8_t  ledCount;                       // Current index into cycle

      // ****************************** Firmware update ****************************

extern char*          updateClass;            // NONE, MAJOR, MINOR, BETA, ALPHA, TEST
extern const uint8_t  publicKey[32];
extern const char     hexcodes_P[];
extern const char     base64codes_P[];
extern long           tableVersion;

// ************************ ADC sample pairs ************************************

#define MAX_SAMPLES 1000

extern uint32_t sumVsq;                           // sampleCycle will compute these while collecting samples    
extern uint32_t sumIsq;
extern int32_t  sumVI;
extern int16_t  samples;                          // Number of samples taken in last sampling
extern int16_t  Vsample [MAX_SAMPLES];            // voltage/current pairs during sampling
extern int16_t  Isample [MAX_SAMPLES];

      // ************************ Declare global functions
void      setup();
void      loop();

serviceBlock* NewService(Service, const uint8_t taskID=0, void* parm=0);
void      AddService(struct serviceBlock*);
uint32_t  dataLog(struct serviceBlock*);
uint32_t  historyLog(struct serviceBlock*);
uint32_t  statService(struct serviceBlock*);
uint32_t  EmonService(struct serviceBlock*);
uint32_t  influxService(struct serviceBlock*);
uint32_t  timeSync(struct serviceBlock*);
uint32_t  updater(struct serviceBlock*);
uint32_t  WiFiService(struct serviceBlock*);
uint32_t  exportLog(struct serviceBlock *_serviceBlock);
uint32_t  getFeedData(); //(struct serviceBlock*);

uint32_t  logReadKey(IotaLogRecord* callerRecord);

void      setLedCycle(const char*);
void      endLedCycle();
void      ledBlink();
void      setLedState();

void      dropDead(void);
void      dropDead(const char*);

bool      setConfig(const char* configPath);
bool      updateConfig(const char *configPath);
bool      recoverConfig();

size_t    sendChunk(char* buf, size_t bufPos);

uint32_t  HTTPreserve(uint16_t id, bool lock = false);
void      HTTPrelease(uint32_t HTTPtoken);

void      getSamples();
double    simSolarPower(uint32_t);
double    simSolarEnergy(uint32_t, uint32_t);

#endif
