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
 * real or virtual, that are (or were) in the Current_log.
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

      if( ! Current_log.isOpen() || (Current_log.lastKey() - Current_log.firstKey()) < History_log.interval()){
        return UTCtime() + 5;
         //History_log.interval();
      }

      log("historyLog: service started."); 

        // Initialize the historyLog class
     
      trace(T_history,2);   
      if(int rtc = History_log.begin(IOTA_HISTORY_LOG_PATH)){
        log("historyLog: Log file open failed: %d, service halted.", rtc);
        return 0;
      }
      
        // If it's not a new log, get the last entry.
     
      if(History_log.firstKey() != 0){    
        log("historyLog: Last log entry %s", localDateString(History_log.lastKey()).c_str());
      }

        // New history log, start with first even increment in IotaLog     

      else {
        if( ! logRecord) logRecord = new IotaLogRecord;
        logRecord->UNIXtime = Current_log.firstKey();
        if(logRecord->UNIXtime % History_log.interval()){
            logRecord->UNIXtime += History_log.interval() - (logRecord->UNIXtime % History_log.interval());
        }
        log("historyLog: first entry %s", localDateString(logRecord->UNIXtime).c_str());
        Current_log.readKey(logRecord);
        History_log.write(logRecord);
        delete logRecord;
        logRecord = nullptr;
      }

      trace(T_history,3);
      if(History_log.lastKey() < Current_log.firstKey()){
        fillTarget = Current_log.firstKey();
        state = logFill;
      } else {
        state = logData;
      }
      return 1;
    } 

          // logFill replicates the last history file record until fillTarget.
          // Used primarily to fill large gaps as when the current log has been 
          // deleted after an interruption.

    case logFill: {
      if( ! logRecord) {
        logRecord = new IotaLogRecord;
        History_log.readSerial(logRecord, History_log.lastSerial());
      }
      logRecord->UNIXtime += History_log.interval();
      if(logRecord->UNIXtime < fillTarget){ 
        History_log.write(logRecord);
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
      while((History_log.lastKey() + History_log.interval()) <= Current_log.lastKey()){
        
        trace(T_history,5);
        if( ! logRecord){
          logRecord = new IotaLogRecord;
          logRecord->UNIXtime = History_log.lastKey(); 
        }
        logRecord->UNIXtime += History_log.interval();
        if(Current_log.readKey(logRecord)){
        log("historyLog: primary log file read failure. Service suspended.");
          delete logRecord;
          logRecord = nullptr;
          return 0;
        }
        trace(T_history,7); 
        if(logRecord->UNIXtime % History_log.interval()){
          Serial.print("log file record not multiple of history interval: ");
          Serial.println(logRecord->UNIXtime);
          delete logRecord;
          logRecord = nullptr;
          return 0;
        }
        trace(T_history,8); 
        History_log.write(logRecord);
        if((History_log.lastKey() + History_log.interval()) > Current_log.lastKey()){
          delete logRecord;
          logRecord = nullptr;
        }
        if(micros() > bingoTime){
          return 15;
        }
      }
      return History_log.lastKey() + History_log.interval() + 1;
    }
  }
  trace(T_history,9);
  return History_log.lastKey() + History_log.interval(); 
}