/*************************************************** 
  IoTa_MCP23S17.h
  
  Simple interface to MCP23S17 GPIO expander.
  impliments only:
  
  begin();
  digitalWrite(pin, LOW/HIGH);
  
  plan to add later:
  
  pinMode(pin, INPUT/OUTPUT);
  digitalRead(pin);
  
  Maybe interrupts much later;
  
  btw/ only need ESP8266 so coded and tested only for that.
  requires SPI.h be included and started before begin();
 ****************************************************/
#ifndef IoTa_MCP23S17_h
#define IoTa_MCP23S17_h
#include "inttypes.h"
#include "Arduino.h"
#include "SPI.h"
 
 class IoTa_MCP23S17
 {
 public:
  
  void begin(uint8_t _csPin);
  void writePin(uint8_t pin, uint8_t level);

 private:
 
  uint8_t csPin;
  uint16_t OLAT = 0;
  uint16_t GPIO = 0;
  uint32_t align = 0;
  byte spiOUT[4] = {0,0,0,0};
  byte spiIN[4] = {0,0,0,0};
  
  void writeRegs(uint8_t addr, uint16_t data);
  
 };
 
 // Register addresses
#define MCP23S17_IODIR   0x00    // I/O direction 1=input(por), 0=output
#define MCP23S17_IPOL    0x02    // I/O polarity  1=inverted, 0=straight-up(por)
#define MCP23S17_GPINTEN 0x04    // Interupt on change 1=enabled, 0=disabled(por)
#define MCP23S17_DEFVAL  0x06    // Default interupt comparator 1=one, 0=zero(por)
#define MCP23S17_INTCON  0x08    // Interupt control 1=compare to DEFVAL, 0=compare to previous value(por)
#define MCP23S17_IOCON   0x0a    // I/O control register (defined above) 0x00(por)
#define MCP23S17_GPPU    0x0c    // Pull up resistor 1=enable, 0=disable(por)
#define MCP23S17_INTF    0x0e    // Interupt flag 1=pin caused interupt, 0=int not pending
#define MCP23S17_INTCAP  0x10    // Interupt capture value (at time of interupt)
#define MCP23S17_GPIO    0x12    // Port register - read only - value of port
#define MCP23S17_OLAT    0x14    // Output latch - modifies output latch for output pins
#endif