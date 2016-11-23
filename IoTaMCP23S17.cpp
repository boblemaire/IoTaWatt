/****************************************************************

IoTa_MCP23S17.cpp

Simple method to write pins in MCP23S17

****************************************************************/
#include "IoTaMCP23S17.h"

void IoTa_MCP23S17::begin(uint8_t _csPin)
{
  csPin = _csPin;                 // Save declared CS pin number
  pinMode(csPin,OUTPUT);          // Initialize the CS pin
  digitalWrite(csPin,HIGH);
  writeRegs(MCP23S17_IODIR, 0);   // All pins output
  writeRegs(MCP23S17_OLAT, 0);    // All pins off
  return; 
}

void IoTa_MCP23S17::writePin(uint8_t pin, uint8_t level)
{
  bitWrite(OLAT, pin ^ 0x08, level);
  writeRegs(MCP23S17_OLAT, OLAT);
  return;
}

void IoTa_MCP23S17::writeRegs(uint8_t addr, uint16_t data)
{
  spiOUT[0] = 0x40;
  spiOUT[1] = addr;
  spiOUT[2] = highByte(data);
  spiOUT[3] = lowByte(data);
  digitalWrite(csPin,LOW);
  SPI.transferBytes(spiOUT,spiIN,4);
  digitalWrite(csPin,HIGH);
  return;
}