#ifndef IotaWatt_h
#define IotaWatt_h

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <AES.h>
#include <CBC.h>
#include <SHA256.h>

#include "IotaInputChannel.h"
#include "IotaList.h"

#define IOTAWATT_VERSION "2.02.15"

#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);

// RTC trace trace module values by module. (See trace routines in Loop tab)

#define T_SETUP 60          // Setup
#define T_LOOP 10           // Loop
#define T_LOG 20            // dataLog
#define T_Emon 30           // EmonService
#define T_GFD 40            // GetFeedData
#define T_SAMP 100          // samplePower
#define T_UPDATE 50         // updater
#define T_TEMP 120

#define MAXINPUTS 32                          // Arbitrary compile time limit for input channels

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
extern uint32_t EmonCMSInterval;               // Interval (sec) to invoke EmonCMS

extern IotaList outputList;

extern ESP8266WebServer server;
extern CBC<AES128> cypher;
extern SHA256 sha256;
extern HTTPClient http;

extern boolean  hasRTC;
extern boolean  RTCrunning;
extern bool hasSD;

extern char    ledColor[12];                        // Pattern to display led, each char is 500ms color - R, G, Blank
extern uint8_t ledCount;                            // Current index into cycle

extern String deviceName;                           // can be specified in config.device.name
extern uint16_t deviceVersion;
extern String IotaMsgLog;

extern uint32_t timeRefNTP;

extern int      localTimeDiff;
extern char     host[];

void trace(uint32_t module, int seq); 

void NewService(uint32_t (*serviceFunction)(struct serviceBlock*));

uint32_t EmonService(struct serviceBlock* _serviceBlock);
  
void setLedState();

#endif // !IotaWatt_h
