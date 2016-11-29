void loop()
{
  millis_now = millis();
  if(((unsigned long)(millis_now - last_post_time)) >= post_interval_ms)
  {
    post_time = last_post_time + post_interval_ms;
    sample_interval = post_time - last_sample_time;
    for(int i=1; i <= channels; i++) 
    {
      averageWatts[i] = (watt_ms[i] + activePower[i] * sample_interval) / post_interval_ms;
      if(averageWatts[i] < 0.0) averageWatts[i] = -averageWatts[i];
      if(averageWatts[i] < 1.0) averageWatts[i] = 0.0;
      watt_ms[i] = 0;
      
      averageVA[i] = (VA_ms[i] + apparentPower[i] * sample_interval) / post_interval_ms;
      if(averageVA[i] < 1.0) averageVA[i] = 0.0;
      VA_ms[i] = 0;
    }
    post_power();
    last_sample_time = post_time;
    last_post_time = post_time;
  }
  else
  {
    sample_interval = millis_now - last_sample_time;
    for(int i=1; i <= channels; i++) 
    {
      watt_ms[i] += activePower[i] * sample_interval;
      VA_ms[i] += apparentPower[i] * sample_interval;
    }
    last_sample_time = millis_now;
  }

  sample_power();
  yield();

  if(reply_pending) checkdata();  
  yield();

}



