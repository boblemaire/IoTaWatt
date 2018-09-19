# 1 "c:\\users\\bob\\appdata\\local\\temp\\tmph7nxa_"
#include <Arduino.h>
# 1 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
#include "IotaWatt.h"
# 181 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
WiFiClient WifiClient;

WiFiManager wifiManager;

DNSServer dnsServer;

IotaLog currLog(5,400);

IotaLog histLog(60,4000);

RTC_PCF8523 rtc;

Ticker ticker;

messageLog msglog;







char* deviceName;

const char* IotaLogFile = "iotawatt/iotalog";

const char* historyLogFile = "iotawatt/histLog";

const char* IotaMsgLog = "iotawatt/iotamsgs.txt";

const char* ntpServerName = "pool.ntp.org";





uint8_t ADC_selectPin[2] = {pin_CS_ADC0,

                            pin_CS_ADC1};
# 229 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
traceUnion traceEntry;
# 251 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
uint32_t lastCrossMs = 0;

uint32_t nextCrossMs = 0;







serviceBlock* serviceQueue;

IotaInputChannel* *inputChannel;

uint8_t maxInputs = 0;

ScriptSet* outputs;



uint16_t deviceVersion = 0;

float VrefVolts = 2.5;
# 289 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
float frequency = 55;

float samplesPerCycle = 550;

float cycleSampleRate = 0;

int16_t cycleSamples = 0;

float heapMs = 0;

uint32_t heapMsPeriod = 0;

IotaLogRecord statRecord;







#define DBG_OUTPUT_PORT Serial

ESP8266WebServer server(80);

bool hasSD = false;

File uploadFile;

SHA256* uploadSHA;

boolean serverAvailable = true;

boolean wifiConnected = false;

uint8_t configSHA256[32];



uint8_t* adminH1 = nullptr;

uint8_t* userH1 = nullptr;

authSession* authSessions = nullptr;

uint16_t authTimeout = 600;
# 343 "C:/Users/Bob/Dropbox/Github/IoTaWatt/Firmware/IotaWatt/IotaWatt.ino"
int16_t HTTPrequestMax = 2;

int16_t HTTPrequestFree = 2;







int localTimeDiff = 0;

uint32_t programStartTime = 0;

uint32_t timeRefNTP = SEVENTY_YEAR_SECONDS;

uint32_t timeRefMs = 0;

uint32_t timeSynchInterval = 3600;

uint32_t EmonCMSInterval = 10;

uint32_t influxDBInterval = 10;

uint32_t statServiceInterval = 1;

uint32_t updaterServiceInterval = 60*60;



bool hasRTC = false;

bool RTCrunning = false;



char ledColor[12];

uint8_t ledCount;







const char* updateURL = "iotawatt.com";

const char* updatePath = "/firmware/iotaupdt.php";

char* updateClass;

const uint8_t publicKey[32] PROGMEM = {

                        0x7b, 0x36, 0x2a, 0xc7, 0x74, 0x72, 0xdc, 0x54,

                        0xcc, 0x2c, 0xea, 0x2e, 0x88, 0x9c, 0xe0, 0xea,

                        0x3f, 0x20, 0x5a, 0x78, 0x22, 0x0c, 0xbc, 0x78,

                        0x2b, 0xe6, 0x28, 0x5a, 0x21, 0x9c, 0xb7, 0xf3

                        };



const char hexcodes_P[] PROGMEM = "0123456789abcdef";

const char base64codes_P[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";







int16_t samples = 0;

int16_t Vsample [MAX_SAMPLES];

int16_t Isample [MAX_SAMPLES];
