/**********************************************************************************************
 * dataLog is a service that is invoked initially and must thereafter reschedule itself.
 * 
 * All services have the single function call and are implimented as state machines.
 * Services should try not to execute for more than a few milliseconds at a time.
 **********************************************************************************************/
 int dataLog(struct serviceBlock* _serviceBlock){

  enum states {initialize, logData};
  static states state = initialize;
  
  static double accum1Then [channels];
  static IotaTime timeThen = 0;
  IotaTime timeNow = millis();
  
  switch(state){

    case initialize: {
      
      for(int i=0; i<channels; i++){
        ageBucket(&buckets[i], timeNow);
        accum1Then[i] = buckets[i].accum1;
      }
      timeThen = timeNow;
      Serial.println("dataLog service started.");
      state = logData;
      break;
    }
 
    case logData: {
      double elapsedHrs = double((IotaTime)(timeNow - timeThen)) / MS_PER_HOUR;
      for(int i=0; i<channels; i++){
        ageBucket(&buckets[i], timeNow);
        double value1 = (buckets[i].accum1 - accum1Then[i]) / elapsedHrs;
        accum1Then[i] = buckets[i].accum1;
        if(value1 > 0.9){
          Serial.print(", ");
          Serial.print(i);
          Serial.print(")");
          Serial.print(value1,0);
        }
      }
      Serial.println();
      timeThen = timeNow;
      break;
    }
  }

  _serviceBlock->callTime = (uint32_t) dataLogInterval * (1 + NTPtime() / dataLogInterval);
  AddService(_serviceBlock);
  
}

