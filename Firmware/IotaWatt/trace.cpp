#include "IotaWatt.h"

/************************************************************************************************
 *  Program Trace Routines.
 *  
 *  This is a real handy part of the package since there is no interactive debugger.  The idea is
 *  to just drop breadcrumbs at key places so that in the event of an exception or wdt restart, we
 *  can at least get some idea where it happened.
 *  
 *  invoking trace() puts a 32 bit entry into the RTC_USER_MEM area.  
 *  After a restart, the 32 most recent entries are logged, oldest to most rent, 
 *  using logTrace.
 *************************************************************************************************/
void trace(const uint8_t module, const uint8_t id, const uint8_t det){
  traceEntry.seq++;
  traceEntry.mod = module;
  traceEntry.id = id;
  traceEntry.det = det;
  WRITE_PERI_REG(RTC_USER_MEM + 96 + (traceEntry.seq & 0x1F), (uint32_t) traceEntry.traceWord);
}

void logTrace(void){
  traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96);
  uint16_t _counter = traceEntry.seq;
  int i = 0;
  do {
    traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96 + (++i%32));
  } while(++_counter == traceEntry.seq);
  String line = "";
  for(int j=0; j<32; j++){
    traceEntry.traceWord = READ_PERI_REG(RTC_USER_MEM + 96 + ((j+i)%32));
    line += ' ' + String(traceEntry.mod) + ':' + String(traceEntry.id);
    if(traceEntry.det == 0){
      line += ',';
    } else {
      line += "[" + String(traceEntry.det) + "],"; 
    }
  }
  line.remove(line.length()-1);  
  log("Trace: %s", line.c_str());
}