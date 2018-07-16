#include "IotaWatt.h"

/******************************************************************************************************** 
 * All of the other SERVICEs that harvest values from the main "buckets" do so for their own selfish 
 * purposes, and have no global scope to share with others. This simple service maintains periodic
 * values for each of the buckets in a global set of buckets called statBucket.  It also is where 
 * status statistics like sample rates are maintained.  
 *******************************************************************************************************/

uint32_t statService(struct serviceBlock* _serviceBlock) { 
  static boolean started = false;
  static uint32_t timeThen = millis();        
  static double accum1Then[MAXINPUTS];
  static double accum2Then[MAXINPUTS];
  uint32_t timeNow = millis();

  trace(T_stats, 0);
  if(!started){
    trace(T_stats, 1);
    log("statService: started.");
    started = true;
    for(int i=0; i<maxInputs; i++){
      accum1Then[i] = inputChannel[i]->dataBucket.accum1;
      accum2Then[i] = inputChannel[i]->dataBucket.accum2;
      statRecord.accum1[i] = 0.0;
      statRecord.accum2[i] = 0.0;
    }
    return (uint32_t)UNIXtime() + 1;
  }
  
  double elapsedHrs = double((uint32_t)(timeNow - timeThen)) / MS_PER_HOUR;

  for(int i=0; i<maxInputs; i++){
    trace(T_stats, 2);
    inputChannel[i]->ageBuckets(timeNow);
    double newValue = (inputChannel[i]->dataBucket.accum1 - accum1Then[i]) / elapsedHrs;
    float damping = .75;
    if((newValue / statRecord.accum1[i]) < .98 || (newValue / statRecord.accum1[i]) > 1.02){
      damping = 0.0;
    }
    statRecord.accum1[i] = damping * statRecord.accum1[i] + (1.0 - damping) * newValue;
    newValue = (inputChannel[i]->dataBucket.accum2 - accum2Then[i]) / elapsedHrs;
    statRecord.accum2[i] = damping * statRecord.accum2[i] + (1.0 - damping) * newValue;
    trace(T_stats, 3);
    accum1Then[i] = inputChannel[i]->dataBucket.accum1;
    accum2Then[i] = inputChannel[i]->dataBucket.accum2;
  }
  trace(T_stats, 4);
  cycleSampleRate = .25 * cycleSampleRate + (1.0 - .25) * float(cycleSamples * 1000) / float((uint32_t)(timeNow - timeThen));
  cycleSamples = 0;
  if(heapMsPeriod > 300000){
    heapMs = 0.0;
    heapMsPeriod = 0;
  }
  heapMs += ESP.getFreeHeap() * (timeNow - timeThen);
  heapMsPeriod += timeNow - timeThen;
  timeThen = timeNow;
  trace(T_stats, 5);
  return ((uint32_t)UNIXtime() + statServiceInterval);
}

