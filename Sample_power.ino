 /***************************************************************************************************
  *  samplePower()
  *  
  *  This is the heart of the program.  We sample one wave from each channel and compute
  *  Active Power (Watts)
  *  Vrmv
  *  Irms
  *  Apparent Power (VA)
  *  
  *  The approach is to start sampling voltage/current pairs in a tight loop.
  *  When voltage crosses zero, we start recording the pairs.
  *  When we have crossed zero cycles*2 more times, we grab 100 more then stop and compute the results.
  *  
  *  Note:  If ever there was a time for low-level hardware manipulation, this is it.
  *  the tighter and faster the samples can be taken, the more accurate the results can be.
  *  The ESP8266 has pretty good SPI functions, but even using them optimally, it's only possible
  *  to achieve about 350 sample pairs per cycle.
  *  
  *  By manipulating the SPI chip select pin through hardware registers and
  *  running the SPI for only the required bits, again using the hardware 
  *  registers, it's possinble to get about 500 sample pairs per cycle running
  *  the SPI at 2MHz, which is the spec for the MCP3208 at 5v.
  *  
  *  The code supports both the MCP3008(10 bit) and MCP3208(12 bit) ADCs.
  *  They have identical packaging and pinout so they are interchangeable 
  *  on the board.  The SPI transactions are identical as well except that 
  *  the 3208 returns two more bits requiring two extra SPI cycles (1us).
  *  
  *  The 12 bit ADCs cost about a dollar more, so why would anybody use the 10 bit anyway?
  *  
  *  I've tried to segregate the bit-banging and document it well.
  *  For anyone interested in the low level registers, you can find 
  *  theem defined in esp8266_peri.h.
  * 
  ****************************************************************************************************/
 
 void sample_power()
{
  const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));

  const int pseudoShift = 0;                              // used to simulate large phase shift (low real power factors)   
  #define cycles 1
  int16_t crossLimit = cycles * 2 + 1;
  int16_t crossCount;
  int16_t crossGuard;

  byte      ADC_VselectPin;
  uint32_t  ADC_VselectMask;
  byte      ADC_IselectPin;
  uint32_t  ADC_IselectMask;
    
  int16_t channel;
  byte    Ichan;
  byte    Vchan;
  int16_t offsetV;
  int16_t offsetI;
  
  int16_t rawV;
  int16_t rawI;
  int16_t lastV;
  
  int32_t sumV;
  int32_t sumI;
  int32_t sumP;
  
  int32_t sumVsq;
  int32_t sumIsq;
  
  float Iratio;
  float Vratio;
  float Irms;
  float Aref;

  int16_t * VsamplePtr;
  int16_t * IsamplePtr;

  // Loop through all of the channels
   
  for(channel=1; channel<=channels; channel++)
  {
    if(channelType[channel] != channelTypePower) continue;
    Ichan = channel;
    Vchan = Vchannel[Ichan];

    // Determine if the channel is active.
    // ADC channel will be grounded (=0) if no CT plugged in.
    // Unplugged channels are inactive if there was no activity on them during this posting period (watt_ms == 0).  
    
    rawI = readADC(Ichan);
    if(rawI == 0)
    {
      if(watt_ms[channel] == 0) channelActive[channel] = false;
      activePower[channel] = 0;
      continue;
    }
    channelActive[channel] = true;

//****************************************** Sample the cycles *********************************

    SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));

    ADC_IselectPin = ADC_selectPin[Ichan >> 3];
    ADC_VselectPin = ADC_selectPin[Vchan >> 3];
    ADC_IselectMask = 1 << ADC_IselectPin;
    ADC_VselectMask = 1 << ADC_VselectPin;
    
    offsetV = offset[Vchan];
    offsetI = offset[Ichan];
        
    samples = 0;
    crossCount = 0;
    crossGuard = 0;
    VsamplePtr = Vsample;
    IsamplePtr = Isample;
    rawV = readADC(Vchan) - offsetV;                    // prime the pump
                           
    while(true)                                        // cross cycles (x2) + 1 times
    {
      lastV = rawV;

      // Hard coded equivilent of:
      // Isample[samples] = readADC(Ichan) - offsetI;
      
          GPOC = ADC_IselectMask;                                           // digitalWrite(ADC_IselectPin, LOW); Select the ADC
    
          // Hard coded equivilent of:
          // Isample[samples] = readADC(Ichan) - offsetI;
          
          SPI1U1 = ((SPI1U1 & mask) | ((4 << SPILMOSI) | (4 << SPILMISO)));
          SPI1W0 = (0x18 | Ichan) << 3;
          SPI1CMD |= SPIBUSY;
          while(SPI1CMD & SPIBUSY) {}
          delayMicroseconds(1);
    
          // Sample and Hold then...
          // Read the results
                                    
          SPI1U1 = ((SPI1U1 & mask) | (((ADC_bits + 1) << SPILMOSI) | ((ADC_bits + 1) << SPILMISO)));
          SPI1W0 = 0xFF;
          SPI1CMD |= SPIBUSY;
          while(SPI1CMD & SPIBUSY) {}
                    
          GPOS = ADC_IselectMask;                                          // digitalWrite(ADC_IselectPin, HIGH);  Deselect the ADC
          volatile uint8_t * fifoPtr8 = (volatile uint8_t *) &SPI1W0;
          *IsamplePtr = (word(*fifoPtr8, *(fifoPtr8+1)) >> (14 - ADC_bits)) - offsetI;

      // Hard coded equivilent of:
      // rawV = readADC(Vchan) - offsetV;
      // Vsample[samples] = (lastV + rawV) / 2;
                                     
          GPOC = ADC_VselectMask;                                            // digitalWrite(ADC_VselectPin, LOW); Select the ADC
          SPI1U1 = ((SPI1U1 & mask) | ((4 << SPILMOSI) | (4 << SPILMISO)));  // Set number of bits (n-1) so this is 5 
          SPI1W0 = (0x18 | Vchan) << 3;                                      // Data left aligned in low byte 
          SPI1CMD |= SPIBUSY;                                                // Start the SPI clock  
          while(SPI1CMD & SPIBUSY) {}                                        // Loop till SPI completes  
          delayMicroseconds(1);                                              // Give S&H cap more time to charge
          
          // Sample and Hold then...
          // Start reading the results
              
          SPI1U1 = ((SPI1U1 & mask) | (((ADC_bits + 1) << SPILMOSI) | ((ADC_bits + 1) << SPILMISO))); // Set number of bits (n-1)
          SPI1W0 = 0xFFFF;                                                    // Pad buffer with ones (don't know why)
          SPI1CMD |= SPIBUSY;                                                 // Start the SPI clock
          while(SPI1CMD & SPIBUSY) {}                                         // Loop till SPI completes
          GPOS = ADC_VselectMask;                                             // digitalWrite(ADC_VselectPin, HIGH); Deselect the ADC
          fifoPtr8 = (volatile uint8_t *) &SPI1W0;                            // Use a pointer to bit twiddle the result
          rawV = (word(*fifoPtr8, *(fifoPtr8+1)) >> (14 - ADC_bits)) - offsetV;  // Result is in left aligned in first FiFo word
          *VsamplePtr = (lastV + rawV) / 2;                                   // Average before and after to align with I in time


      
      if(((rawV ^ lastV) & crossGuard) >> 15)           // If crossed unambiguously (one but not both Vs negative and crossGuard negative 
      {
        crossCount++;                                   // Count this crossing
        crossGuard = 100;                               // No two crosses in 100 samples
      }
      if(crossCount)                                    // If past first crossing
      {
        VsamplePtr++;                                   // Accumulate samples
        IsamplePtr++;
        if(crossCount < crossLimit) samples++;          // Only count samples between crossings
        else if (crossGuard < 0) break;                 // Overun sampling to accomodate phase shift correction
        
      }
      crossGuard--;                                     // Count down crossing Guard
    }

    Aref = getAref();

