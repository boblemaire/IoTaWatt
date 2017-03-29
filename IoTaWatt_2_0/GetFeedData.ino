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
  static IotaLogRecord* lastRecord = new IotaLogRecord;
  static IotaLogRecord* swapRecord;
  static IotaOutputChannel* _output;
  static double accum1Then = 0;
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
  
  static uint32_t timeIO;
  static uint32_t timeCOM;
  static uint32_t timeWAIT;
  static uint32_t timeTOT;
  static uint32_t timePER;
  static uint32_t timeSTART;
  
  static uint32_t Init;
  static uint32_t Io;
  static uint32_t Send;
  static uint32_t Idle;
  static uint32_t Comp;
  static uint32_t Start;
    
  switch (state) {
    case Initialize: {
      state = Setup;
      return 1;
    }
  
    case Setup: {
      trace(T_GFD,0);

      timeIO = 0;
      timeCOM = 0;
      timeWAIT = 0;
      timePER = micros();
      timeSTART = micros();
        
      channel = server.arg("id").toInt();
      queryType = channel % 10;
      channel /= 10;
      if(channel >= 100){
        _output = (IotaOutputChannel*)outputList.findFirst();
        while(_output){
          if(_output->_channel == channel){
            Serial.println(_output->_name);
            break;
          }
          _output = (IotaOutputChannel*)outputList.findNext(_output);
        }
      }
    
      startUnixTime = server.arg("start").substring(0,10).toInt();
      endUnixTime = server.arg("end").substring(0,10).toInt();
      intervalSeconds = server.arg("interval").toInt();
    
      accum1Then = 0;
      logHoursThen = 0;

      if(startUnixTime >= iotaLog.firstKey()){   
        lastRecord->UNIXtime = startUnixTime - intervalSeconds;
      } else {
        lastRecord->UNIXtime = iotaLog.firstKey();
      }
      if(!iotaLog.readKey(lastRecord)){
        
      } 
     
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.sendHeader("Accept-Ranges","none");
      server.sendHeader("Transfer-Encoding","chunked");
      server.send(200,"application/json","");

      replyData = "[";
      intervalNumber = 0;
      UnixTime = startUnixTime;
  
      state = process;
      _serviceBlock->priority = priorityLow;
      return 1;
      
      timePER = micros();
      
    }
  
    case process: {
      timeit(timePER,timeWAIT);
      trace(T_GFD,1);
      uint32_t exitTime = nextCrossMs + 5000 / frequency;              // Take an additional half cycle
      SPI.beginTransaction(SPISettings(SPI_FULL_SPEED, MSBFIRST, SPI_MODE0));
      while(UnixTime <= endUnixTime) {
        logRecord->UNIXtime = UnixTime;
        timePER = micros();
        int rtc = iotaLog.readKey(logRecord);
        timeit(timePER,timeIO);
        trace(T_GFD,2);
        replyData += '[' + String(UnixTime) + "000,";
        elapsedHours = logRecord->logHours - lastRecord->logHours;
        if(rtc || logRecord->logHours == lastRecord->logHours){
           replyData +=  "null";
        }

          // input channel
        
        else if(channel < 100){       
          trace(T_GFD,3);                  
          switch (queryType) {
            case QUERY_VOLTAGE: {
              replyData += String((logRecord->channel[channel].accum1 - lastRecord->channel[channel].accum1) / elapsedHours,1);
              break;
            }
            case QUERY_POWER: {
              replyData += String((logRecord->channel[channel].accum1 - lastRecord->channel[channel].accum1) / elapsedHours,1);
              break;
            }
            case QUERY_ENERGY: {
              replyData += String((logRecord->channel[channel].accum1 / 1000.0),2);
              break;
            }
          }  
        }

         // output channel
        
        else {
          trace(T_GFD,4);
          
          if(queryType == QUERY_ENERGY){
            replyData += String(_output->runScript([](int i)->double {
              return logRecord->channel[i].accum1 / 1000.0;}), 2);
          }
          else {
            replyData += String(_output->runScript([](int i)->double {
              return (logRecord->channel[i].accum1 - lastRecord->channel[i].accum1) / elapsedHours;}), 1);
          }
        } 
           
        replyData += ']';
        swapRecord = lastRecord;
        lastRecord = logRecord;
        logRecord = swapRecord;
        
        trace(T_GFD,5);
        if(replyData.length() > 2048){
          trace(T_GFD,6);
          yield();
          timePER = micros();
          sendChunk(replyData);
          timeit(timePER,timeCOM);
          yield();
          replyData = "";
        }
        replyData += ',';
        UnixTime += intervalSeconds;
        intervalNumber++;
        if(millis() >= exitTime){
          timePER = micros();
          return 1;
        }
      }
      trace(T_GFD,7);
      yield();
      replyData.setCharAt(replyData.length()-1,']');
      timePER = micros();
      sendChunk(replyData); 
      
      yield();
      replyData = "";
      sendChunk(replyData); 
      timeit(timePER,timeCOM);
      trace(T_GFD,7);
      serverAvailable = true;
      state = Setup;
      timeTOT = micros() - timeSTART;
//      PRINTL("IO:",timeIO)
//      PRINTL("COM:",timeCOM)
//      PRINTL("WAIT:",timeWAIT)
//      PRINTL("Elapsed:",timeTOT);
      return 0;                                       // Done for now, return without scheduling.
    }
  }
}

void timeit(uint32_t &timePER,uint32_t &timeACCT){
  timeACCT += micros() - timePER;
  timePER = micros();
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
