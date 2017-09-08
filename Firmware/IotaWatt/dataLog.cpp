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
 
 #include <Arduino.h>

 #include "IotaWatt.h"
 #include "msgLog.h"
 
 uint32_t dataLog(struct serviceBlock* _serviceBlock){
  // trace 2x
  enum states {initialize, checkClock, logData};
  static states state = initialize;
  #define GapFill 600                                  // Fill in gaps of up to these seconds                             
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static double accum1Then [MAXINPUTS];
  static uint32_t timeThen = 0;
  uint32_t timeNow = millis();
  static uint32_t timeNext;
  switch(state){

    case initialize: {

      msgLog("dataLog: service started.");

      // Initialize the IotaLog class
      
      if(int rtc = iotaLog.begin((char*)IotaLogFile.c_str())){
        msgLog("dataLog: Log file open failed. ", String(rtc));
        dropDead();
      }

      // If it's not a new log, get the last entry.
      
      if(iotaLog.firstKey() != 0){
        logRecord->UNIXtime = iotaLog.lastKey();
        iotaLog.readKey(logRecord);
        msgLog("dataLog: Last log entry:", iotaLog.lastKey());
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
        }
      }
      timeThen = timeNow;

            // If clock is not running, return

      if( ! RTCrunning) break;

      // If it's been a long time since last entry, skip ahead.
      
      if((UNIXtime() - logRecord->UNIXtime) > GapFill){
        logRecord->UNIXtime = UNIXtime() - UNIXtime() % dataLogInterval;
      }

      // Initialize timeNext (will be incremented at exit below)
      // Set state to log on subsequent calls.

      timeNext = logRecord->UNIXtime;
      state = logData;
      break;
    }
 
    case logData: {

      // If this seems premature.... get outta here.

      if(UNIXtime() < timeNext) return timeNext;

      // If log is up to date, update the entry with latest data.
          
      if(timeNext == (UNIXtime() - UNIXtime() % dataLogInterval)){
        double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;
        for(int i=0; i<maxInputs; i++){
          IotaInputChannel* _input = inputChannel[i];
          if(_input){
            _input->ageBuckets(timeNow);
            logRecord->channel[i].accum1 += _input->dataBucket.accum1 - accum1Then[i];
            if(logRecord->channel[i].accum1 != logRecord->channel[i].accum1) logRecord->channel[i].accum1 = 0;
            accum1Then[i] = _input->dataBucket.accum1;
          }
          else {
            accum1Then[i] = 0;
          }
        }
        timeThen = timeNow;
        logRecord->logHours += elapsedHrs;
      }

      // set the time and record number and write the entry.
      
      logRecord->UNIXtime = timeNext;
      logRecord->serial++;
      iotaLog.write(logRecord);
      break;
    }
  }

  // Advance the time and return.
  
  timeNext += dataLogInterval;
  return timeNext;
}

