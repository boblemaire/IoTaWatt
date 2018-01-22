#ifndef samplePower_h
#define samplePower_h

void    samplePower(int channel, int overSample);
int     sampleCycle(IotaInputChannel* Vchannel, IotaInputChannel* Ichannel, int cycles, int overSamples);
float   getAref(int channel);
int     readADC(uint8_t channel);
float   sampleVoltage(uint8_t Vchan, float Vcal);
String  samplePhase(uint8_t Vchan, uint8_t Ichan, uint16_t Ishift);
void    printSamples();

#endif