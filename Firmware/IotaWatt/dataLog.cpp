 /**********************************************************************************************
 * dataLog is a SERVICE that posts the raw input data from the "buckets" to a file on the SDcard.
 * The IotaLog class handles the SD file work.
 * If there is no file, it will be created.
 * 
 * The log records contain 2 double precision value*hours accumulators for each channel.
 * Currently the two are Volt*Hrs / hz.Hrs for VT channels and
 * Watt*Hrs / Irms*Hrs for CT channels.
 * 
 * Given any two log records, the average volts, hz, watts or Irms for the period between them
 * can be determined, in addition to the basic metric like WattHrs.  Power factor (average) can 
 * be determined by dividing Watts/(Irms * Vrms) (Vrms is found in the CT channel's associated
 * VT channel bucket.
 * 
 * Entries are only made in real time when the IotaWatt is running, so they are not periodic, 
 * but they are ordered.  It is relatively quick to find any record by key (UNIXtime) and a 
 * readKEY method is provided in the IotaLog class.
 * 
 * As with all of the SERVICES, it has a  single function call and is implimented as state machine.
 * Services should try not to execute for more than a few milliseconds at a time.
 **********************************************************************************************/
 #include "IotaWatt.h"
 #define GapFill 600           // Fill in gaps of less than this seconds 
       
 uint32_t dataLog(struct serviceBlock* _serviceBlock){
  enum states {initialize, checkClock, logData};
  static states state = initialize;                                                       
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static double accum1Then [MAXINPUTS];
  static double accum2Then [MAXINPUTS];
  static uint32_t timeThen = 0;
  uint32_t timeNow = millis();
  static uint32_t timeNext;
  switch(state){

    case initialize: {

      log("dataLog: service started.");

      // Initialize the IotaLog class
      
      if(int rtc = currLog.begin(IotaLogFile)){
        log("dataLog: Log file open failed. %d", rtc);
        dropDead();
      }

      // Initialize the IotaLogRecord accums in case no context.

      for(int i=0; i<MAXINPUTS; i++){
        logRecord->accum1[i] = 0.0;
        logRecord->accum2[i] = 0.0;
      }

      // If it's not a new log, get the last entry.
      
      if(currLog.fileSize() == 0){
        if(histLog.begin(historyLogFile) == 0 && histLog.fileSize() > 0){
          logRecord->UNIXtime = histLog.lastKey();
          histLog.readKey(logRecord);
          log("dataLog: Last history entry: %d", logRecord->UNIXtime);
        }
      }
      else {
        logRecord->UNIXtime = currLog.lastKey();
        currLog.readKey(logRecord);
        log("dataLog: Last log entry %s", dateString(currLog.lastKey()).c_str());
      }

      state = checkClock;

      // Fall through to checkClock

    }

    case checkClock: {
      
      // Initialize local accumulators
      
      for(int i=0; i<maxInputs; i++){
        IotaInputChannel* _input = inputChannel[i];
        if(_input){
          inputChannel[i]->ageBuckets(timeNow);
          accum1Then[i] = inputChannel[i]->dataBucket.accum1;
          accum2Then[i] = inputChannel[i]->dataBucket.accum2;
        }
      }
      timeThen = timeNow;

            // If clock is not running, return

      if( ! RTCrunning) break;

      // If it's been a long time since last entry, skip ahead.
      
      if((UNIXtime() - logRecord->UNIXtime) > GapFill){
        logRecord->UNIXtime = UNIXtime() - UNIXtime() % currLog.interval();
      }

      // Initialize timeNext (will be incremented at exit below)
      // Set state to log on subsequent calls.

      timeNext = logRecord->UNIXtime;
      state = logData;
      _serviceBlock->priority = priorityHigh;
      break;
    }
 
    case logData: {

      // If this seems premature.... get outta here.

      if(UNIXtime() < timeNext) return timeNext;

      // If log is up to date, update the entry with latest data.
          
      if(timeNext >= (UNIXtime() - UNIXtime() % currLog.interval())){
        double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;
        for(int i=0; i<maxInputs; i++){
          IotaInputChannel* _input = inputChannel[i];
          if(_input){
            _input->ageBuckets(timeNow);
            logRecord->accum1[i] += _input->dataBucket.accum1 - accum1Then[i];
            if(logRecord->accum1[i] != logRecord->accum1[i]) logRecord->accum1[i] = 0;
            accum1Then[i] = _input->dataBucket.accum1;
            logRecord->accum2[i] += _input->dataBucket.accum2 - accum2Then[i];
            if(logRecord->accum2[i] != logRecord->accum2[i]) logRecord->accum2[i] = 0;
            accum2Then[i] = _input->dataBucket.accum2;
          }
          else {
            accum1Then[i] = 0;
            accum2Then[i] = 0;
          }
        }
        timeThen = timeNow;
        logRecord->logHours += elapsedHrs;
      }

      // set the time and record number and write the entry.
      
      logRecord->UNIXtime = timeNext;
      logRecord->serial++;
      currLog.write(logRecord);
      break;
    }
  }

  // Advance the time and return.
  
  timeNext += currLog.interval();
  return timeNext;
}

/******************************************************************************
 * logReadKey(iotaLogRecord) - read a keyed record from the combined log
 * 
 * This function brokers keyed log read requests, servicing them from the
 * appropriate log:
 * 
 * currLog:
 * relatively recent data spanning the past 12-15 months.
 * small interval (5 seconds).
 * potentially slower access because it can have holes neccessitating searching.
 * 
 * histLog:
 * contains all of the data since the beginning of time.
 * large interval (60 seconds).
 * Look ma - no holes!  direct access w/o searching.
 * 
 * This function will decide the most appropriate log to retrieve the requested 
 * record from based on these principles.
 * 
 * If the key is a multiple of the history log interval, and is contained in
 * the history log, use the history log.
 * 
 * If the key is not a multiple of the history log interval and contained in
 * the currLog, use the currLog.
 * 
 * If the key is not a multiple of the history log, but not contained in the 
 * currLog, use the history log.
 * 
 * if the key is between the end of the history log and the start of the currLog,
 * return the last record in the history log with requested key.
 * 
 * ***************************************************************************/

uint32_t logReadKey(IotaLogRecord* callerRecord) {
  uint32_t key = callerRecord->UNIXtime;
  if(key % histLog.interval()){               // not multiple of histLog interval
    if(key >= currLog.firstKey()){            // in iotaLog
      return currLog.readKey(callerRecord);
    }
    if(key <= histLog.lastKey()){             // in histLog
      return histLog.readKey(callerRecord);
    }
  }
  else {                                      // multiple of histLog interval
    if(key <= histLog.lastKey()){             // in histLog
      return histLog.readKey(callerRecord);
    }
    if(key >= currLog.firstKey()){            // in IotaLog
      return currLog.readKey(callerRecord);
    }
  }
  callerRecord->UNIXtime = histLog.lastKey(); // between the two logs (rare)
  histLog.readKey(callerRecord);
  callerRecord->UNIXtime = key;
  return 0;
}
