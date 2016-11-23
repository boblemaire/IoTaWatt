void post_power()
{
  String req = "GET /input/post.json?"
               "&node=";
  req += String(node);

  // If Serial link is active, log power to it.
  
  if(Serial)
  {
    Serial.print(", VRMS=");
    Serial.print(Vrms,1);
    Serial.print("(");
    Serial.print(offset[0]);
    Serial.print(",");
    Serial.print(samplesPerCycle);
    Serial.print(")");
    for(int i=1; i <= channels; i++)
    {
      if(channelActive[i])
      {
        Serial.print(", (");
        Serial.print(i);
        Serial.print(")");
        Serial.print(averageWatts[i],0);
//      Uncomment to print power factor
        Serial.print("[");                              
        Serial.print(averageWatts[i]/averageVA[i],4);
        Serial.print("]"); 
      }
    }
    Serial.println();
  }

  if (reply_pending)
  {
    if(Serial)Serial.println("Previous request timed out.");
    cloudServer.flush();
    cloudServer.stop();
    reply_pending = false;
  }

  if(!cloudServer.connected())
  {
    cloudServer.flush();
    cloudServer.stop();
    check_internet();
    char url[80];
    cloudURL.toCharArray(url, 80); 
    int connect_rtc = cloudServer.connect(url, 80);
    if (connect_rtc != 1)
    {
      if(Serial)
      {
        Serial.print("failed to connect to:");
        Serial.print(cloudURL);
        Serial.print(", rtc=");
        Serial.println(connect_rtc);
      }
      return;
    }
  }

  req += "&csv=";
  int commas = 0;
  for (int i = 1; i <= channels; i++)
  {
    if(channelActive[i])
    {
      while(commas > 0) 
      {
        req += ",";
        commas--;
      }
      req += String(long(averageWatts[i]+.5));
    }
    commas++;
  }
  req += "&apikey=";
  req += apikey;

  cloudServer.println(req);
  if(Serial)Serial.println(req);
  reply_pending = true;

  return;
}

void checkdata()
{

  if (!reply_pending) return;

  int data_length = cloudServer.available();
  if (data_length < 2) return;

  char o = cloudServer.read();
  char k = cloudServer.read();
  if (o == 'o' & k == 'k')reply_pending = false;
  else
  {
    if(Serial)Serial.print(o); Serial.print(k);
    while (cloudServer.available()) if(Serial)Serial.print(cloudServer.read());
  }
  while (cloudServer.available()) cloudServer.read();
  cloudServer.stop();
  return;

}

boolean check_internet(void)
{
//  int rtc = Ethernet.maintain();

//  if(rtc == 1 || rtc == 3) return(false);
  return(true);  
}



