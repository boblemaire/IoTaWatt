#ifndef IotaWatt_h
#define IotaWatt_h

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <AES.h>
#include <CBC.h>
#include <RTClib.h>
#include <SHA256.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <IotaLog.h>

#include "IotaInputChannel.h"
#include "IotaScript.h"
#include "webServer.h"

#define IOTAWATT_VERSION "02_02_20"

#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);

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
     
// Identifiers used to construct id numbers for graph API

#define QUERY_VOLTAGE  1
#define QUERY_POWER  2
#define QUERY_ENERGY 3
            
#define MAXINPUTS 32                          // Arbitrary compile time limit for input channels
#define  SEVENTY_YEAR_SECONDS 2208988800UL
extern const double MS_PER_HOUR;       // useful constant

#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)

// Define the hardware pins

#define pin_CS_ADC0 0                       // Define the hardware SPI chip select pins
#define pin_CS_ADC1 16
#define pin_CS_ADC2 2
#define pin_CS_SDcard 15

#define pin_I2C_SDA 4                       // I2C for rtc.  Wish it were SPI.
#define pin_I2C_SCL 5

#define redLed 16                           // IoTaWatt overusage of pins
#define greenLed 0

#define Vadj_1 38.532                         // IotaWatt 2.1 attenuation at 1.2v Aref (VT input/ADC input)
#define Vadj_3 13                             // IotaWatt 2.1 attenuation at 3.2v Aref

// ADC descriptors

#define ADC_BITS 12
#define ADC_RANGE 4096      // 1 << ADC_BITS      
      
#define DBG_OUTPUT_PORT Serial

enum priorities: byte {priorityLow=3, priorityMed=2, priorityHigh=1};

struct serviceBlock;
struct serviceBlock {                  // Scheduler/Dispatcher list item (see comments in Loop)
  serviceBlock* next;                  // Next serviceBlock in list
  uint32_t callTime;                   // Time (in NTP seconds) to dispatch
  priorities priority;                 // All things equal tie breaker
  uint32_t (*service)(serviceBlock*);  // the SERVICE
  serviceBlock(){next=NULL; callTime=0; priority=priorityMed; service=NULL;}
};

extern uint32_t lastCrossMs;              // Timestamp at last zero crossing (ms) (set in samplePower)
extern uint32_t nextCrossMs;              // Time just before next zero crossing (ms) (computed in Loop)
extern uint32_t nextChannel;              // Next channel to sample (maintained in Loop)

extern serviceBlock* serviceQueue;     // Head of ordered list of services

extern IotaInputChannel* *inputChannel;              // -->s to incidences of input channels (maxInputs entries)
extern uint8_t maxInputs;                 // channel limit based on configured hardware (set in Config)
extern float VrefVolts;                        // Voltage reference shunt value used to calibrate

extern boolean serverAvailable;           // Set false when asynchronous handler active to avoid new requests

extern uint8_t ADC_selectPin[3];

// *********************** EmonCMS configuration stuff *************************
// Note: nee dto move out to a class and change for dynamic configuration
// Start stop is a kludge for now.

enum EmonSendMode {
  EmonSendGET = 1,
  EmonSendPOSTsecure = 2
};

extern bool EmonStarted;                    // set true when Service started
extern bool EmonStop;                       // set true to stop the Service
extern bool EmonInitialize;                  // Initialize or reinitialize EmonService
extern String  EmonURL;                             // These are set from the config file
extern String  EmonURI;
extern String apiKey;
extern uint8_t cryptoKey[16];
extern String node;
extern boolean EmonSecure;
extern String EmonUsername;
extern int16_t EmonBulkSend;
extern enum EmonSendMode EmonSend;
extern ScriptSet* emonOutputs;

//********************** influxDB configuration stuff *****************************//
// again, need to move this stuff to a class.

extern bool influxStarted;
extern bool influxStop;
extern bool influxInitialize;
extern String influxURL;
extern uint16_t influxPort;
extern String influxDataBase;
extern int16_t influxBulkSend;
extern ScriptSet* influxOutputs;      
      
extern uint32_t timeSynchInterval;              // Interval (sec) to roll NTP forward and try to refresh
extern uint32_t dataLogInterval;                // Interval (sec) to invoke dataLog
extern uint32_t EmonCMSInterval;                // Interval (sec) to invoke EmonCMS
extern uint32_t influxDBInterval;               // Interval (sec) to invoke inflexDB 
extern uint32_t statServiceInterval;            // Interval (sec) to invoke statService
extern uint32_t updaterServiceInterval;         // Interval (sec) to check for software updates

extern WiFiManager wifiManager;
extern ESP8266WebServer server;
extern IotaLog iotaLog;                            // instance of IotaLog class
extern RTC_PCF8523 rtc;                            // Instance of RTC_PCF8523
extern Ticker ticker;
extern CBC<AES128> cypher;
extern SHA256 sha256;
extern HTTPClient http;
extern MD5Builder md5;

extern boolean  hasRTC;
extern boolean  RTCrunning;
extern bool hasSD;
extern File uploadFile;

extern char    ledColor[12];                        // Pattern to display led, each char is 500ms color - R, G, Blank
extern uint8_t ledCount;                            // Current index into cycle

// ****************************** Firmware update ****************************

extern const char* updateURL;
extern const char* updateURI;
extern String updateClass;              // NONE, MAJOR, MINOR, BETA, ALPHA, TEST    
extern uint8_t publicKey[];
      
extern String deviceName;                           // can be specified in config.device.name
extern uint16_t deviceVersion;

extern String IotaMsgLog;
extern String IotaLogFile;
extern String EmonPostLogFile;
extern String influxPostLogFile;

extern uint32_t programStartTime;               // Time program started (UnixTime)
extern uint32_t timeRefNTP;
extern uint32_t timeRefMs;                      // Internal MS clock corresponding to timeRefNTP

extern int      localTimeDiff;
extern char     *host;

extern boolean wifiConnected;
extern uint8_t configSHA256[];

extern float frequency;                             // Split the difference to start
extern float samplesPerCycle;                      // Here as well
extern float cycleSampleRate;
extern int16_t cycleSamples;
extern dataBuckets statBucket[MAXINPUTS];

// ****************************** list of output channels **********************

extern ScriptSet* outputs;
      
// ************************ ADC sample pairs ************************************
#define MAX_SAMPLES 1000
extern int16_t samples;                              // Number of samples taken in last sampling
extern int16_t Vsample [MAX_SAMPLES];                    // voltage/current pairs during sampling
extern int16_t Isample [MAX_SAMPLES];

void logTrace(void);
void trace(uint32_t module, int seq); 

// Services
void NewService(uint32_t (*serviceFunction)(struct serviceBlock*));
uint32_t EmonService(struct serviceBlock* _serviceBlock);
uint32_t dataLog(struct serviceBlock* _serviceBlock);
uint32_t statService(struct serviceBlock* _serviceBlock);
uint32_t WiFiService(struct serviceBlock* _serviceBlock);
uint32_t updater(struct serviceBlock* _serviceBlock);
uint32_t handleGetFeedData(struct serviceBlock* _serviceBlock);
uint32_t influxService(struct serviceBlock* _serviceBlock);
  
void dropDead(void);
void setLedState();

boolean getConfig(void);

float sampleVoltage(uint8_t Vchan, float Vcal);  
void samplePower(int channel, int overSample);
float samplePhase(uint8_t Vchan, uint8_t Ichan, uint16_t Ishift);

String base64encode(const uint8_t* in, size_t len);

#endif // !IotaWatt_h
