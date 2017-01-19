void loop()
{

  // -------- This calibration is a kludge. Requires an AC cord connected
  //          while IotaWatt in isolated running on battery and communicating
  //          via Wifi.  The utility value is that it measures the VT phase
  //          shift very accurately.  The standard calibration procedure in
  //          the configuration utility allows calibrating the voltage with
  //          just a meter or other method of reading the plug voltage.
    
  if(calibrationMode){
    uint32_t calStart = millis();
    while(calibrationMode){
      if(calibrateVT() && calibrateVT()) calibrationMode = false ;
      yield();
      server.handleClient();
      yield();
      server.handleClient();
      if((unsigned long)(millis() - calStart) > 20000){
        calibrationMode = false;
      }
    }
  }

/******************************************************************************
 * The main loop is very simple:
 *  Sample a power channel.
 *  Yield to the OS and Wifi Server.
 *  Run next dispatchable service if there is one
 *  Yield to the OS and Wifi Server.
 *  Go back, Jack, and do it again.
 ******************************************************************************/

  // ------- If AC zero crossing approaching, go sample a channel.

  if(millis() >= nextCrossMs){
    samplePower(nextChannel);
    nextCrossMs = lastCrossMs + 490 / int(frequency);
    nextChannel = ++nextChannel % channels;
  }

  yield();
  server.handleClient();
  yield();

// ---------- If the head of the service queue is dispatchable
//            invoke it's service function.

  if(serviceQueue != NULL && NTPtime() >= serviceQueue->callTime){
    serviceBlock* thisBlock = serviceQueue;
    serviceQueue = thisBlock->next;
    thisBlock->callTime = thisBlock->service(thisBlock);
    AddService(thisBlock); 
  }
     
  yield();
  server.handleClient();
  yield();
}

/*********************************************************************************************************
 * Scheduler/Dispatcher support functions.
 * 
 * The main loop steps through and samples the channels at the millisecond level.  It invokes samplePower()
 * which samples one or more waves and updates the corresponding data buckets.  After each sample, there 
 * are a few milliseconds before the next AC zero crossing, so we try to do everything else during that
 * downtime.
 * 
 * To accomplish that, other activities are organized as SERVICEs that are scheduled and are dispatched in 
 * Loop during the half-cycle downtime. The ESP8266 is already running a lower level operating system that 
 * is task oriented and dispatches this program along with other tasks - most notably the WiFi stack. Ticker
 * taps into that and provides one time or periodic interrupts that could be used to run services, but we
 * run the channel sampling with interrupts disabled, and we really don't need sub-second scheduling for 
 * our services anyway, so Ticker is not used.
 * 
 * This mechanism schedules at a resolution of one second, and dispatches during the optimal time period
 * between AC cycles.  To avoid context and synchonization issues, each service is coded as a state-machine.
 * They must be well behaved and should try to run for less than a few milliseconds. Although that isn't
 * always possible.
 * 
 * Services return the UNIXtime of the next requested dispatch.  If the requested time is in the past 
 * (includes 0), the service is requeued at the current time.  
 * 
 * The schedule itself is kept as an ordered list of control blocks in ascending order of time + priority.
 * Loop simply invokes the service currently at the beginning of the list.
 * 
 * The WiFi server is not one of these services.  It is invoked each time through the loop because it
 * polls for activity.
 ********************************************************************************************************/

void NewService(uint32_t (*serviceFunction)(struct serviceBlock*)){
    serviceBlock* newBlock = new serviceBlock;
    newBlock->service = serviceFunction;
    AddService (newBlock);
  }

void AddService(struct serviceBlock* newBlock){
  if(newBlock->callTime < NTPtime()) newBlock->callTime = NTPtime();
  if(serviceQueue == NULL ||
    (newBlock->callTime < serviceQueue->callTime) ||
    (newBlock->callTime == serviceQueue->callTime && newBlock->priority < serviceQueue->priority)){
    newBlock->next = serviceQueue;
    serviceQueue = newBlock;
  }
  else {
    serviceBlock* link = serviceQueue;
    while(link->next != NULL){
      if((newBlock->callTime < link->next->callTime) ||
        (newBlock->callTime == link->next->callTime && newBlock->priority < link->next->priority)){
        break;
      }        
      link = link->next;
    } 
    newBlock->next = link->next;
    link->next = newBlock;
  }
}

/******************************************************************************************************** 
 * All of the other SERVICEs that harvest values from the main "buckets" do so for their own selfish 
 * purposes, and have no global scope to share with others. This simple service maintains periodic
 * values for each of the buckets in a global set of buckets called statBuckets.  It also is where 
 * status statistics like sample rates are maintained.  It runs at relatively low frequency (10 sec)
 * but is reved up while there is an active status session querrying the server.
 *******************************************************************************************************/

uint32_t statService(struct serviceBlock* _serviceBlock) {
  static uint32_t timeThen = millis() - 1;            // Don't want divide by zero on first call
  uint32_t timeNow = millis();

  if(float(cycleSamples) > 0){
    cycleSampleRate = cycleSampleRate * .75 + .25 * float(cycleSamples * 1000) / float((uint32_t)(timeNow - timeThen));
  } else {
    cycleSampleRate = cycleSampleRate * .75;
  }
  
  double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;
  for(int i=0; i<channels; i++){
    ageBucket(&buckets[i],timeNow);
    statBuckets[i].value1 = (buckets[i].accum1 - statBuckets[i].accum1) / elapsedHrs;
    statBuckets[i].value2 = (buckets[i].accum2 - statBuckets[i].accum2) / elapsedHrs;
    statBuckets[i].value3 = (buckets[i].accum3 - statBuckets[i].accum3) / elapsedHrs;
    statBuckets[i].accum1 = buckets[i].accum1;
    statBuckets[i].accum2 = buckets[i].accum2;
    statBuckets[i].accum3 = buckets[i].accum3;
  }
    
  timeThen = timeNow;
  cycleSamples = 0;
  
  if(statServiceInterval < 10)statServiceInterval++;
  return ((uint32_t)NTPtime() + statServiceInterval);
}



