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
 uint32_t dataLog(struct serviceBlock* _serviceBlock){
  // trace 2x
  enum states {initialize, logData, noLog};
  static states state = initialize;
  
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static double accum1Then [channels];
  static double accum2Then [channels];
  static uint32_t timeThen = 0;
  uint32_t timeNow = millis();
  switch(state){

    case initialize: {
      
      for(int i=0; i<channels; i++){
        ageBucket(&buckets[i], timeNow);
        accum1Then[i] = buckets[i].accum1;
        accum2Then[i] = buckets[i].accum2;
      }
      timeThen = timeNow;
      msgLog("dataLog: service started.");
      
      if(iotaLog.begin((char*)IotaLogFile.c_str())){
        msgLog("dataLog: Log file open failed.");
        dropDead();
      }
      
      if(iotaLog.firstKey() != 0){
        logRecord->UNIXtime = iotaLog.lastKey();
        iotaLog.readKey(logRecord);
        msgLog("dataLog: Last log entry:", iotaLog.lastKey());
      }

      // Check last log record for NaN and reset to zero.
      // Should only happen if something went berzerk.
      // This is better than 86ing the whole historical log.
      // Maybe in the future start up a SERVICE to walk the error back and clean up. 

      if(logRecord->logHours != logRecord->logHours)logRecord->logHours = 0;
      for(int i=0; i<channels; i++) {
        if(logRecord->channel[i].accum1 != logRecord->channel[i].accum1) logRecord->channel[i].accum1 = 0;
        if(logRecord->channel[i].accum2 != logRecord->channel[i].accum2) logRecord->channel[i].accum2 = 0;
      }
      
      state = logData;
      break;
    }
 
    case logData: {
      double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;
      for(int i=0; i<channels; i++){
        ageBucket(&buckets[i], timeNow);
        logRecord->channel[i].accum1 += (buckets[i].accum1 - accum1Then[i]);
        if(logRecord->channel[i].accum1 != logRecord->channel[i].accum1) logRecord->channel[i].accum1 = 0;
        logRecord->channel[i].accum2 += (buckets[i].accum2 - accum2Then[i]);
        if(logRecord->channel[i].accum2 != logRecord->channel[i].accum2) logRecord->channel[i].accum2 = 0;
        double value1 = (buckets[i].accum1 - accum1Then[i]) / elapsedHrs;
        accum1Then[i] = buckets[i].accum1;
        accum2Then[i] = buckets[i].accum2;
      }  
      timeThen = timeNow;
      logRecord->logHours += elapsedHrs;
      if(logRecord->logHours != logRecord->logHours) logRecord->logHours = 0;
      logRecord->serial++;
      logRecord->UNIXtime = UnixTime();

      iotaLog.write(logRecord);
      break;
    }
  }
  
  return ((uint32_t)NTPtime() + dataLogInterval - (NTPtime() % dataLogInterval));
}

