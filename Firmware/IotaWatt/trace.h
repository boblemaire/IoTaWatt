#pragma once
#include <Arduino.h>

/*******************************************************************************************
 * 
 * The trace facility uses the RTC memory, which endures through a restart, to save a
 * string of "bread-crumbs" that can be dropped with the trace macro.  They are added 
 * to a circular list and are displayed as part of the restart for any reason except
 * power fail (which would have cleared RTC memory).
 * 
 * To use the trace, define a module number below and insert trace macros
 * into the source code. 
 * 
  *******************************************************************************************/

void      trace(const uint8_t module, const uint8_t id, const uint8_t det=0); 
void      logTrace(void);

      // Trace context and work area

union traceUnion {
      uint32_t    traceWord;
      struct {
            uint8_t     seq;
            uint8_t     mod;
            uint8_t     id;
            uint8_t     det;
      };
};

extern traceUnion traceEntry;

     // RTC trace trace module IDs by module. (See trace routines in Loop tab)

#define T_LOOP 1           // Loop
#define T_LOG 2            // dataLog
#define T_Emoncms 3        // Emoncms uploader
#define T_GFD 4            // GetFeedData
#define T_UPDATE 5         // updater
#define T_SETUP 6          // Setup
#define T_influx 7         // influxDB
#define T_SAMP 8           // sampleCycle
#define T_POWER 9          // Sample Power
#define T_WEB 10           // (30)Web server handlers
#define T_CONFIG 11        //  Get Config
#define T_encryptEncode 12 //  base64encode and encryptData in EmonService
#define T_uploadGraph 13 
#define T_history 14
#define T_base64 15        // base 64 encode
#define T_stats 18         // Stat service 
#define T_datalog 19       // datalog service
#define T_timeSync 20      // timeSync service 
#define T_WiFi 21          // WiFi service
#define T_PVoutput 22      // PVoutput class 
#define T_samplePhase 23   // Sample phase (within samplePower) 
#define T_RTCWDT 24        // Dead man pedal service
#define T_CSVquery 25      // CSVquery
#define T_xurl 26          // xurl 
#define T_utility 27       // Miscelaneous utilities 
#define T_EXPORTLOG 28     // Export log
#define T_influx2 29       // influxDB2_uploader 
#define T_influx2Config 30 // influx2 configuration 
#define T_uploader 31      // Uploader base class
#define T_influx1 32       // influxDB_uploader
#define T_integrator 33    // Integrator class  
#define T_Script 34
#define T_Scriptset 35
#define T_uploaderRegistry 36

