#include <Arduino.h>
#include <ESP8266httpUpdate.h>

#include "IotaWatt.h"
#include "msgLog.h"
#include "timeServices.h"

void AddService(struct serviceBlock* newBlock);
  
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

  if((uint32_t)(millis() - lastCrossMs) >= (490 / int(frequency))){
    ESP.wdtFeed();
    trace(T_LOOP,1);
    samplePower(nextChannel, 0);
    trace(T_LOOP,2);
    nextCrossMs = lastCrossMs + 490 / int(frequency);
    while( ! inputChannel[++nextChannel % maxInputs]);
    nextChannel = nextChannel % maxInputs;
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
    trace(T_LOOP,5);
    thisBlock->callTime = thisBlock->service(thisBlock);
    trace(T_LOOP,6);
    if(thisBlock->callTime > 0){
      AddService(thisBlock); 
    } else {
      delete thisBlock;    
    }
  }

// ----------- Another shout out to the Web 
     
  yield();
  ESP.wdtFeed();
  if(serverAvailable){
    trace(T_LOOP,7);
    server.handleClient();
    trace(T_LOOP,8);
    yield();
  }
}

/*****************************************************************************************************

                                    End of main Loop
             
******************************************************************************************************

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

void NewService(uint32_t (*serviceFunction)(struct serviceBlock*)){
    serviceBlock* newBlock = new serviceBlock;
    newBlock->service = serviceFunction;
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

/******************************************************************************************************** 
 * All of the other SERVICEs that harvest values from the main "buckets" do so for their own selfish 
 * purposes, and have no global scope to share with others. This simple service maintains periodic
 * values for each of the buckets in a global set of buckets called statBucket.  It also is where 
 * status statistics like sample rates are maintained.  
 *******************************************************************************************************/

uint32_t statService(struct serviceBlock* _serviceBlock) { 
  static uint32_t timeThen = millis();        
  static boolean started = false;
  static float damping = .5;
  uint32_t timeNow = millis();

  if(!started){
    msgLog("statService: started.");
    started = true;
    for(int i=0; i<maxInputs; i++){
      statBucket[i].accum1 = inputChannel[i]->dataBucket.accum1;
      statBucket[i].accum2 = inputChannel[i]->dataBucket.accum2;
    }
    return (uint32_t)UNIXtime() + 1;
  }
  
  double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;
  for(int i=0; i<maxInputs; i++){
    inputChannel[i]->ageBuckets(timeNow); 
    statBucket[i].value1 = (damping * statBucket[i].value1) + ((1.0 - damping) * (inputChannel[i]->dataBucket.accum1 - statBucket[i].accum1) / elapsedHrs);
    statBucket[i].value2 = (damping * statBucket[i].value2) + ((1.0 - damping) * (inputChannel[i]->dataBucket.accum2 - statBucket[i].accum2) / elapsedHrs);
    statBucket[i].accum1 = inputChannel[i]->dataBucket.accum1;
    statBucket[i].accum2 = inputChannel[i]->dataBucket.accum2;
  }
  
  cycleSampleRate = damping * cycleSampleRate + (1.0 - damping) * float(cycleSamples * 1000) / float((uint32_t)(timeNow - timeThen));
  cycleSamples = 0;
  timeThen = timeNow;
  
  return ((uint32_t)UNIXtime() + statServiceInterval);
}

/************************************************************************************************
 *  Program Trace Routines.
 *  
 *  This is a real handy part of the package since there is no interactive debugger.  The idea is
 *  to just drop breadcrumbs at key places so that in the event of an exception or wdt restart, we
 *  can at least get some idea where it happened.
 *  
 *  invoking trace() puts a 16 bit entry with highbyte=module and lowbyte=seq into the 
 *  RTC_USER_MEM area.  After a restart, the 32 most recent entries are logged, oldest to most rent, 
 *  using logTrace.
 *************************************************************************************************/

void trace(uint32_t module, int seq){
  static uint16_t traceSeq = 0;
  static uint32_t entry;
  entry = (module+seq) | (traceSeq++ << 16);
  WRITE_PERI_REG(RTC_USER_MEM + 96 + (traceSeq & 0x1F), entry);
}

void logTrace(void){
  uint16_t _counter = READ_PERI_REG(RTC_USER_MEM + 96) >> 16;
  int i = 1;
  while(((uint16_t)++_counter) == (READ_PERI_REG(RTC_USER_MEM + 96 + (i%32)) >> 16)) i++;
  String line = "Trace: ";
  for(int j=0; j<32; j++){
    uint32_t _entry = READ_PERI_REG(RTC_USER_MEM + 96 + ((j+i)%32)) & 0xFFFF;
    line += String(_entry) + ",";
  }
  line.remove(line.length()-1);  
  msgLog(line);
}

/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock) { 
  static bool upToDateMsg = false;
  ESPhttpUpdate.rebootOnUpdate(false);
  trace(T_UPDATE,0);
  t_httpUpdate_return ret = ESPhttpUpdate.update("iotawatt.com", 80, "/firmware/update.php", IOTAWATT_VERSION);
  trace(T_UPDATE,1);
  switch(ret) {
    case HTTP_UPDATE_FAILED:
        msgLog("update: Update failed.", ESPhttpUpdate.getLastErrorString());
        return ((uint32_t)UNIXtime() + 5*60);
    case HTTP_UPDATE_NO_UPDATES:
        if( ! upToDateMsg){
          msgLog("update: Firmware is up to date.");
          upToDateMsg = true;
        }
        break;
    case HTTP_UPDATE_OK:
        msgLog("update: Firmware updated.");
        delay(500);
        ESP.restart();
        break;
  }  
  return ((uint32_t)UNIXtime() + updaterServiceInterval);
}


