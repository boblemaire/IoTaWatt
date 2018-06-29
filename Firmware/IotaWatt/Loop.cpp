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

  if((uint32_t)(millis() - lastCrossMs) >= (430 / int(frequency))){
    ESP.wdtFeed();
    trace(T_LOOP,1,nextChannel);
    samplePower(nextChannel, 0);
    trace(T_LOOP,2);
    nextCrossMs = lastCrossMs + 490 / int(frequency);
    while( ! inputChannel[++nextChannel % maxInputs]->isActive());
    nextChannel = nextChannel % maxInputs;
    trace(T_LOOP,2,nextChannel);
  }

  // --------- Give web server a shout out.
  //           serverAvailable will be false if there is a request being serviced by
  //           an Iota SERVICE. (GetFeedData)

  yield();
  ESP.wdtFeed();
  trace(T_LOOP,3);
  if(serverAvailable){
    server.handleClient();
    trace(T_LOOP,4);
    yield();
  }
  

// ---------- If the head of the service queue is dispatchable
//            call the SERVICE.

  if(serviceQueue != NULL && UNIXtime() >= serviceQueue->callTime){
    serviceBlock* thisBlock = serviceQueue;
    serviceQueue = thisBlock->next;
    ESP.wdtFeed();
    trace(T_LOOP,5,thisBlock->taskID);
    thisBlock->callTime = thisBlock->service(thisBlock);
    yield();
    trace(T_LOOP,6);
    if(thisBlock->callTime > 0){
      AddService(thisBlock); 
    } else {
      delete thisBlock;    
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

void NewService(uint32_t (*serviceFunction)(struct serviceBlock*), const uint8_t taskID){
    serviceBlock* newBlock = new serviceBlock;
    newBlock->service = serviceFunction;
    newBlock->taskID = taskID;
    AddService (newBlock);
  }

void AddService(struct serviceBlock* newBlock){
  if(newBlock->callTime < UNIXtime()) newBlock->callTime = UNIXtime();
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