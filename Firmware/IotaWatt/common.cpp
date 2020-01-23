#include "IotaWatt.h"
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
/*********************************** Change log ****************************************************
 *  
 *   03/05/17 2.00.01 Cleaned up and added more documentation to Sample Power.  Also streamlined
 *                    it a little more and fixed a few loose ends.
 *   03/08/17 2.00.02 Recognize /edit and /graph uri in server
 *   03/10/17 2.00.03 API performance enhancement.  Add L1 index cache by full block buffering.
 *   03/12/17 2.00.04 Insist on WiFi connect at startup.
 *   03/12/17 2.00.05 Update frequency, samples/cycle in voltage only sample.
 *   03/17/17 2.00.06 Use ArduinoJson to generate server responses
 *                    Increase Emoncms retry interval to 30 seconds 
 *   03/18/17 2.00.08 Fix rounding in API Json output
 *   03/18/17 2.00.09 Fix typo in json rework
 *   03/18/17 2.00.10 Change wifi retry interval
 *   
 *   04/14/17 2.01.00 Major update - move inputs to a class, add outputs, CSS
 *   04/21/17 2.01.01 Add polyphase correction with single VT
 *   05/06/17 2.01.02 Rework samplePower for better phase correction
 *   05/28/17 2.01.03 Add automatic update and rework WiFiManager use
 *   06/04/17 2.01.04 Miscelaneous cleanup
 *   06/26/17 2.01.05 Ongoing development
 *   07/09/17 2.02.00 Version 4 hardware support
 *   07/12/17 2.02.01 Fix sample power, enhance graph
 *   07/15/17 2.02.02 Enhance status display
 *   07/17/17 2.02.03 Fix problems with sample power & multiple V channels
 *   07/19/17 2.02.04 Changes to Emoncms support 
 *   07/19/17 2.02.05 Bump version
 *   07/19/17 2.02.06 Accept Emoncms or Emoncms (compatibility)
 *   07/23/17 2.02.07 Overhaul RTC initialization and power fail logging 
 *   07/23/17 2.02.08 Add LED problem indicators during startup
 *   07/23/17 2.02.09 Add LED indication of connection and timer status
 *   08/07/17 2.02.10 Add Emonpi URL support
 *   08/11/17 2.02.11 Upgrade to new script format (requires new index.htm)
 *   08/12/17 2.02.12 Change WiFi pwd to device name. Filter ADC lsb noise.
 *   08/15/17 2.02.13 Add secure encrypted posting to Emoncms
 *   08/16/17 2.02.14 Fix problem changing Emoncms method on the fly
 *   08/18/17 2.02.15 Finalize encrypted Emoncms post
 *   09/12/17 2.02.16 Signed firmware release update
 *   09/14/17 2.02.17 Version bump to transition to new update system
 *   09/17/17 02_02_18 Encrypted Emoncms, new Script, influxDB, Emoncms outputs
 *   09/21/17 02_02_19 Bug fixes, add config context check to app
 *   09/24/17 02_02_20 Fix get feed data and initialize WiFi hostname
 *   10/13/17 02_02_21 New release for production 
 *   10/18/17 02_02_22 Recompile with staged core to fix Krack vulnerability
 *   10/28/17 02_02_23 Fix several problems encountered using new arduino core 
 *   11/03/17 02_02_24 Improve error handling Emoncms, change API to use I/O channel names
 *   11/08/17 02_02_25 Rework data log into current and history logs 
 *   11/11/17 02_02_26 Fix bug in searchKey, improve searchKey, throttle back historyLog Service 
 *   12/05/17 02_02_27 Emoncms context from Emoncms, Rework config app 
 *   01/14/18 02_02_28 Add derived three-phase support 
 *   01/23/18 02_02_29 Improved phase correction plus misc. fixes  
 *   02/08/18 02_02_30 Bump version to release with tables.txt filename fixed
 *   03/01/18 02_03_01 AsyncHTTP 
 *   03/20/18 02_03_02 New influDB support, allow multiple upload services
 *   03/29/18 02_03_03 Units computation in calculator, heap management improvements
 *   03/30/18 02_03_04 Punchlist, fix memory loak in getConfig, build server payload at bulk maturation
 *   04/15/18 02_03_05 Rework influxDB support with variables and double buffering 
 *   05/16/18 02_03_06 Reimpliment messagelog, upgrade to 2.4.1 core and lwip 2
 *   06/03/18 02_03_07 Add web server authorization, temp time-wait fix
 *   06/20/18 02_03_08 Use lwip 2 "high bandwidth", misc fixes   
 *   07/11/18 02_03_09 Lastest asyngHTTPrequest, Issue#166, improve auth&trace  
 *   07/25/18 02_03_10 run getFeedData in handler, auth session changes, use staging core 
 *   07/31/18 02_03_11 Minor changes for general release 
 *   08/08/18 02_03_12 Finally fix memory leak?
 *   08/11/18 02_03_13 Back to 2.4.1 core, overhaul timeservice, fix zero voltage, add RSSI for WiFi 
 *   09/08/18 02_03_14 Add reverse and double to inputs, modify sampleCycle 
 *   09/12/18 02_03_15 Ready for MFG load, don't overwrite config, cleanup, deletelog=both, remove .ndx support, datalog fix, delete, dump
 *   09/21/18 02_03_16 Add access to spiffs, maintain burden in spiffs, improve performance of iotalog and histlog service 
 *   10/22/18 02_03_17 LLMNR responder, core 2.4.2, Age with new values, Add WDT to HTTP, Skip gaps posting Emon&influx, Check zeroDiv in script
 *   10/27/18 02_03_18 Improvements to influx, set WiFi hostname
 *   12/09/18 02_03_19 Add PVoutput support, local-time with DST, reduce use of WiFi Manager
 *   12/14/18 02_03_20 Upload live status only in PVoutput 
 *   01/18/19 02_03_21 EEprom, influx error recovery+, RTC battery warning, graph from IoTaWatt.com 
 *   03/05/19 02_04_00 Frequency and current based phase correction 
 *   07/23/19 02_04_01 Maintenance. Ongoing heap battle, tables, etc.
 *   08/13/19 02_04_02 Maintenance. PVoutput recovery, Save last config, safe mode on config problems
 *   10/04/19 02_05_00 Changes to new query and new Graph+ 
 *   10/12/19 02_05_01 Graph+ add download, add datalog WTD, add Query progress check, increase Query limit to 100K
 *   12/04/19 02_05_02 Graph+ improvements
 *   01/19/20 02_05_03 Update use versions.json, derived phase-phase support, Issue #252 query seconds, Fix Graph+ delete,
 *                     Fix and improve authorization, instantiate WiFiManager in local scope.                 
 * 
 *****************************************************************************************************/

      // Define instances of major classes to be used

