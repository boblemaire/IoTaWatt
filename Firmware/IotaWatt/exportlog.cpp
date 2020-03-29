#include "iotawatt.h"

int16_t exportLogInterval = 0;
int16_t exportLogRetention = 0;
int32_t exportLogRevision = -1;
Script *exportLogScript = nullptr;
bool exportLogStarted = false;

uint32_t exportLog(struct serviceBlock* _serviceBlock){
    enum states
    {
        initialize,
        startWait,
    } static state = initialize;
    

    trace(T_EXPORTLOG,0,state);
    if (! _serviceBlock){
        return 0;
    }
    

        // Handle state

    switch(state){

        case initialize: {

            trace(T_EXPORTLOG,10);
            if (_serviceBlock->priority != priorityLow){
                _serviceBlock->priority = priorityLow;
                return 1;
            } 

                // If iotaLog not open or empty, check back later.

            if( ! Current_log.isOpen() || (Current_log.lastKey() - Current_log.firstKey()) < Export_log->interval()){
                return UTCtime() + Export_log->interval(); 
            }
            log("ExportLog: Service started.");

                // Initialize the class.

            trace(T_EXPORTLOG,10);
            if(int rtc = Export_log->begin(IOTA_EXPORT_LOG_PATH)){
                log("ExportLog: Log file open failed: %d, service halted.", rtc);
                return 0;
            }

                // If not a new log, log last entry;

            if(Export_log->firstKey() != 0){

            }

        }
            

            


    }

}

bool configExportLog(const char* configObj){
    // DynamicJsonBuffer Json;
    // JsonObject& config = Json.parseObject(configObj);
    // if( ! config.success()){
    //     log("ExportLog: Json parse failed.");
    //     return false;
    // }

    // trace(T_EXPORTLOG,100);
    // int revision = config[F("revision")];
    // if(revision == exportLogRevision){
    //     return true;
    // }
    // delete exportLog;
    // exportLog = nullptr;

    // trace(T_EXPORTLOG,105);
    // int interval = config[F("interval")].as<int>();
    // int retention = config[F("retention")].as<int>();
    // if( ! (interval && retention)){
    //     return false;
    // }

    // trace(T_EXPORTLOG,110);
    // delete exportLog;
    // exportLog = nullptr;
    // JsonObject& JsonScript = config[F("script")]; 
    // if( ! JsonScript.success()){
    //     return false;
    // }
    // delete exportLogScript;
    // exportLogScript = new Script(JsonScript);
    // exportLogInterval = interval;
    // exportLogRetention = retention;
    // exportLog = new IotaLog(32, exportLogInterval, exportLogRetention);
    // if( ! exportLogStarted){
    //     // launch the service
    // }
    // return true;
}