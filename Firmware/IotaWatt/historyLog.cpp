/**********************************************************************************************
 * historyLog is a Service that maintains the 1 minute interval historical data log.
 * Primarily due to constraints in the fat32 file system, the 5 second interval log has a
 * capacity of about 485 days (1.3 years).  So it will wrap around at some point and
 * older data overwritten.
 * 
 * The history log has 60 second resolution and therefore has a capacity of nearly
 * 16 years.  Moreover, because it is built by this process using the iotalog, it has
 * no "holes", so all keyed reads are direct without searching.
 * 
 * Queries from graph, and other well behaved users, will request data beginning at a 
 * minute boundary, with an interval that is a multiple of one minute.  So most requests
 * should be serviceable from this history log.
 * 
 * An added benefit is that it serves as low-resolution backup for the 5 second log, so
 * even if the 5 minute log is lost, most queries will still be satisfied.
 * 
 * The records in this log are simply an identical subset of the one-minute entries,
 * real or virtual, that are (or were) in the currLog.
 * 
 **********************************************************************************************/
#include "IotaWatt.h"
#define GapFill 600           // Fill in gaps of less than this seconds 
      
uint32_t historyLog(struct serviceBlock* _serviceBlock){
  enum states {initialize, logFill, logData};
  static states state = initialize;
  static uint32_t lastExitTime = 0;
  static uint32_t fillTarget = 0;                                         
  static IotaLogRecord* logRecord = nullptr;
  trace(T_history,0);  
 
  switch(state){

    case initialize: {
      trace(T_history,1); 

        // If iotaLog not open or empty, check back later.

      if( ! currLog.isOpen() || (currLog.lastKey() - currLog.firstKey()) < histLog.interval()){
         return UTCTime() + histLog.interval(); 
      }

      log("historyLog: service started."); 

        // Initialize the historyLog class
     
      trace(T_history,2);   
      if(int rtc = histLog.begin(historyLogFile)){
        log("historyLog: Log file open failed: %d, service halted.", rtc);
        return 0;
      }
      
        // If it's not a new log, get the last entry.
     
      if(histLog.firstKey() != 0){    
        log("historyLog: Last log entry %s", localDateString(histLog.lastKey()).c_str());
      }

        // New history log, start with first even increment in IotaLog     

      else {
        if( ! logRecord) logRecord = new IotaLogRecord;
        logRecord->UNIXtime = currLog.firstKey();
        if(logRecord->UNIXtime % histLog.interval()){
            logRecord->UNIXtime += histLog.interval() - (logRecord->UNIXtime % histLog.interval());
        }
        log("historyLog: first entry %s", localDateString(logRecord->UNIXtime).c_str());
        currLog.readKey(logRecord);
        histLog.write(logRecord);
        delete logRecord;
        logRecord = nullptr;
      }

      trace(T_history,3);
      if(histLog.lastKey() < currLog.firstKey()){
        fillTarget = currLog.firstKey();
        state = logFill;
      } else {
        state = logData;
      }
      break;
    } 

          // logFill replicates the last history file record until fillTarget.
          // Used primarily to fill large gaps as when the current log has been 
          // deleted after an interruption.

    case logFill: {
      if( ! logRecord) {
        logRecord = new IotaLogRecord;
        histLog.readSerial(logRecord, histLog.lastSerial());
      }
      logRecord->UNIXtime += histLog.interval();
      if(logRecord->UNIXtime < fillTarget){ 
        histLog.write(logRecord);
      }
      else {
        delete logRecord;
        logRecord = nullptr;
        state = logData;
      }
      return 1;
    }

    case logData: {
      trace(T_history,4);   
      if((histLog.lastKey() + histLog.interval()) > currLog.lastKey()){
        return UTCTime() + 5;
      }
      trace(T_history,5);
      if( ! logRecord){
        logRecord = new IotaLogRecord;
        logRecord->UNIXtime = histLog.lastKey(); 
      }
      logRecord->UNIXtime += histLog.interval();
      if(currLog.readKey(logRecord)){
      log("historyLog: primary log file read failure. Service suspended.");
        delete logRecord;
        logRecord = nullptr;
        return 0;
      }
      trace(T_history,7); 
      if(logRecord->UNIXtime % histLog.interval()){
        Serial.print("log file record not multiple of history interval: ");
        Serial.println(logRecord->UNIXtime);
        delete logRecord;
        logRecord = nullptr;
        return 0;
      }
      trace(T_history,8); 
      histLog.write(logRecord);
      if((histLog.lastKey() + histLog.interval()) > currLog.lastKey()){
        delete logRecord;
        logRecord = nullptr;
      }
      break;
    }
  }
  trace(T_history,9);
  return histLog.lastKey() + histLog.interval(); 
}