WiFiClient WifiClient;
DNSServer dnsServer;    
IotaLog currLog(5,365);                     // current data log  (1 year) 
IotaLog histLog(60,3652);                   // history data log  (10 years)  
RTC_PCF8523 rtc;                            // Instance of RTC_PCF8523
Ticker ticker;
Ticker logWDT;
messageLog msglog;                          // Message log handler    

      // Define filename Strings of system files.          

char* deviceName;             
const char* IotaLogFile = "iotawatt/iotalog";
const char* historyLogFile = "iotawatt/histLog";
const char* IotaMsgLog = "iotawatt/iotamsgs.txt";
                       
uint8_t ADC_selectPin[2] = {pin_CS_ADC0,    // indexable reference for ADC select pins
                            pin_CS_ADC1};  


      // Trace context and work area

traceUnion traceEntry;

      /**************************************************************************************************
       * Core dispatching parameters - There's a lot going on, but the steady rhythm is sampling the
       * power channels, and that has to be done on their schedule - the AC frequency.  During sampling,
       * the time (in ms) of the last zero crossing is saved here.  Upon return to "Loop", the estimated
       * time just before the next crossing is computed.  That's when samplePower should be called again.
       * We try to run everything else during the half-wave intervals between power sampling.  
       **************************************************************************************************/
       
uint32_t lastCrossMs = 0;             // Timestamp at last zero crossing (ms) (set in samplePower)
uint32_t nextCrossMs = 0;             // Time just before next zero crossing (ms) (computed in Loop)

      // Various queues and lists of resources.

serviceBlock* serviceQueue;           // Head of active services list in order of dispatch time.       
IotaInputChannel* *inputChannel = nullptr; // -->s to incidences of input channels (maxInputs entries) 
uint8_t maxInputs = 0;                // channel limit based on configured hardware (set in Config)      
ScriptSet* outputs;                   // -> scriptSet for output channels

