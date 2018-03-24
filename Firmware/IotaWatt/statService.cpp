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
  static float  damping = .25;
  static double accum1Then[MAXINPUTS];
  static double accum2Then[MAXINPUTS];
  uint32_t timeNow = millis();

  if(!started){
    msgLog(F("statService: started."));
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
    inputChannel[i]->ageBuckets(timeNow);
    double newValue = (inputChannel[i]->dataBucket.accum1 - accum1Then[i]) / elapsedHrs;
    if(abs(statRecord.accum1[i] - newValue) / abs(statRecord.accum1[i]) < 0.02){
      statRecord.accum1[i] = damping * statRecord.accum1[i] + (1.0 - damping) * newValue;
    }
    else statRecord.accum1[i] = newValue;
    newValue = (inputChannel[i]->dataBucket.accum2 - accum2Then[i]) / elapsedHrs;
    if(abs(statRecord.accum2[i] - newValue) / abs(statRecord.accum2[i]) < 0.02){
      statRecord.accum2[i] = damping * statRecord.accum2[i] + (1.0 - damping) * newValue;
    }
    else statRecord.accum2[i] = newValue;
    accum1Then[i] = inputChannel[i]->dataBucket.accum1;
    accum2Then[i] = inputChannel[i]->dataBucket.accum2;
  }
  
  cycleSampleRate = damping * cycleSampleRate + (1.0 - damping) * float(cycleSamples * 1000) / float((uint32_t)(timeNow - timeThen));
  cycleSamples = 0;
  timeThen = timeNow;
  
  return ((uint32_t)UNIXtime() + statServiceInterval);
}

