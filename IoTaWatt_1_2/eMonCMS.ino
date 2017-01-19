/**********************************************************************************************
 * postEmonCMS is a service that is invoked initially and must thereafter reschedule itself.
 * 
 * All services have the single function call and are implimented as state machines.
 * Services should try not to execute for more than a few milliseconds at a time.
 **********************************************************************************************/
int postEmonCMS(struct serviceBlock* _serviceBlock){

  enum states {initialize, post};
  static states state = initialize;

  _serviceBlock->callTime = (uint32_t) eMonCMSInterval * (1 + NTPtime() / eMonCMSInterval);
  AddService(_serviceBlock);
  
  static double accum1Then [channels];
  static IotaTime timeThen; 
  IotaTime timeNow = millis();
  
  switch(state){  

    case initialize: {
      for(int i=0; i<channels; i++){ 
        ageBucket(&buckets[i], timeNow);
        accum1Then[i] = buckets[i].accum1;
      }
      timeThen = timeNow;
      Serial.println("EmonCMS service started.");
      state = post;
      break;
    }

    case post: {
      double value1 [channels];
      double elapsedHrs = double((IotaTime)(timeNow - timeThen)) / MS_PER_HOUR;
      for(int i=0; i<channels; i++){ 
        ageBucket(&buckets[i], timeNow);
        value1[i] = (double)(buckets[i].accum1 - accum1Then[i]) / elapsedHrs;
        accum1Then[i] = buckets[i].accum1;
      }
      timeThen = timeNow;
      if (reply_pending) {
        if(Serial)Serial.println("Previous request timed out.");
        cloudServer.flush();
        cloudServer.stop();
        reply_pending = false;
      }
    
      if(!cloudServer.connected()) {
        cloudServer.flush();
        cloudServer.stop();
        char url[80];
        cloudURL.toCharArray(url, 80); 
        int connect_rtc = cloudServer.connect(url, 80);
        if (connect_rtc != 1) {
          if(Serial) {
            Serial.print("failed to connect to:");
            Serial.print(cloudURL);
            Serial.print(", rtc=");
            Serial.println(connect_rtc);
          }
          return -1;
        }
      }
    
      String req = "GET /input/post.json?&node=" + String(node) + "&csv=";
      int commas = 0;
      for (int i = 0; i < channels; i++) {
        if(value1 != 0) {
          while(commas > 0) {
            req += ",";
            commas--;
          }
          if(channelType[i] == channelTypeVoltage) req += String(value1[i],1);
          else if(channelType[i] == channelTypePower) req += String(long(value1[i]+0.5));
          else req += String(long(value1[i]+0.5));
        }
        commas++;
      }
      req += "&apikey=" + apiKey;
         
      cloudServer.println(req);
      if(Serial)Serial.println(req);
      reply_pending = true;
      break;
    }
  }
  return 0;
}

void checkdata() {
  if (!reply_pending) return;

  int data_length = cloudServer.available();
  if (data_length < 2) return;
  char o = cloudServer.read();
  char k = cloudServer.read();
  if (o == 'o' & k == 'k')reply_pending = false;
  else {
    if(Serial)Serial.print(o); Serial.print(k);
    while (cloudServer.available()) if(Serial)Serial.print(cloudServer.read());
  }
  while (cloudServer.available()) cloudServer.read();
  cloudServer.stop();
  return;
}




