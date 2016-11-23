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

#define IoTaWatt_version 1.0

#include <SPI.h>
#include <SdFat.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiUDP.h>
#include <IoTaMCP23S17.h>
#include <ArduinoJson.h>

SdFat SD;
IoTa_MCP23S17 GPIO;

// Define the hardware SPI chip select pins

#define pin_CS_ADC0 0
#define pin_CS_ADC1 2
#define pin_CS_SDcard 15
#define pin_CS_GPIO 4
uint8_t ADC_selectPin[2] = {pin_CS_ADC0, pin_CS_ADC1};

// set up variables using the SD utility library functions:
const int chipSelect = pin_CS_SDcard;

// These are not the ESP8266 pins.
// They are MCP23S17 GPIO pins (0-15)

#define yellowLedPin 8                      // Connected to yellow LED
#define redLedPin 9                         // Connected to red LED

#define VrefChan 15                         // Voltage reference ADC channel
#define VrefVolts 2.5                       // Voltage reference value
float Aref = 0.1;                           // Measured Aref using above reference
                                   
uint8_t  ADC_bits;                          // JWYT - ADC output bits, set during Config.
uint16_t ADC_range;                         // computed integer range of ADC output, also set during Config.

int16_t post_interval_sec = 10;             // Posting interval in seconds, overide in Config file  
uint32_t post_interval_ms = post_interval_sec * 1000; // In milliseconds

//**************************** timing parameters *********************************************

uint32_t millis_now = 0;
uint32_t last_sample_time = 0;            // Clock at last sampling
uint32_t last_post_time = 0;              // Clock at start of last posting
uint32_t post_time = 0;                   // Clock at start of current Posting
uint32_t post_interval = 0;               // Posting interval in milliseconds
uint32_t sample_interval = 0;             // Sampling interval in milliseconds

// Define number of physical channels, their configuration status, and active status

const int channels = 14;                        
boolean channelActive [16];

uint8_t channelType [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#define channelTypeUndefined 0
#define channelTypeVoltage   1
#define channelTypePower     2
#define channelTypeTemp      3
#define channelTypeMa        4
#define channelTypeSwitch    5
 
// The calibration factor for a voltage channel is the true voltage (in Volts) per ADC volt.
// The calibration for a current channel is the true current (in Amperes) per ADC volt.

float calibration [16]   =  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
float phaseCorrection [16] = {4.0,0,1.5,3.25,0,0,0,1.5,0,0,0,0,0,0,0,0};
uint8_t Vchannel[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};     // Voltage reference channel (3 phase?)

// ADC setup.  Assume perfect split DC bias. Will adjust within min/max range after each sample.     
                             
int16_t minOffset;
int16_t maxOffset;
int16_t offset [16]; 

// Power measurement and reporting accumulators

float activePower[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};       // True power measured in last sample
float watt_ms[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};         // accumulated watt-ms in current post interval
float averageWatts[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};    // average watts in last post interval
float apparentPower[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};   // Apparent power measured in last sample
float VA_ms[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};           // accumulated watt-ms in current post interval
float averageVA[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};       // average VA in last post interval 
                                                               // power factor is True power/ Apparent power                                                              // or watts / VA 
float    Vrms = 0.0;
uint16_t frequency = 60;
int16_t  samplesPerCycle = 0;                                 

// WiFi Stuff

WiFiClient cloudServer;
boolean internet_connection = 0;
boolean reply_pending = false;

// Network Time Protocol (NTP) stuff

unsigned int localPort = 2390;
IPAddress timeServerIP;
WiFiUDP udp;
const char* ntpServerName = "time.nist.gov";
int local_time_adjustment = -4;
const int NTP_PACKET_SIZE = 4;
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// eMonCMS configuration stuff (should impliment protocol in a class)

String server;
String cloudURL;
String apikey;
int16_t node = 9;
 
int16_t samples = 0;                              // Number of samples in following arrays
#define maxSamples 1500
int16_t Vsample [maxSamples];                     // voltage/current pairs during sampling
int16_t Isample [maxSamples];
                    
boolean oneShot = true;