//***************************************** Process the samples ********************************

    sumV = 0;
    sumI = 0;
    sumVsq = 0;
    sumIsq = 0;
    sumP = 0;
    
    samplesPerCycle = samples / cycles;
    float _phaseCorrection = (phaseCorrection[Vchan] - phaseCorrection[Ichan]) * samplesPerCycle / 360.0;
    int stepCorrection = int(_phaseCorrection);
    float stepFraction = _phaseCorrection - stepCorrection;
    
    VsamplePtr = Vsample + pseudoShift;
    IsamplePtr = Isample;
    
    if(_phaseCorrection >= 0) IsamplePtr += stepCorrection;
    else                      VsamplePtr -= stepCorrection;

    for(int i=0; i<samples; i++)
    {   
      rawV = *VsamplePtr;
      rawI = *IsamplePtr;
      if(stepFraction > 0) rawI += int(stepFraction * (*(IsamplePtr + 1) - *IsamplePtr));
      if(stepFraction < 0) rawV += int(-stepFraction * (*(VsamplePtr + 1) - *VsamplePtr));
      sumV += rawV;
      sumVsq += rawV * rawV;
      sumI += rawI;
      sumIsq += rawI * rawI;
      sumP += rawV * rawI;      
      VsamplePtr++;
      IsamplePtr++;  
    }

