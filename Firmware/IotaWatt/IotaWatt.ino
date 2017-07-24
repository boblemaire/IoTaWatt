
      /***********************************************************************************
      MIT License
      
      Copyright (c) [2016] [Bob Lemaire]
      
      Permission is hereby granted, free of charge, to any person obtaining a copy
      of this software and associated documentation files (the "Software"), to deal
      in the Software without restriction, including without limitation the rights
      to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
      copies of the Software, and to permit persons to whom the Software is
      furnished to do so, subject to the following conditions:
      
      The above copyright notice and this permission notice shall be included in all
      copies or substantial portions of the Software.
      
      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
      FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
      AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
      LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
      OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
      SOFTWARE.
      ***********************************************************************************/

#define IOTAWATT_VERSION "2.02.08"

#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);
#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)


#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266httpUpdate.h>
#include <SD.h>
#include <WiFiUDP.h>
#include <ArduinoJson.h>
#include "IotaLog.h"
#include "IotaInputChannel.h"
#include "IotaOutputChannel.h"
#include "IotaList.h"
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include <Ticker.h>

      // Declare instances of various classes above

WiFiClientSecure WifiClientSecure;
WiFiClient WifiClient;
WiFiManager wifiManager;
DNSServer dnsServer;    
IotaLog iotaLog;                            // instance of IotaLog class
RTC_PCF8523 rtc;                            // Instance of RTC_PCF8523
Ticker ticker;

const int HttpsPort = 443;
const double MS_PER_HOUR = 3600000UL;       // useful constant

      // Collection of filenames that IotaWatt uses.

String deviceName = "IotaWatt";             // can be specified in config.device.name
String IotaLogFile = "/IotaWatt/IotaLog";
String IotaMsgLog = "/IotaWatt/IotaMsgs.txt";
String eMonPostLogFile = "/iotawatt/emonlog.log";
uint16_t deviceVersion = 0;

        // Define the hardware pins

#define pin_CS_ADC0 0                       // Define the hardware SPI chip select pins
#define pin_CS_ADC1 16
#define pin_CS_ADC2 2
#define pin_CS_SDcard 15

#define pin_I2C_SDA 4                       // I2C for rtc.  Wish it were SPI.
#define pin_I2C_SCL 5
                    
#define redLed 16                           // IoTaWatt overusage of pins
#define greenLed 0


const int chipSelect = pin_CS_SDcard;       // for the benefit of SD.h
                            
uint8_t ADC_selectPin[3] = {pin_CS_ADC0,    // indexable reference for ADC select pins
                            pin_CS_ADC1,
                            pin_CS_ADC2};  

      // Identifiers used to construct id numbers for graph API

#define QUERY_VOLTAGE  1
#define QUERY_POWER  2
#define QUERY_ENERGY 3

     // RTC trace trace module values by module. (See trace routines in Loop tab)

#define T_SETUP 60          // Setup
#define T_LOOP 10           // Loop
#define T_LOG 20            // dataLog
#define T_EMON 30           // eMonService
#define T_GFD 40            // GetFeedData
#define T_SAMP 100          // samplePower
#define T_UPDATE 50         // updater
#define T_TEMP 120

      // ADC descriptors

#define ADC_BITS 12
#define ADC_RANGE 4096      // 1 << ADC_BITS      

      /**************************************************************************************************
       * Core dispatching parameters - There's a lot going on, but the steady rhythm is sampling the
       * power channels, and that has to be done on their schedule - the AC frequency.  During sampling,
       * the time (in ms) of the last zero crossing is saved here.  Upon return to "Loop", the estimated
       * time just before the next crossing is computed.  That's when samplePower will be called again.
       * We try to run everything else during the half-wave intervals between power sampling.  The next 
       * channel to be sampled is also kept here to complete the picture.  
       **************************************************************************************************/
       
uint32_t lastCrossMs = 0;             // Timestamp at last zero crossing (ms) (set in samplePower)
uint32_t nextCrossMs = 0;             // Time just before next zero crossing (ms) (computed in Loop)
uint32_t nextChannel = 0;             // Next channel to sample (maintained in Loop)

enum priorities: byte {priorityLow=3, priorityMed=2, priorityHigh=1};
 