uint8_t  deviceMajorVersion = 4;      // Default to 4.8
uint8_t  deviceMinorVersion = 8;                 
float    VrefVolts = 2.5;             // Voltage reference shunt value used to calibrate

      // ****************************************************************************
      // statService maintains current averages of the channel values
      // so that current values can be displayed by web clients
      // statService runs at low frequency but is reved up by the web server 
      // handlers if the statistics are used.

float   frequency = 55;                  // Split the difference to start
float   configFrequency;                 // Frequency at last config (phase corrrection basis)    
float   samplesPerCycle = 550;           // Here as well
float   cycleSampleRate = 0;
int16_t cycleSamples = 0;
float    heapMs = 0;                      // heap size * milliseconds for weighted average heap
uint32_t heapMsPeriod = 0;                // total ms measured above.
IotaLogRecord statRecord;                 // Maintained by statService with real-time values

      // ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
ESP8266WebServer server(80);
bool    hasSD = false;
File    uploadFile;
SHA256* uploadSHA;
boolean serverAvailable = true;   // Set false when asynchronous handler active to avoid new requests
boolean wifiConnected = false;
uint8_t configSHA256[32];         // Hash of config file last time read or written

uint8_t*          adminH1 = nullptr;      // H1 digest md5("admin":"admin":password) 
uint8_t*          userH1 = nullptr;       // H1 digest md5("user":"user":password)
authSession*      authSessions = nullptr; // authSessions list head;
uint16_t          authTimeout = 600;      // Timeout interval of authSession in seconds;   
 

      // ************************** HTTP concurrent request semaphore *************************

int16_t  HTTPrequestFree = HTTPrequestMax;  // Request semaphore
uint32_t HTTPrequestStart[HTTPrequestMax];  // Reservation time(ms)
uint16_t HTTPrequestId[HTTPrequestMax];     // Module ID of reserver    
uint32_t HTTPlock = 0;                      // Time(ms) HTTP was locked (no new requests)   

      // ****************************** Timing and time data **********************************

int32_t  localTimeDiff = 0;                  // Hours from UTC 
tzRule*  timezoneRule = nullptr;             // Rule for DST 
uint32_t programStartTime = 0;               // Time program started (UnixTime)
uint32_t timeRefNTP = SEVENTY_YEAR_SECONDS;  // Last time from NTP server (NTPtime)
uint32_t timeRefMs = 0;                      // Internal MS clock corresponding to timeRefNTP
uint32_t timeSynchInterval = 3600;           // Interval (sec) to roll NTP forward and try to refresh
uint32_t EmonCMSInterval = 10;               // Interval (sec) to invoke EmonCMS
uint32_t influxDBInterval = 10;              // Interval (sec) to invoke inflexDB 
uint32_t statServiceInterval = 1;            // Interval (sec) to invoke statService
uint32_t updaterServiceInterval = 60*60;     // Interval (sec) to check for software updates 

bool     hasRTC = false;
bool     RTCrunning = false;
bool     powerFailRestart = false;
bool     validConfig = false;                // Config exists and Json parses first level
bool     RTClowBat = false;                  // RTC Battery is low
bool     sampling = false;                   // All channels have been sampled     

char     ledColor[12];                       // Pattern to display led, each char is 500ms color - R, G, Blank
uint8_t  ledCount;                           // Current index into cycle

      // ****************************** Firmware update ****************************
      
const char* updateURL = "iotawatt.com";
const char* updatePath = "/firmware/versions.json";
char*    updateClass = nullptr;                                   // NONE, MAJOR, MINOR, BETA, ALPHA, TEST    
const uint8_t publicKey[32] PROGMEM = {
                        0x7b, 0x36, 0x2a, 0xc7, 0x74, 0x72, 0xdc, 0x54,
                        0xcc, 0x2c, 0xea, 0x2e, 0x88, 0x9c, 0xe0, 0xea,
                        0x3f, 0x20, 0x5a, 0x78, 0x22, 0x0c, 0xbc, 0x78,
                        0x2b, 0xe6, 0x28, 0x5a, 0x21, 0x9c, 0xb7, 0xf3
                        }; 

const char hexcodes_P[] PROGMEM = "0123456789abcdef";
const char base64codes_P[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";  

      // ************************ ADC sample pairs ************************************

uint32_t  sumVsq;                                   // sampleCycle will compute these while collecting samples    
uint32_t  sumIsq;
int32_t   sumVI; 
int16_t   samples = 0;                              // Number of samples taken in last sampling
int16_t   Vsample [MAX_SAMPLES];                    // voltage/current pairs during sampling
int16_t   Isample [MAX_SAMPLES];
