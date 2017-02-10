/***************************************************************************************************
 *  GetFeedData SERVICE.
 *  
 *  The web server does a pretty good job of handling file downloads and uploads asynchronously, 
 *  but assumes that any callbacks to new handlers get the job completely done befor returning. 
 *  The GET /feed/data/ request takes a long time and generates a lot of data so it needs to 
 *  run as a SERVICE so that sampling can continue while it works on providing the data.  
 *  To accomplish that without modifying ESP8266WebServer, we schedule this SERVICE and
 *  block subsequent calls to server.handleClient() until the request is satisfied, at which time
 *  this SERVICE returns with code 0 to cause it's serviceBlock to be deleted.  When a new /feed/data
 *  request comes in, the web server handler will reshedule this SERVICE with NewService.
 * 
 **************************************************************************************************/

uint32_t handleGetFeedData(struct serviceBlock* _serviceBlock){
  // trace T_GFD
  enum   states {Initialize, Setup, process};
  static states state = Initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static double accum1Then = 0;
  static double accum2Then = 0;
  static double voltageThen = 0;
  static double logHoursThen = 0;
  static double elapsedHours = 0;
  static uint32_t startUnixTime;
  static uint32_t endUnixTime;
  static uint32_t intervalSeconds;
  static uint32_t UnixTime;
  static int channel;
  static int queryType;
  static int intervalNumber;
  static int voltageChannel = 0;
  static boolean Kwh = false;
  static String replyData = "";
  static int directReads = 0;

  switch (state) {
    case Initialize: {
      state = Setup;
      return 0;
    }
  
    case Setup: {
      trace(T_GFD,0);
//      Serial.print(server.arg("id"));
//      Serial.print(" ");
//      Serial.print(server.arg("start"));
//      Serial.print(" ");
//      Serial.print(server.arg("end"));
//      Serial.print(" ");
//      Serial.print(server.arg("interval"));
//      Serial.print(" ");
//      Serial.println();
      directReads = 0;
  
      channel = server.arg("id").toInt() % 1000;
      queryType = channel % 10;
      channel /= 10;
    
      startUnixTime = server.arg("start").substring(0,10).toInt();
      endUnixTime = server.arg("end").substring(0,10).toInt();
      intervalSeconds = server.arg("interval").toInt();
    
    
      logRecord->UNIXtime = startUnixTime - intervalSeconds;
      iotaLog.readKey(logRecord);
      accum1Then = logRecord->channel[channel].accum1;
      accum2Then = logRecord->channel[channel].accum2;
     
      if(queryType == QUERY_PF){
        voltageChannel = Vchannel[channel];
        voltageThen = logRecord->channel[channel].accum1;
      }
      logHoursThen = logRecord->logHours; 
      
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.sendHeader("Accept-Ranges","none");
      server.sendHeader("Transfer-Encoding","chunked");
      server.send(200,"application/json","");
      
      replyData = "[";
      intervalNumber = 0;
      UnixTime = startUnixTime;
  
      state = process;
      _serviceBlock->priority = priorityLow;
      serverAvailable = false;
      return 1;
    }
  
    case process: {
      trace(T_GFD,1);
      uint32_t exitTime = nextCrossMs + 500 / frequency;              // Take an additional half cycle
      while(UnixTime <= endUnixTime) {

        // Make a play for the record by sequence number.

        logRecord->serial += (UnixTime - logRecord->UNIXtime) / dataLogInterval - 1;
        iotaLog.readNext(logRecord);
        if(logRecord->UNIXtime == UnixTime) directReads++;
        else {
          logRecord->UNIXtime = UnixTime;
          iotaLog.readKey(logRecord);
        }
        trace(T_GFD,2);
        if(logRecord->logHours == logHoursThen){
          // replyData += "[" + String(intervalNumber) + ",null]";
          replyData += "[" + String(UnixTime) + "000,null]";
        }
        else {
          elapsedHours = logRecord->logHours - logHoursThen;
          replyData += "[" + String(UnixTime) + "000,";
          trace(T_GFD,3);
                    
          switch (queryType) {
            case QUERY_VOLTAGE: {
              replyData += String((logRecord->channel[channel].accum1 - accum1Then) / elapsedHours,1) + "]";
              break;
            }
            case QUERY_FREQUENCY: {
              replyData += String((logRecord->channel[channel].accum2 - accum2Then) / elapsedHours,2) + "]";
              break;
            }
            case QUERY_POWER: {
              replyData += String((logRecord->channel[channel].accum1 - accum1Then) / elapsedHours,0) + "]";
              break;
            }
            case QUERY_ENERGY: {
              replyData += String((logRecord->channel[channel].accum1 / 1000.0),1) + "]";
              break;
            }
            case QUERY_PF: {
              if(((logRecord->channel[channel].accum1 - accum1Then) / elapsedHours) < 50){
                replyData += "null]";
              }
              else {
                replyData += String(((logRecord->channel[channel].accum1 - accum1Then) / elapsedHours) / 
                (((logRecord->channel[channel].accum2 - accum2Then) / elapsedHours) * ((logRecord->channel[voltageChannel].accum1 - voltageThen) / elapsedHours)),2) + "]";
              }
              break;
            }
          }
        }
        accum1Then = logRecord->channel[channel].accum1;
        accum2Then = logRecord->channel[channel].accum2;
        voltageThen = logRecord->channel[voltageChannel].accum1;
        logHoursThen = logRecord->logHours;
        trace(T_GFD,4);
        if(replyData.length() > 1024){
          trace(T_GFD,5);
          yield();
          sendChunk(replyData);
          replyData = "";
          yield();
        }
        replyData += ",";
        UnixTime += intervalSeconds;
        intervalNumber++;
        if(millis() >= exitTime){
          return 1;
        }
      }
      trace(T_GFD,6);
      yield();
      replyData.setCharAt(replyData.length()-1,']');
      sendChunk(replyData); 
      yield();
      replyData = "";
      sendChunk(replyData); 
      trace(T_GFD,7);
      serverAvailable = true;
      state = Setup;
      return 0;                                       // Done for now, return without scheduling.
    }
  }
}

void sendChunk(String replyData){
  char * chunkHeader = "000\r\n";
  const char* hexDigit = "0123456789ABCDEF";
  int _len = replyData.length();
  chunkHeader[0] = hexDigit[_len/256];
  chunkHeader[1] = hexDigit[(_len/16) % 16];
  chunkHeader[2] = hexDigit[_len % 16];
  server.sendContent(String(chunkHeader));
  replyData += "\r\n";
  server.sendContent(replyData);
}     
