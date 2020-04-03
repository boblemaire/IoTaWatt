#include "iotawatt.h"

#define EXPORT_LOG_INTERVAL 60          // DO NOT CHANGE unless creating new log
#define EXPORT_LOG_RETENTION 10*365     // Change only effective if value greater than filesize at restart    
Script *exportLogScript = nullptr;      // import(+)/export(-) Script
uint32_t exportLogLookback = 0;         // Lookback days on creation

uint32_t exportLog(struct serviceBlock* _serviceBlock){

    struct export_record {
        uint32_t UNIXtime;          // Time period represented by this record
        int32_t serial;             // record number in file
        double logHours;            // Total hours of monitoring logged to date in this log
        double importWh;            // Cummulative imported Wh    
        double exportWh;            // Cummulative exported Wh
    };

    static IotaLogRecord *exportRecord = nullptr;
    static IotaLogRecord *oldRec = nullptr;
    static IotaLogRecord *newRec = nullptr;
    static uint32_t timeNext = 0;

    enum states
    {
        initialize = 0,
        waitnext = 1,
        integrate = 2
    };

    static states state = initialize;

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
            trace(T_EXPORTLOG,10);
            log("ExportLog: Service started.");

                // Initialize the class.

            trace(T_EXPORTLOG,10);
            if(int rtc = Export_log->begin(IOTA_EXPORT_LOG_PATH)){
                log("ExportLog: Log file open failed: %d, service halted.", rtc);
                return 0;
            } 

                // Allocate export record and an IoTaLog Record.
                // Need to type to IotaLogRecord for IotaLog class.

            trace(T_EXPORTLOG,10);
            if(! exportRecord){
                exportRecord = (IotaLogRecord *) new export_record;
            }

            // If not a new log, get last entry;

            if(Export_log->firstKey() != 0){
                trace(T_EXPORTLOG,11);
                exportRecord->UNIXtime = Export_log->lastKey();
                Export_log->readKey(exportRecord);
                log("Exportlog: last entry %s", localDateString(Export_log->lastKey()).c_str());
            }
            
                // New Export_log, determine where to start.

            else {
                trace(T_EXPORTLOG,12);
                uint32_t startTime = UTCtime() - (exportLogLookback * SECONDS_PER_DAY);

                tm *_tm = localtime((time_t*)&startTime);
                _tm->tm_hour = 0;
                _tm->tm_min = 0;
                _tm->tm_sec = 0;
                startTime = UTCtime((uint32_t)mktime(_tm));

                if(startTime < Current_log.firstKey()){
                    startTime = Current_log.firstKey();
                }
                startTime = startTime + Export_log->interval() - 5;
                startTime = startTime - (startTime % Export_log->interval());
                exportRecord->UNIXtime = startTime;
                exportRecord->logHours = 0.0;
                exportRecord->Export = 0.0;
                exportRecord->Import = 0.0;
                log("Exportlog: begin after %s", localDateString(exportRecord->UNIXtime).c_str());
            }

            exportRecord->UNIXtime + Export_log->interval();
            state = waitnext;
            return exportRecord->UNIXtime;
        }
            
        case waitnext: {
            
            if(exportRecord->UNIXtime > Current_log.lastKey()){
                return exportRecord->UNIXtime;
            }

            oldRec = new IotaLogRecord;
            oldRec->UNIXtime = exportRecord->UNIXtime - Export_log->interval();
            Current_log.readKey(oldRec);
            state = integrate;
            return 1;
        }

        case integrate: {

            if(oldRec->UNIXtime >= exportRecord->UNIXtime){
                //Export_log->write(exportRecord)
                if((exportRecord->UNIXtime % 120) == 0)
                Serial.printf("%s, %.3f, %.3f\n", localDateString(exportRecord->UNIXtime).c_str(),
                              exportRecord->Export, exportRecord->Import);
                exportRecord->UNIXtime += Export_log->interval();
                if(exportRecord->UNIXtime > Current_log.lastKey()){
                    delete oldRec;
                    state = waitnext;
                }
                return 1;
            }

            newRec = new IotaLogRecord;
            if (Current_log.readSerial(newRec, oldRec->serial + 1)){
                delete newRec;
                return UTCtime() + 1;
            }

            if(newRec->UNIXtime <= exportRecord->UNIXtime){
                double elapsedHours = newRec->logHours - oldRec->logHours;
                if(elapsedHours){
                    double result = exportLogScript->run(oldRec, newRec, elapsedHours, "Wh");
                    exportRecord->logHours += elapsedHours;
                    if(result >= 0.0){
                        exportRecord->Import += result;
                    } else {
                        exportRecord->Export += result;
                    }
                }
            }

            delete oldRec;
            oldRec = newRec;
            return 1;
        }
    }

    log("Exportlog: invalid state %d, terminating", state);
    return 0;
}

bool exportLogConfig(const char* configObj){
    trace(T_EXPORTLOG,110); 
    DynamicJsonBuffer Json;
    JsonObject& config = Json.parseObject(configObj);
    if( ! config.success()){
        log("ExportLog: Json parse failed.");
        return false;
    }
    trace(T_EXPORTLOG,110);
    exportLogLookback = config[F("lookback")].as<int>();
    trace(T_EXPORTLOG,110); 
    JsonObject& JsonScript = config[F("script")]; 
    if( ! JsonScript.success()){
        log("ExportLog: Invalid Script.");
        return false;
    }
    delete exportLogScript;
    exportLogScript = new Script(JsonScript);
    if( ! Export_log){
        Export_log = new IotaLog(32, EXPORT_LOG_INTERVAL, EXPORT_LOG_RETENTION);
        NewService(exportLog, T_EXPORTLOG);
    }
    return true;
}