struct serviceBlock {                  // Scheduler/Dispatcher list item (see comments in Loop)
  serviceBlock* next;                  // Next serviceBlock in list
  uint32_t callTime;                   // Time (in NTP seconds) to dispatch
  priorities priority;                 // All things equal tie breaker
  uint32_t (*service)(serviceBlock*);  // the SERVICE
  serviceBlock(){next=NULL; callTime=0; priority=priorityMed; service=NULL;}
};
  
serviceBlock* serviceQueue = NULL;     // Head of ordered list of services

      // Define maximum number of input channels. 
      // Create pointer for array of pointers to incidences of input channels
      // Initial values here are defaults for IotaWatt 2.1.
      // VrefVolts is the declared value of the voltage reference shunt,
      // Can be specified in config.device.aref
      // Voltage adjustments are the values for AC reference attenuation in IotaWatt 2.1.    

#define MAXINPUTS 32                          // Arbitrary compile time limit for input channels
IotaInputChannel* *inputChannel;              // -->s to incidences of input channels (maxInputs entries)
uint8_t maxInputs = 0;                        // channel limit based on configured hardware (set in Config)
float VrefVolts = 1.0;                        // Voltage reference shunt value used to calibrate
                                              // the ADCs. (can be specified in config.device.refvolts)
#define Vadj_1 38.532                         // IotaWatt 2.1 attenuation at 1.2v Aref (VT input/ADC input)
#define Vadj_3 13                             // IotaWatt 2.1 attenuation at 3.2v Aref            
 
      // ****************************************************************************
      // statService maintains current averages of the channel values
      // so that current values can be displayed by web clients
      // statService runs at low frequency but is reved up by the web server 
      // handlers if the statistics are used.

float frequency = 55;                             // Split the difference to start
float samplesPerCycle = 550;                      // Here as well
float cycleSampleRate = 0;
int16_t cycleSamples = 0;
dataBuckets statBucket[MAXINPUTS];

      // ****************************** list of output channels **********************

IotaList outputList; 

      // ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
char* host = "IotaWatt";
ESP8266WebServer server(80);
static bool hasSD = false;
File uploadFile;
void handleNotFound();
boolean serverAvailable = true;   // Set false when asynchronous handler active to avoid new requests
boolean wifiConnected = false;

      // ****************************** Timing and time data *************************
#define  SEVENTY_YEAR_SECONDS 2208988800UL
int      localTimeDiff = 0;
uint32_t programStartTime = 0;               // Time program started (UnixTime)
uint32_t timeRefNTP = SEVENTY_YEAR_SECONDS;  // Last time from NTP server (NTPtime)
uint32_t timeRefMs = 0;                      // Internal MS clock corresponding to timeRefNTP
uint32_t timeSynchInterval = 3600;           // Interval (sec) to roll NTP forward and try to refresh
uint32_t dataLogInterval = 5;                // Interval (sec) to invoke dataLog
uint32_t eMonCMSInterval = 10;               // Interval (sec) to invoke eMonCMS 
uint32_t statServiceInterval = 1;            // Interval (sec) to invoke statService
uint32_t updaterServiceInterval = 60*60;     // Interval (sec) to check for software updates 

boolean  hasRTC = false;
boolean  RTCrunning = false;

char    ledColor[12];                         // Pattern to display led, each char is 500ms color - R, G, Blank
uint8_t ledCount;                             // Current index into cycle

      // *********************** eMonCMS configuration stuff *************************
      // Note: nee dto move out to a class and change for dynamic configuration
      // Start stop is a kludge for now.
bool eMonStarted = false;                    // set true when Service started
bool eMonStop = false;                       // set true to stop the Service                                         
String  eMonURL;                             // These are set from the config file 
String apiKey;
String node = "IotaWatt";
boolean eMonSecure = false;
int16_t eMonBulkSend = 1;
const char* eMonSHA1 = "A2 FB AA 81 59 E2 B5 12 10 5D 38 22 23 A7 4E 74 B0 11 7D AA";

      // ************************ ADC sample pairs ************************************
 
int16_t samples = 0;                              // Number of samples taken in last sampling
#define MAX_SAMPLES 1000
int16_t Vsample [MAX_SAMPLES];                    // voltage/current pairs during sampling
int16_t Isample [MAX_SAMPLES];

      // I can't remove this unused function because the compiler goes berzerk.

void wtf(){} 


