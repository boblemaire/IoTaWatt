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

#include "IotaWatt.h"
#include "IotaLog.h"
#include "IotaOutputChannel.h"

uint32_t handleGetFeedData(struct serviceBlock* _serviceBlock){
  // trace T_GFD
  enum   states {Initialize, Setup, process};
 
  static states state = Initialize;
  static IotaLogRecord* logRecord = new IotaLogRecord;
  static IotaLogRecord* lastRecord = new IotaLogRecord;
  static IotaLogRecord* swapRecord;
  static char* bufr = nullptr;
  static uint32_t bufrSize = 0;
  static uint32_t bufrPos = 0;
  static double accum1Then = 0;
  static double logHoursThen = 0;
  static double elapsedHours = 0;
  static uint32_t startUnixTime;
  static uint32_t endUnixTime;
  static uint32_t intervalSeconds;
  static uint32_t UnixTime;
  static int voltageChannel = 0;
  static boolean Kwh = false;
  static String replyData = "";
    
  struct req {
    req* next;
    int channel;
    int queryType;
    IotaOutputChannel* output;
    req(){next=nullptr; channel=0; queryType=0; output=nullptr;};
    ~req(){delete next;};
  } static reqRoot;
     
  switch (state) {
    case Initialize: {
      state = Setup;
      return 1;
    }
  
    case Setup: {
      trace(T_GFD,0);

          // Parse the ID parm into a list.
      
      String idParm = server.arg("id");
      reqRoot.next = nullptr;
      req* reqPtr = &reqRoot;
      int i = 0;
      if(idParm.startsWith("[")){
        idParm[idParm.length()-1] = ',';
        i = 1;
      } else {
        idParm += ",";
      }
      while(i < idParm.length()){
        reqPtr->next = new req;
        reqPtr = reqPtr->next;
        int id = idParm.substring(i,idParm.indexOf(',',i)).toInt();
        i = idParm.indexOf(',',i) + 1;
        reqPtr->channel = id / 10;
        reqPtr->queryType = id % 10;
        if(reqPtr->channel >= 100){
          IotaOutputChannel* _output = (IotaOutputChannel*)outputList.findFirst();
          while(_output){
            if(_output->_channel == reqPtr->channel){
              break;
            }
            _output = (IotaOutputChannel*)outputList.findNext(_output);
          }
          reqPtr->output = _output;
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

          // Using String for a large buffer abuses the heap
          // and takes up a lot of time. We will build 
          // relatively short response elements with String
          // and copy them to this larger buffer.

      bufrSize = ESP.getFreeHeap() / 2;
      if(bufrSize > 4096) bufrSize = 4096;
      bufr = new char [bufrSize];

          // Setup buffer to do it "chunky-style"
      
      bufr[3] = '\r';
      bufr[4] = '\n'; 
      bufrPos = 5;
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.sendHeader("Accept-Ranges","none");
      server.sendHeader("Transfer-Encoding","chunked");
      server.send(200,"application/json","");

      replyData = "[";
      UnixTime = startUnixTime;
      state = process;
      _serviceBlock->priority = priorityLow;
      return 1;
    }
  
    case process: {
      trace(T_GFD,1);
      uint32_t exitTime = nextCrossMs + 5000 / frequency;              // Take an additional half cycle
      SPI.beginTransaction(SPISettings(SPI_FULL_SPEED, MSBFIRST, SPI_MODE0));

          // Loop to generate entries
      
      while(UnixTime <= endUnixTime) {
        logRecord->UNIXtime = UnixTime;
        int rtc = iotaLog.readKey(logRecord);
        trace(T_GFD,2);
        replyData += '[';  //  + String(UnixTime) + "000,";
        elapsedHours = logRecord->logHours - lastRecord->logHours;
        req* reqPtr = &reqRoot;
        while((reqPtr = reqPtr->next) != nullptr){
          int channel = reqPtr->channel;
          if(rtc || logRecord->logHours == lastRecord->logHours){
             replyData +=  "null";
          }
  
            // input channel

          else if(channel < 100){
            trace(T_GFD,3);       
            if(reqPtr->queryType == QUERY_VOLTAGE) {
              replyData += String((logRecord->channel[channel].accum1 - lastRecord->channel[channel].accum1) / elapsedHours,1);
            } 
            else if(reqPtr->queryType == QUERY_POWER) {
              replyData += String((logRecord->channel[channel].accum1 - lastRecord->channel[channel].accum1) / elapsedHours,1);
            }
            else if(reqPtr->queryType == QUERY_ENERGY) {
              replyData += String((logRecord->channel[channel].accum1 / 1000.0),2);
            }  
          }
  
           // output channel
          
          else {
            trace(T_GFD,4);
            if(reqPtr->output == nullptr){
              replyData += "null";
            }
            else if(reqPtr->queryType == QUERY_ENERGY){
              replyData += String(reqPtr->output->runScript([](int i)->double {
                return logRecord->channel[i].accum1 / 1000.0;}), 2);
            }
            else {
              replyData += String(reqPtr->output->runScript([](int i)->double {
                return (logRecord->channel[i].accum1 - lastRecord->channel[i].accum1) / elapsedHours;}), 1);
            }
          }
          replyData += ',';
        } 
           
        replyData.setCharAt(replyData.length()-1,']');
        swapRecord = lastRecord;
        lastRecord = logRecord;
        logRecord = swapRecord;
        UnixTime += intervalSeconds;

            // When buffer is full, send a chunk.
        
        trace(T_GFD,5);
        if((bufrSize - bufrPos - 5) < replyData.length()){
          trace(T_GFD,6);
          sendChunk(bufr, bufrPos);
          bufrPos = 5;
        }

            // Copy this element into the buffer
        
        for(int i = 0; i < replyData.length(); i++) {
          bufr[bufrPos++] = replyData[i];  
        }
        replyData = ',';
      }
      trace(T_GFD,7);

          // All entries generated, terminate Json and send.
      
      replyData.setCharAt(replyData.length()-1,']');
      for(int i = 0; i < replyData.length(); i++) {
        bufr[bufrPos++] = replyData[i];  
      }
      sendChunk(bufr, bufrPos); 

          // Send terminating zero chunk, clean up and exit.
      
      sendChunk(bufr, 5); 
      trace(T_GFD,7);
      delete reqRoot.next;
      delete[] bufr;
      state = Setup;
      serverAvailable = true;
      return 0;                                       // Done for now, return without scheduling.
    }
  }
}

void sendChunk(char* bufr, const uint32_t bufrPos){
  trace(T_GFD,9);
  const char* hexDigit = "0123456789ABCDEF";
  int _len = bufrPos - 5;
  bufr[0] = hexDigit[_len/256];
  bufr[1] = hexDigit[(_len/16) % 16];
  bufr[2] = hexDigit[_len % 16]; 
  bufr[bufrPos] = '\r';
  bufr[bufrPos+1] = '\n';
  bufr[bufrPos+2] = 0;
  server.sendContent(bufr);
} 
   
