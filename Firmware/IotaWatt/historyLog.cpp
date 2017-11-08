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
  enum states {initialize, logData};
  static states state = initialize;                                          
  IotaLogRecord* logRecord;
  
 
  switch(state){

    case initialize: {

      msgLog(F("historyLog: service started."));

        // If iotaLog not open or empty, check back later.

      if( ! currLog.isOpen() || (currLog.firstKey() - currLog.lastKey()) < histLog.interval()){
         return UNIXtime() + histLog.interval(); 
      }
        
        // Initialize the historyLog class
     
      if(int rtc = histLog.begin((char*)historyLogFile.c_str())){
        msgLog("historyLog: Log file open failed. ", String(rtc));
        dropDead();
      }
      
        // If it's not a new log, get the last entry.
     
      if(histLog.firstKey() != 0){    
        msgLog("historyLog: Last log entry:", histLog.lastKey());
      }

        // New history log, start with first even increment in IotaLog     

      else {
        logRecord = new IotaLogRecord;
        logRecord->UNIXtime = currLog.firstKey();
        if(logRecord->UNIXtime % histLog.interval()){
            logRecord->UNIXtime += histLog.interval() - (logRecord->UNIXtime % histLog.interval());
        }
        msgLog("historyLog: first entry:", logRecord->UNIXtime);
        currLog.readKey(logRecord);
        histLog.write(logRecord);
        delete logRecord;
      }

      //Serial.println("iotaLog");
      //currLog.dumpFile();
      Serial.println("histLog");
      histLog.dumpFile();

      _serviceBlock->priority = priorityLow;
      state = logData;
      break;
    }

    case logData: {

      if((histLog.lastKey() + histLog.interval()) > currLog.lastKey()){
        return UNIXtime() + 5;
      }

      logRecord = new IotaLogRecord;
      logRecord->UNIXtime = histLog.lastKey() + histLog.interval();
      if(logRecord->UNIXtime < currLog.firstKey()){
        logRecord->UNIXtime = histLog.lastKey();
        histLog.readKey(logRecord);
        logRecord->UNIXtime += histLog.interval();
      }
      else if(currLog.readKey(logRecord)){
        msgLog(F("historyLog: primary log file read failure. Service suspended."));
        delete logRecord;
        return 0;
      }

      if(logRecord->UNIXtime % histLog.interval()){
        Serial.print("log file record not multiple of history interval: ");
        Serial.println(logRecord->UNIXtime);
        delete logRecord;
        return 0;
      }
      histLog.write(logRecord);
      delete logRecord;
      break;
    }
  }
  return histLog.lastKey() + histLog.interval(); 
}