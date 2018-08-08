
#include "IotaWatt.h"

void getSamples(){ //(struct serviceBlock* _serviceBlock){
  // trace T_GFD

 
  size_t   chunkSize = 1600;
  char* buf = new char[chunkSize+8];
  int bufPos = 6;
    
      // Setup buffer to do it "chunky-style"
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
  bufPos += sprintf_P(buf+bufPos , PSTR("samples %d\r\n"), samples);

      // Loop to generate entries
  
  for(int i=0; i<samples; i++){
    bufPos += sprintf_P(buf+bufPos, PSTR("%d,%d\r\n"),Vsample[i], Isample[i]);

    if(bufPos > (chunkSize - 15)){
      sendChunk(buf, bufPos);
      bufPos = 6;
    }    
  }

      // All entries generated.
  
  if(bufPos > 6){
    sendChunk(buf, bufPos);
  }
  
      // Send terminating zero chunk, clean up and exit.    
  
  sendChunk(buf, 6);
  trace(T_GFD,7);
  delete[] buf;
}