void loop()
{
    
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

  if(millis() >= nextCrossMs){
    samplePower(nextChannel);
    nextCrossMs = lastCrossMs + 490 / int(frequency);
    nextChannel = ++nextChannel % channels;
  }
  
  if(reply_pending) checkdata();
  yield();
  server.handleClient();
  yield();

  if(serviceQueue != NULL){
    if(NTPtime() >= serviceQueue->callTime){
      serviceBlock* thisBlock = serviceQueue;
      serviceQueue = thisBlock->next;
      thisBlock->service(thisBlock); 
    }
  }
     
  yield();
  server.handleClient();
  yield();
}



void ageBucket(struct dataBucket *bucket, uint16_t timeNow){
  double elapsedHrs = double((IotaTime)(timeNow - bucket->timeThen)) / MS_PER_HOUR;
  bucket->accum1 += bucket->value1 * elapsedHrs;
  bucket->accum2 += bucket->value2 * elapsedHrs;
  bucket->accum3 += bucket->value3 * elapsedHrs;
  bucket->timeThen = timeNow;
}

/*********************************************************************************************************
 * Scheduler/Dispatcher support functions.
 * 
 * The main loop steps through and samples the channels at the millisecond level.  It invokes samplePower()
 * which samples one or more waves and updates the corresponding data buckets.  After each sample, there 
 * are a few milliseconds before the next AC zero crossing, so we try to do everything else during that
 * downtime.
 * 
 * To accomplish that, other activities are organized as "services" that are scheduled and are dispatched in 
 * Loop during the half-cycle downtime. The ESP8266 is already running a lower level operating system that 
 * is task oriented and dispatches this program along with other tasks - most notably the WiFi stack. Ticker
 * taps into that and provides one time or periodic interrupts that could be used to run services, but we
 * run the channel sampling with interrupts disabled, and we really don't need sub-second scheduling for 
 * our services anyway, so Ticker is not used.
 * 
 * This mechanism schedules at a resolution of one second, and dispatches during the optimal time period
 * between AC cycles.  To avoid context and synchonization issues, each service is coded as a state-machine,
 * and is responsible for rescheduling itself.  They must be well behaved and should try to run for less
 * than a few milliseconds.
 * 
 * The schedule itself is kept as an ordered list of control blocks in ascending order of time + priority.
 * Loop simply invokes the service currently at the beginning of the list.
 * 
 * The WiFi server is not one of these services.  It is invoked each time through the loop because it
 * polls for activity.
 ********************************************************************************************************/

void NewService(int (*serviceFunction)(struct serviceBlock*)){
    serviceBlock* newBlock = new serviceBlock;
    newBlock->service = serviceFunction;
    AddService (newBlock);
  }

void AddService(struct serviceBlock* newBlock){
  if(newBlock->callTime < NTPtime()) newBlock->callTime = NTPtime();
  if(serviceQueue == NULL || newBlock->callTime < serviceQueue->callTime){
    newBlock->next = serviceQueue;
    serviceQueue = newBlock;
  }
  else {
    serviceBlock* link = serviceQueue;
    while(link->next != NULL){
      if(newBlock->callTime < link->next->callTime) break;
      if(newBlock->callTime == link->next->callTime &&
         newBlock->priority < link->next->priority) break;
      link = link->next;
    } 
    newBlock->next = link->next;
    link->next = newBlock;
  }
}


int statService(struct serviceBlock* _serviceBlock) {
  static IotaTime timeThen = millis() - 1;            // Don't want divide by zero on first call
  IotaTime timeNow = millis();

  cycleSampleRate = cycleSampleRate * .75 + .25 * float(cycleSamples * 1000) / float((IotaTime)(timeNow - timeThen));
  double elapsedHrs = double((IotaTime)(timeNow - timeThen)) / MS_PER_HOUR;
  for(int i=0; i<channels; i++){
    ageBucket(&buckets[i],timeNow);
    statBuckets[i].value1 = (buckets[i].accum1 - statBuckets[i].accum1) / elapsedHrs;
    statBuckets[i].value2 = (buckets[i].accum2 - statBuckets[i].accum2) / elapsedHrs;
    statBuckets[i].value3 = (buckets[i].accum3 - statBuckets[i].accum3) / elapsedHrs;
    statBuckets[i].accum1 = buckets[i].accum1;
    statBuckets[i].accum2 = buckets[i].accum2;
    statBuckets[i].accum3 = buckets[i].accum3;
  }
    
  timeThen = millis();
  cycleSamples = 0;
  
  if(statServiceInterval < 10)statServiceInterval++;
  _serviceBlock->callTime = NTPtime() + statServiceInterval;
  AddService(_serviceBlock);
}

