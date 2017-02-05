
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

#define IOTAWATT_VERSION "1.2"

#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <SD.h>
#include <WiFiUDP.h>
#include <IoTaMCP23S17.h>
#include <IotaLog.h>
#include <ArduinoJson.h>
#include <math.h>

WiFiClientSecure WifiClientSecure;
WiFiClient WifiClient;
const int HttpsPort = 443;
IoTa_MCP23S17 GPIO;                         // QandD method to run the GPIO chip
IotaLog iotaLog;

String deviceName = "IoTaWatt";             // can be specified in config.device.name
String IotaLogFile = "/IotaWatt/IotaLog.log";
String IotaMsgLog = "/IotaWatt/IotaMsgs.txt";


// Define the hardware SPI chip select pins

#define pin_CS_ADC0 0                       
#define pin_CS_ADC1 2
#define pin_CS_SDcard 15
#define pin_CS_GPIO 4
uint8_t ADC_selectPin[2] = {pin_CS_ADC0, pin_CS_ADC1};  // indexable reference

const int chipSelect = pin_CS_SDcard;       // for the benefit of SD.h

//*************************************************************************************
// Following are not the ESP8266 pins.
// They are MCP23S17 GPIO pins (0-15)
//*************************************************************************************
#define yellowLedPin 8                      // Connected to yellow LED
#define redLedPin 9                         // Connected to red LED

//*************************************************************************************

#define VrefChan 15                         // Voltage reference ADC channel
#define VrefVolts 2.5                       // Voltage reference value
float Aref = 0.1;                           // Measured Aref using above reference
                                   
uint8_t  ADC_bits;                          // JWYT - ADC output bits, set by senseADCbits() in Setup
uint16_t ADC_range;                         // computed integer range of ADC output

/******************************************************************************************************
 * Core dispatching parameters - There's a lot going on, but the steady rhythm is sampling the
 * power channels, and that has to be done on their schedule - the AC frequency.  During sampling,
 * the time (in ms) of the last zero crossing is saved here.  Upon return to "Loop", the estimated
 * time just before the next crossing is computed.  That's when samplePower will be called again.
 * We try to run everything else during the half-wave intervals between power sampling.  The next 
 * channel to be sampled is also kept here to complete the picture.  
 ******************************************************************************************************/
uint32_t lastCrossMs = 0;             // Timestamp at last zero crossing (ms)
uint32_t nextCrossMs = 0;             // Time just before next zero crossing (ms)
uint32_t nextChannel = 0;               // Next channel to sample
#define priorityLow 3
#define priorityMed 2
#define priorityHigh 1

struct serviceBlock {                  // Scheduler/Dispatcher list item (see comments in Loop)
  serviceBlock* next;                  // Next serviceBlock in list
  uint32_t callTime;                   // Time (in NTP seconds) to dispatch
  uint32_t priority;                   // Priority - lower is better
  uint32_t (*service)(serviceBlock*);  // the SERVICE
  serviceBlock(){next=NULL; callTime=0; priority=priorityMed; service=NULL;}
};

serviceBlock* serviceQueue = NULL;     // Head of ordered list of services

// Define number of physical channels, their configuration status, and active status

#define channels 15

enum channelTypes {channelTypeUndefined,
                   channelTypeVoltage,
                   channelTypePower};               

channelTypes channelType [channels];
String channelName[channels]; 
 
// The calibration factor for a voltage channel is the true voltage (in Volts) per ADC volt.
// The calibration for a current channel is the true current (in Amperes) per ADC volt.

float calibration [channels];
float phaseCorrection [channels];       // in degrees +lead, -lag  
uint8_t Vchannel [channels];            // Voltage reference channel (typically channel 0)

// ADC setup.  Assume perfect split DC bias.
// Initialized in setADCbits, adjusted within min/max range after each sample.     
                             
int16_t minOffset;
int16_t maxOffset;
int16_t offset [channels];              

//********************************************************************************************
// This structure is the basic data repository for a sensor channel.
// The values are the values developed at the instant of the last sampling.
// The accums are the corresponding time weighted accumulators of those values.
// timeThen is the time, in milliseconds, of the last update of the accumulators
//
// The accumulators are never cleared.  They continue to accumulate as long as
// the program runs.  Services that need to know the average values over some interval
// must record the value of an accumulator, wait the interval, and then divide the
// difference by the interval to get the average value over the period. Note that the
// accumulators are in hours (WattHrs, or VoltHrs) while the time stamps are in
// milliseconds - hence the useful constant:

const double MS_PER_HOUR = 3600000UL;               // useful constant

struct dataBucket {
  double value1;
  double value2;
  double value3;
  double accum1;
  double accum2;
  double accum3;
  uint32_t timeThen;

  dataBucket(){value1=0; value2=0; value3=0;
               accum1=0; accum2=0; accum3=0; 
               timeThen=0;}
};

double logHours = 0;
uint32_t logSerial = 0;
dataBucket buckets[channels];               // create a bunch of them

#define volts value1                        // so we can reference bucket.volts
#define hz value2                           // etc.
#define watts value1
#define amps value2
#define wattHrs accum1
#define pf value3
#define pfHrs accum3

// This function ages the contents of one bucket.

void ageBucket(struct dataBucket *bucket, uint32_t timeNow){
    double elapsedHrs = double((uint32_t)(timeNow - bucket->timeThen)) / MS_PER_HOUR;
    bucket->accum1 += bucket->value1 * elapsedHrs;
    bucket->accum2 += bucket->value2 * elapsedHrs;
    bucket->accum3 += bucket->value3 * elapsedHrs;
    bucket->timeThen = timeNow;
}

#define QUERY_VOLTAGE  1
#define QUERY_FREQUENCY  2
#define QUERY_POWER  3
#define QUERY_ENERGY 4
#define QUERY_PF  5

//***********************************************************************************************
// statService maintains current averages of the channel values
// so that current values can be displayed by web clients
// statService runs at low frequency but is reved up by the web server 
// handlers if the statistics are used.

float frequency = 60;
float  samplesPerCycle = 500;
float  cycleSampleRate = 0;
int16_t  cycleSamples = 0;

dataBucket statBuckets[channels];

// ****************************** SDWebServer stuff ****************************

#define DBG_OUTPUT_PORT Serial
const char* ssid = "flyaway";
const char* password = "68volkswagon";
char* host = "IoTaWatt";
ESP8266WebServer server(80);
static bool hasSD = false;
File uploadFile;
void handleNotFound();
serviceBlock* getFeedDataHandler;
boolean serverAvailable = true;                    // Set false when asynchronous handler active to avoid new requests

// ****************************** Timing and time data *************************
uint32_t localTimeDiff = -5;
uint32_t programStartTime = 0;                  // Time program started
uint32_t timeRefNTP = 0;                       // Last time from NTP server
uint32_t timeRefMs = 0;                       // Internal MS clock corresponding to timeRefNTP
uint32_t timeSynchInterval = 3600;              // Interval (sec) to roll NTP forward and try to refresh
uint32_t dataLogInterval = 5;                   // Interval (sec) to invoke dataLog
uint32_t eMonCMSInterval = 10;                  // Interval (sec) to invoke eMonCMS 
uint32_t statServiceInterval = 5;
#define SEVENTY_YEAR_SECONDS 2208988800UL

// *********************** eMonCMS configuration stuff *************************

String  eMonURL;
String apiKey;
int16_t node = 9;
boolean eMonSecure = false;
int16_t emonBulkEntries = 2;
const char* eMonSHA1 = "A2 FB AA 81 59 E2 B5 12 10 5D 38 22 23 A7 4E 74 B0 11 7D AA";

// ************************ ADC sample pairs ************************************
 
int16_t samples = 0;                              // Number of samples taken in last sampling
#define maxSamples 1500
int16_t Vsample [maxSamples];                     // voltage/current pairs during sampling
int16_t Isample [maxSamples];

// **************************** Calibration *************************************
                    
boolean calibrationMode = false;
int16_t calibrationVchan = 0;
int16_t calibrationRefChan = 0;
float calibrationCal = 0.0;
float calibrationPhase = 0.0; 

// ***************************RTC Trace **********************************************

     // Starting trace values by module. 

#define T_LOOP 10           // Loop
#define T_LOG 20            // dataLog
#define T_EMON 30           // eMonService
#define T_GFD 40            // GetFeedData
#define T_SAMP 50           // samplePower


