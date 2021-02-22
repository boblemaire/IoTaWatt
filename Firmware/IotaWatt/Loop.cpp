#include "IotaWatt.h"
 
void loop()
{
/******************************************************************************
 * The main loop is very simple:
 *  Sample a power channel.
 *  Yield to the OS and Wifi Server.
 *  Run next dispatchable service if there is one
 *  Yield to the OS and Wifi Server.
 *  Go back, Jack, and do it again.
 ******************************************************************************/

  setLedState();

  // ------- If AC zero crossing approaching, go sample a channel.
  static int lastChannel = 0;
  uint32_t microsNow = micros();
  if(microsNow <= lastCrossMs){
    bingoTime = 0;
  }
  if(maxInputs && microsNow > bingoTime){
    trace(T_LOOP,1,lastChannel);
    int nextChannel = (lastChannel + 1) % maxInputs;
    while( (! inputChannel[nextChannel]->isActive()) && nextChannel != lastChannel){
      nextChannel = ++nextChannel % maxInputs;
    }
    ESP.wdtFeed();
    trace(T_LOOP,2,nextChannel);
    samplePower(nextChannel, 0);
    trace(T_LOOP,2);
    nextCrossMs = lastCrossMs + 490 / int(frequency);
    if(int(frequency) > 25){
      bingoTime = lastCrossUs + 500000 / int(frequency) - 1500;
    }
    else {
      bingoTime = lastCrossUs + 3500;
    }
    //bingoTime = lastCrossUs + ((lastCrossUs - firstCrossUs) / 2) - 1500;
    if(nextChannel <= lastChannel) sampling = true;
    lastChannel = nextChannel;
  }

  // --------- Give web server a shout out.
  //           serverAvailable will be false if there is a request being serviced by
  //           an Iota SERVICE. (GetFeedData)

  yield();
  ESP.wdtFeed();
  trace(T_LOOP,3);
  if(serverAvailable){
    server.handleClient();
    trace(T_LOOP,3);
    yield();
  }

  // Config is updated asynchronously in web-server ISRs.
  // Upon closing an updated config file, getNewConfig is set.
  // Process that config here.
  
  if(getNewConfig){
    trace(T_LOOP,4);
    getNewConfig = false;
    if(updateConfig("config+1.txt")){
      trace(T_LOOP,4);
      copyFile("/esp_spiffs/config.txt", "config.txt");
    }
    else {

    }
  }

// ---------- If the head of the service queue is dispatchable
//            call the SERVICE.

  microsNow = micros();
  if(microsNow < bingoTime && serviceQueue != NULL && millis() >= serviceQueue->scheduleTime){
    trace(T_LOOP,6,1);
    serviceBlock *selPtr = (serviceBlock*)&serviceQueue;
    serviceBlock *tstPtr = selPtr->next;
    while(tstPtr->next && tstPtr->next->scheduleTime <= millis()){
      trace(T_LOOP,6,2);
      if(tstPtr->next->priority > selPtr->next->priority){
        selPtr = tstPtr;
      }
      tstPtr = tstPtr->next;
    }
    trace(T_LOOP,6,3);
    tstPtr = selPtr;
    selPtr = selPtr->next;
    tstPtr->next = tstPtr->next->next;
    ESP.wdtFeed();
    trace(T_LOOP,5,selPtr->taskID);
    trace(T_LOOP,6,4);
    selPtr->scheduleTime = selPtr->service(selPtr);
    yield();
    trace(T_LOOP,6,6);
    if(selPtr->scheduleTime > 0){
      AddService(selPtr); 
    } else {
      delete selPtr;    
    }
  } 
}

/*****************************************************************************************************

                                    End of main Loop

             
 /******************************************************************************************************

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
 * always possible and doesn't do any real harm if they run over - just reduces the sampling frequency a bit.
 * 
 * Services return the UNIXtime of the next requested dispatch.  If the requested time is in the past, 
 * the service is requeued at the current time, so if a service just wants to relinquish but reschedule 
 * for the next available opportunity, just return 1.  If a service returns zero, it's service block will
 * be deleted.  To reschedule, AddService would have to be called to create a new serviceBlock.
 * 
 * The schedule itself is kept as an ordered list of control blocks in ascending order of time + priority.
 * Loop simply invokes the service currently at the beginning of the list.
 * 
 * The WiFi server is not one of these services.  It is invoked each time through the loop because it
 * polls for activity.
 ********************************************************************************************************/

serviceBlock* NewService(Service serviceFunction, const uint8_t taskID, void* parm){
    serviceBlock* newBlock = new serviceBlock;
    newBlock->service = serviceFunction;
    newBlock->taskID = taskID;
    newBlock->serviceParm = parm;
    AddService (newBlock);
    return newBlock;
  }

void AddService(struct serviceBlock* newBlock){
  uint32_t _millis = millis();
  if(newBlock->scheduleTime == 1){
    newBlock->scheduleTime = _millis;
  }
  else if(newBlock->scheduleTime <= 1000){
    newBlock->scheduleTime += _millis;
  }
  else {
    newBlock->scheduleTime = millisAtUTCTime(MAX(newBlock->scheduleTime, UTCtime()));
  }
  
  if(serviceQueue == NULL ||
    (newBlock->scheduleTime < serviceQueue->scheduleTime) ||
    (newBlock->scheduleTime == serviceQueue->scheduleTime && newBlock->priority > serviceQueue->priority)){
    newBlock->next = serviceQueue;
    serviceQueue = newBlock;
  }
  else {
    serviceBlock* link = serviceQueue;
    while(link->next != NULL){
      if((newBlock->scheduleTime < link->next->scheduleTime) ||
        (newBlock->scheduleTime == link->next->scheduleTime && newBlock->priority > link->next->priority)){
        break;
      }        
      link = link->next;
    } 
    newBlock->next = link->next;
    link->next = newBlock;
  }
}

/************************************************************************************************
 *  Program Trace Routines.
 *  
 *  This is a real handy part of the package since there is no interactive debugger.  The idea is
 *  to just drop breadcrumbs at key places so that in the event of an exception or wdt restart, we
 *  can at least get some idea where it happened.
 *  
 *  invoking trace() puts a 32 bit entry into the RTC_USER_MEM area.  
 *  After a restart, the 32 most recent entries are logged, oldest to most rent, 
 *  using logTrace.
 *************************************************************************************************/
void trace(const uint8_t module, const uint8_t id, const uint8_t det){
  traceEntry.seq++;
  traceEntry.mod = module;
  traceEntry.id = id;
  traceEntry.det = det;
  WRITE_PERI_REG(RTC_USER_MEM + 96 + (traceEntry.seq & 0x1F), (uint32_t) traceEntry.traceWord);
}

void logTrace(void){
  traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96);
  uint16_t _counter = traceEntry.seq;
  int i = 0;
  do {
    traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96 + (++i%32));
  } while(++_counter == traceEntry.seq);
  String line = "";
  for(int j=0; j<32; j++){
    traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96 + ((j+i)%32));
    line += ' ' + String(traceEntry.mod) + ':' + String(traceEntry.id);
    if(traceEntry.det == 0){
      line += ',';
    } else {
      line += "[" + String(traceEntry.det) + "],"; 
    }
  }
  line.remove(line.length()-1);  
  log("Trace: %s", line.c_str());
}