//***************************************** Compute results **************************************    
    
    Vratio = calibration[Vchan] * Aref / float(ADC_range);
    Vrms = Vratio * sqrt((float)sumVsq / samples);  
    Iratio = calibration[Ichan] * Aref / float(ADC_range);
    Irms = Iratio * sqrt((float)  sumIsq / samples);
    activePower[channel] = (float) Vratio * Iratio * sumP / samples;    // Watts
    apparentPower[channel] = Vrms * Irms;                                     // VA
    

//***************************************** Touch up the offset values **************************

    offsetV += sumV / samples;
    if(offsetV < minOffset) offsetV = minOffset;
    if(offsetV > maxOffset) offsetV = maxOffset;
    
    offsetI += sumI / samples;
    if(offsetI < minOffset) offsetI = minOffset;
    if(offsetI > maxOffset) offsetI = maxOffset;
    offset[Ichan] = offsetI;
   
    yield();                                      // Yield to the higher power
  }
 
  return;
}



//**********************************************************************************************
//
//        readADC(uint8_t channel)
//
//**********************************************************************************************

int readADC(uint8_t channel)
{ 
  uint32_t align = 0;               // SPI requires out and in to be word aligned                                                                 
  uint8_t ADC_out [4] = {0, 0, 0, 0};
  uint8_t ADC_in  [4] = {0, 0, 0, 0};  
  uint8_t ADCselectPin;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));  // SD may have changed this
  ADCselectPin = pin_CS_ADC0;
  if(channel > 7) ADCselectPin = pin_CS_ADC1;
    
  ADC_out[0] = 0x18 | (channel & 0x07);            

  digitalWrite(ADCselectPin, LOW);                  // Lower the chip select
  SPI.transferBytes(ADC_out, ADC_in, 1);            // Start bit, single bit, ADC port
                                                    // At this point ADC is sampling the port
                                                    // by charging the S&H capacitor. The sample 
                                                    // period ends when we initiate the next read
  SPI.transferBytes(ADC_out, ADC_in, 2);            // Start reading the results
  digitalWrite(ADCselectPin, HIGH);                 // Raise the chip select to deselect and reset
  return (word(ADC_in[0] & 0x3F, ADC_in[1]) >> (14 - ADC_bits));  // Put the result together and return
}

//**********************************************************************************************
//
//        getAref()  -  Get the current value of Aref
//
//**********************************************************************************************

float getAref(void) {return (float) VrefVolts * ADC_range / readADC(VrefChan);}

