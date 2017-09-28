
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


#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266httpUpdate.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include <Ticker.h>
#include <Crypto.h>
#include <AES.h>
#include <CBC.h>
#include <SHA256.h>
#include <Ed25519.h>

#include "IotaWatt.h"
#include "IotaLog.h"
#include "IotaInputChannel.h"
#include "IotaScript.h"
#include "webServer.h"

      // Declare instances of various classes above

WiFiClient WifiClient;
WiFiManager wifiManager;
DNSServer dnsServer;
IotaLog iotaLog;                            // instance of IotaLog class
RTC_PCF8523 rtc;                            // Instance of RTC_PCF8523
Ticker ticker;
CBC<AES128> cypher;
SHA256 sha256;
HTTPClient http;
MD5Builder md5;

const int HttpsPort = 443;
const double MS_PER_HOUR = 3600000UL;       // useful constant

      // Collection of filenames that IotaWatt uses.

String deviceName = "IotaWatt";             // can be specified in config.device.name
String IotaLogFile = "/IotaWatt/IotaLog";
String IotaMsgLog = "/IotaWatt/IotaMsgs.txt";
String EmonPostLogFile = "/iotawatt/Emonlog.log";
String influxPostLogFile = "/iotawatt/influxdb.log";
uint16_t deviceVersion = 0;

const int chipSelect = pin_CS_SDcard;       // for the benefit of SD.h

uint8_t ADC_selectPin[3] = {pin_CS_ADC0,    // indexable reference for ADC select pins
                            pin_CS_ADC1,
                            pin_CS_ADC2};

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

serviceBlock* serviceQueue = NULL;     // Head of ordered list of services

      // Define maximum number of input channels.
      // Create pointer for array of pointers to incidences of input channels
      // Initial values here are defaults for IotaWatt 2.1.
      // VrefVolts is the declared value of the voltage reference shunt,
      // Can be specified in config.device.aref
      // Voltage adjustments are the values for AC reference attenuation in IotaWatt 2.1.

IotaInputChannel* *inputChannel;              // -->s to incidences of input channels (maxInputs entries)
uint8_t maxInputs = 0;                        // channel limit based on configured hardware (set in Config)
float VrefVolts = 1.0;                        // Voltage reference shunt value used to calibrate
                                              // the ADCs. (can be specified in config.device.refvolts)

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

ScriptSet* outputs;

      // ****************************** SDWebServer stuff ****************************

char *host = "IotaWatt";
ESP8266WebServer server(80);
bool hasSD = false;
File uploadFile;

boolean serverAvailable = true;   // Set false when asynchronous handler active to avoid new requests
boolean wifiConnected = false;
uint8_t configSHA256[32];         // Hash of config file

      // ****************************** Timing and time data *************************
int      localTimeDiff = 0;
uint32_t programStartTime = 0;               // Time program started (UnixTime)
uint32_t timeRefNTP = SEVENTY_YEAR_SECONDS;  // Last time from NTP server (NTPtime)
uint32_t timeRefMs = 0;                      // Internal MS clock corresponding to timeRefNTP
uint32_t timeSynchInterval = 3600;           // Interval (sec) to roll NTP forward and try to refresh
uint32_t dataLogInterval = 5;                // Interval (sec) to invoke dataLog
uint32_t EmonCMSInterval = 10;               // Interval (sec) to invoke EmonCMS
uint32_t influxDBInterval = 10;              // Interval (sec) to invoke inflexDB 
uint32_t statServiceInterval = 1;            // Interval (sec) to invoke statService
uint32_t updaterServiceInterval = 60*60;     // Interval (sec) to check for software updates

boolean  hasRTC = false;
boolean  RTCrunning = false;

char    ledColor[12];                         // Pattern to display led, each char is 500ms color - R, G, Blank
uint8_t ledCount;                             // Current index into cycle

      // ****************************** Firmware update ****************************

const char* updateURL = "iotawatt.com";
const char* updateURI = "/firmware/iotaupdt.php";
String updateClass = "NONE";              // NONE, MAJOR, MINOR, BETA, ALPHA, TEST    
uint8_t publicKey[32] = {0x7b, 0x36, 0x2a, 0xc7, 0x74, 0x72, 0xdc, 0x54,
                         0xcc, 0x2c, 0xea, 0x2e, 0x88, 0x9c, 0xe0, 0xea,
                         0x3f, 0x20, 0x5a, 0x78, 0x22, 0x0c, 0xbc, 0x78,
                         0x2b, 0xe6, 0x28, 0x5a, 0x21, 0x9c, 0xb7, 0xf3}; 

      // *********************** EmonCMS configuration stuff *************************
      // Note: nee dto move out to a class and change for dynamic configuration
      // Start stop is a kludge for now.

bool EmonStarted = false;                    // set true when Service started
bool EmonStop = false;                       // set true to stop the Service
bool EmonInitialize = true;                  // Initialize or reinitialize EmonService
String  EmonURL;                             // These are set from the config file
String  EmonURI = "";
String apiKey;
uint8_t cryptoKey[16];
String node = "IotaWatt";
boolean EmonSecure = false;
String EmonUsername;
int16_t EmonBulkSend = 1;
enum EmonSendMode EmonSend = EmonSendPOSTsecure;
ScriptSet* emonOutputs;

      //********************** influxDB configuration stuff *****************************//
      // again, need to move this stuff to a class.

bool influxStarted = false;                    // set true when Service started
bool influxStop = false;                       // set true to stop the Service
bool influxInitialize = true;                  // Initialize or reinitialize 
String influxURL = "167.114.114.94";
uint16_t influxPort = 8086;
String influxDataBase = "test";
int16_t influxBulkSend = 1;
ScriptSet* influxOutputs;      

      // ************************ ADC sample pairs ************************************

int16_t samples = 0;                              // Number of samples taken in last sampling
int16_t Vsample [MAX_SAMPLES];                    // voltage/current pairs during sampling
int16_t Isample [MAX_SAMPLES];

      // I can't remove this unused function because the compiler goes berzerk.

void wtf(){}
