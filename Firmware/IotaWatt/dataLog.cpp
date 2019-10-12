 /**********************************************************************************************
 * dataLog is a SERVICE that posts the raw input data from the "buckets" to a file on the SDcard.
 * The IotaLog class handles the SD file work.
 * If there is no file, it will be created.
 * 
 * The log records contain 2 double precision value*hours accumulators for each channel.
 * Currently the two are Volt*Hrs / hz.Hrs for VT channels and
 * Watt*Hrs / VA*Hrs for CT channels.
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
  static uint32_t msThen = 0;

  switch(state){

    case initialize: {
      
            // If clock is not running, return

      if( ! RTCrunning || ! sampling) break;

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

      // If it's a new log,
      // Check to see if there is a history log for context.
      
      if(currLog.fileSize() == 0){
        log("dataLog: New current log created.");
        if(histLog.begin(historyLogFile) == 0 && histLog.fileSize() > 0){
          logRecord->UNIXtime = histLog.lastKey();
          histLog.readKey(logRecord);
          log("dataLog: Last history entry: %s", datef(UTC2Local(logRecord->UNIXtime)).c_str());
        }
      }
      
      // If it's not a new log, get the last entry.

      else {
        logRecord->UNIXtime = currLog.lastKey();
        currLog.readKey(logRecord);
        log("dataLog: Last log entry %s", datef(UTC2Local(currLog.lastKey())).c_str());
      }

      // If not at current log interval, return until then.  

      state = checkClock;
      uint32_t syncDelay = UTCtime() % currLog.interval();
      if(syncDelay) return syncDelay;

      // Fall through to checkClock

    }

    case checkClock: {
      
      // Initialize local accumulators
      
      msThen = millis();
      for(int i=0; i<maxInputs; i++){
        IotaInputChannel* _input = inputChannel[i];
        if(_input){
          inputChannel[i]->ageBuckets(msThen);
          accum1Then[i] = inputChannel[i]->dataBucket.accum1;
          accum2Then[i] = inputChannel[i]->dataBucket.accum2;
        }
      }

      // If it's been a long time since last entry, skip ahead.
      
      if((UTCtime() - logRecord->UNIXtime) > GapFill){
        logRecord->UNIXtime = UTCtime();
        logRecord->UNIXtime -= logRecord->UNIXtime % currLog.interval();
      }

      // Set state to log on subsequent calls.

      state = logData;
      _serviceBlock->priority = priorityHigh;
      break;
    }
 
    case logData: {

      // If this seems premature.... get outta here.

      if(UTCtime() < logRecord->UNIXtime) return logRecord->UNIXtime;

      // If log is up to date, update the entry with latest data.
          
      if(logRecord->UNIXtime >= UTCtime()){
        uint32_t msNow = millis();
        double elapsedHrs = double((uint32_t)(msNow - msThen)) / MS_PER_HOUR;
        for(int i=0; i<maxInputs; i++){
          IotaInputChannel* _input = inputChannel[i];
          if(_input){
            _input->ageBuckets(msNow);
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
        msThen = msNow;
        logRecord->logHours += elapsedHrs;
      }

      // Write the record
      
      currLog.write(logRecord);

      // Logging data is the primary purpose of IoTaWatt.
      // Set a WDT to make sure it continues.

      logWDT.detach();
      logWDT.attach(30, datalogWDT);

      break;
    }
  }

  // Advance the time and return.
  
  logRecord->UNIXtime += currLog.interval();
  return logRecord->UNIXtime;
}

/******************************************************************************
 * 
 *    The datalog WDT is set each time the datalog is updated. If it expires
 *    the datyalog is not being updated.  The program may be stuck in a 
 *    loop somewhere.  Log a message and restart.  The trace will provide
 *    diagnostic information. 
 * 
 * *****************************************************************************/

void datalogWDT(){
        log("dataLog: datalog WDT - restarting");
        ESP.restart();
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
  if( ! histLog.isOpen()){
    return currLog.readKey(callerRecord);
  }
  if(key % histLog.interval()){               // not multiple of histLog interval
    if(key >= currLog.firstKey() || key < histLog.firstKey()){   // in iotaLog
      return currLog.readKey(callerRecord);
    }
    return histLog.readKey(callerRecord);     // in histLog
  }
  else {                                      // multiple of histLog interval
    if(key >= histLog.firstKey() && key <= histLog.lastKey()){   // in histLog
      return histLog.readKey(callerRecord);
    }
    return currLog.readKey(callerRecord);     // in IotaLog
  }
  callerRecord->UNIXtime = histLog.lastKey(); // between the two logs (rare)
  histLog.readKey(callerRecord);
  callerRecord->UNIXtime = key;
  return 0;
}
