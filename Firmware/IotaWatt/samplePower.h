#ifndef samplePower_h
#define samplePower_h

void    samplePower(int channel, int overSample);
int     sampleCycle(IotaInputChannel* Vchannel, IotaInputChannel* Ichannel, int cycles = 1);
float   getAref(int channel);
int     readADC(uint8_t channel);
float   sampleVoltage(uint8_t Vchan, float Vcal);
float   samplePhase(uint8_t Vchan, uint8_t Ichan, int Ishift = 100);
float   samplePhase(uint8_t Ichan, uint8_t Cchan, int shift, double *VPri, double *VSec);
void    printSamples();

#endif