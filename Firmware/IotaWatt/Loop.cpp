#include "IotaWatt.h"
 
void loop()
{
  static int lastChannel = 0;

  /******************************************************************************
   * The main loop is very simple:
   * 
   *  Set the LED state.
   *  Sample a channel.
   *  Run the Wifi Server.
   *  Check for and update a new config
   *  Run next dispatchable service if there is one
   * 
   ******************************************************************************/

  setLedState();

  // Check for rollover of micros() clock and if so reset bingo time as well.

  if(micros() <= lastCrossUs){
    bingoTime = 0;
  }

  // If there are Inputs and next crossing is close ("bingo time"),
  // sample a cycle. 

  if(maxInputs && micros() > bingoTime){

    // Determine next channel to sample.

    trace(T_LOOP,1,lastChannel);
    int nextChannel = (lastChannel + 1) % maxInputs;
    while( (! inputChannel[nextChannel]->isActive()) && nextChannel != lastChannel){
      nextChannel = ++nextChannel % maxInputs;
    }
    trace(T_LOOP,2,nextChannel);

    // Sample it.

    ESP.wdtFeed();
    samplePower(nextChannel, 0);
    ESP.wdtFeed();

    // Set "bingo" time to micros when Services should return control in order to catch next AC cycle.

    if(int(frequency) > 25){
      bingoTime = lastCrossUs + 500000 / int(frequency) - 2000;
    }
    else {
      bingoTime = lastCrossUs + 6333;
    }
    
    // Indicate sampling active after one pass through inputs 

    if(nextChannel <= lastChannel){
      sampling = true;
    }
    lastChannel = nextChannel;
  }

  // Give web server a shout out.
  // serverAvailable will be false if there is a request being serviced by
  // an Iota SERVICE.

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
      log("Config update failed.");
    }
  }

// If the head of the service queue is dispatchable
// Find the highest priority Service that is dispatchable.
// Remove it from the serviceQueue
// call it
// Reschedule it.

  if(micros() < bingoTime && serviceQueue != NULL && millis() >= serviceQueue->scheduleTime){
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
 * interval. Bingo time is set as the micros() time when sampling should be resumed.
 * 
 * The WiFi server is invoked each time through the loop to check for work.
 * 
 * Other activities are organized as Services that are scheduled and dispatched in Loop during the half-cycle
 * downtime. This mechanism schedules at a resolution of one millisecond, and dispatches during the optimal 
 * time period between AC cycles.  To preserve context and synchonization, each Service is coded as a 
 * state-machine. They must be well behaved and should try to run for less than a few milliseconds and/or
 * relinquish at Bingo time. That isn't always possible and doesn't do any real harm if they run over
 * occasionally as it just reduces the sampling frequency a bit.
 * 
 * Services return a value that is used to set their reschedule time as follows:
 * 0 - Do not reschedule, deallocate the Service Block.
 * 1 - Set to redispatch immediately.
 * 2-1000 value is milliseconds to delay before redispatch.
 * > 1000 value is UNIXtime of requested redispatch.
 * So if a service just wants to relinquish in deference to sampling but is not finished with its
 * business, just reeturn 1 to be redispatched at the next available opportunity.
 * 
 * The schedule itself is kept as an ordered list of control blocks in ascending order of time + priority
 * called serviceQueue. Loop invokes the service with the highest priority that is dispatchable.
 * 
 * The following two functions are used to maintain the serviceQueue.
 * 
 * NewService creates a new serviceBlock that is immediately dispatchable. This is used to create an
 * instance of a Service and is mostly used at startup.  Ad-hoc Services can be created as well at any
 * time and they can terminate by simply returning zero.
 * 
 * AddService is the workhorse.  It inserts a serviceBlock into the serviceQueue in the appropriate
 * order based on its scheduleTime and priority.  When Services are dispatched, they are removed 
 * from the serviceQueue and then reinserted into the serviceQueue upon return using AddService.
 * 
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
