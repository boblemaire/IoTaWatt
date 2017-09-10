#ifndef timeServices_h
#define timeServices_h

#include <Arduino.h>

uint32_t NTPtime();
uint32_t UNIXtime();    

void dateTime(uint16_t* date, uint16_t* time);
  
uint32_t timeSync(struct serviceBlock* _serviceBlock);
  
#endif // timeServices_h
