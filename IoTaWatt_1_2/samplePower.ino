 /***************************************************************************************************
  *  samplePower()
  *  
  *  Sample all of the channels.
  *  
  *  Test for active and skip inactive channels.
  *  For each channel:
  *  
  *   Record samples of voltage and current for one cycle.  - sampleCycle()
  *   Process the cycles to compute power
  *   yield and call the web server until the next crossing is due.  *
  *  
  **************************************************************************************************/
void samplePower(int channel){
     
  if(channelType[channel] != channelTypePower) return;

//******************************************* Collect the samples *******************************************
  byte Vchan = Vchannel[channel];
  byte Ichan = channel;

  uint32_t timeNow = millis();
  
  ageBucket(&buckets[Ichan], timeNow);  
  
  double _Irms = 0;
  double _hz = 60;
  double _watts = 0;
  double _Vrms = 0;

  if(sampleCycle(Vchan, Ichan, _Irms)) {                 // collect the I and V samples
       
//******************************************* Process the samples ********************************************
    float Aref = getAref();    
    int32_t sumV = 0;
    int32_t sumI = 0;
    int32_t sumP = 0;
    int32_t sumVsq = 0;
    int32_t sumIsq = 0;
          
    int16_t* VsamplePtr = Vsample;
    int16_t* IsamplePtr = Isample;
  
    float _phaseCorrection = (phaseCorrection[Vchan] - phaseCorrection[Ichan]) * samplesPerCycle / 360.0;
    int stepCorrection = int(_phaseCorrection);
    float stepFraction = _phaseCorrection - stepCorrection;
    
    if(_phaseCorrection >= 0) IsamplePtr += stepCorrection;
    else                      VsamplePtr -= stepCorrection;
    
    int16_t rawV;
    int16_t rawI;
    
    for(int i=0; i<samples; i++){  
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
    
    //***************************************** Touch up the offset values **************************
  
    int16_t offsetV = offset[Vchan] + sumV / samples;
    if(offsetV < minOffset) offsetV = minOffset;
    if(offsetV > maxOffset) offsetV = maxOffset;
    offset[Vchan] = offsetV;
    
    int16_t offsetI = offset[Ichan] + sumI / samples;
    if(offsetI < minOffset) offsetI = minOffset;
    if(offsetI > maxOffset) offsetI = maxOffset;
    offset[Ichan] = offsetI;
      
    //***************************************** Compute results **************************************    
      
    double Vratio = calibration[Vchan] * Aref / double(ADC_range);
    double Iratio = calibration[Ichan] * Aref / double(ADC_range);
    _Vrms = Vratio * sqrt((double)(sumVsq / samples));
    // _Irms = Iratio * sqrt((double)(sumIsq / samples));
    _watts = Vratio * Iratio * (double)(sumP / samples);
    if(_watts < 0) _watts = -_watts;
    
    ageBucket(&buckets[Vchan], timeNow);
    buckets[Vchan].volts = _Vrms;
    buckets[Vchan].hz = _hz;
    if(_watts > 60){
      buckets[Ichan].pf = _watts / (_Irms * _Vrms);
    }
  } 
    
  buckets[Ichan].watts = _watts;
  buckets[Ichan].amps = _Irms;
}

/**********************************************************************************************
 * 
  *  sampleCycle(Vchan, Ichan)
  *  
  *  This is the heart of the program.  We sample a cycle of Voltage and Current and compute
  *  Active Power (Watts)
  *  Vrms
  *  Irms
  *  
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
  *  them defined in esp8266_peri.h.
  * 
  ****************************************************************************************************/
  
boolean sampleCycle(int Vchan, int Ichan, double &Irms) {
  
  
  #define cycles 1                            // Cycles to sample (>1 on your own)
  #define phaseMax 10                         // Maximum degrees of phase shift correction to allow for

  const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
  
  byte      ADC_VselectPin;
  uint32_t  ADC_VselectMask;
  byte      ADC_IselectPin;
  uint32_t  ADC_IselectMask;
    
  int16_t offsetV = offset[Vchan];
  int16_t offsetI = offset[Ichan];
  
  int16_t rawV;
  int16_t rawI;
  int16_t lastV;
    
  int32_t sumIsq = 0;
  samples = 0;
  
  int16_t * VsamplePtr = Vsample;
  int16_t * IsamplePtr = Isample;
    
  int16_t crossLimit = cycles * 2 + 1;
  int16_t crossCount = 0;
  int16_t crossGuard = 0;
  int16_t crossGuardReset = 2 + phaseMax * samplesPerCycle / 360;

  uint32_t startMs = millis();
  uint32_t timeoutMs = 1 + ((cycles * 1000) + 500) / frequency;
  uint32_t firstCrossUs;
  uint32_t lastCrossUs;

    SPI.beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));

    ADC_IselectPin = ADC_selectPin[Ichan >> 3];
    ADC_VselectPin = ADC_selectPin[Vchan >> 3];
    ADC_IselectMask = 1 << ADC_IselectPin;
    ADC_VselectMask = 1 << ADC_VselectPin;
   
    if(readADC(Ichan) < 4) return false;                // channel is unplugged (grounded)

    rawV = readADC(Vchan) - offsetV;
    
    samples = 0;
    os_intr_lock();                                     // disable interrupts
                    
    do {                                     
     
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
          
          // Do some loop housekeeping asynchronously while SPI runs.
          
          *VsamplePtr = (lastV + rawV) >> 1;                // Average before and after to align with I in time
          lastV = rawV;
          
          if(crossCount) {                                  // If past first crossing 
            if(crossCount < crossLimit){
              samples++;                                    // Only count samples between crossings
              sumIsq += *IsamplePtr * *IsamplePtr;          // Need Irms early to correct phase shift     
              if(samples >= maxSamples){                    // AC must be shut down
                trace(22);
                GPOS = ADC_IselectMask;                     // shortcut out
                os_intr_unlock();
                return false;
              }
            }
            VsamplePtr++;                                   // Accumulate samples
            IsamplePtr++;
          }
          crossGuard--;                              
          
          // Now wait for SPI to complete
          
          while(SPI1CMD & SPIBUSY) {}                       
                    
          GPOS = ADC_IselectMask;                                          // digitalWrite(ADC_IselectPin, HIGH);  Deselect the ADC
          volatile uint8_t * fifoPtr8 = (volatile uint8_t *) &SPI1W0;
          *IsamplePtr = (word((*fifoPtr8 & 0x3f), *(fifoPtr8+1)) >> (14 - ADC_bits)) - offsetI;

      // Hard coded equivilent of:
      // rawV = readADC(Vchan) - offsetV;
      // Vsample[samples] = (lastV + rawV) / 2;
                                     
          GPOC = ADC_VselectMask;                                            // digitalWrite(ADC_VselectPin, LOW); Select the ADC
          SPI1U1 = ((SPI1U1 & mask) | ((4 << SPILMOSI) | (4 << SPILMISO)));  // Set number of bits (n-1) so this is 5 
          SPI1W0 = (0x18 | Vchan) << 3;                                      // Data left aligned in low byte 
          SPI1CMD |= SPIBUSY;                                                // Start the SPI clock  
          while(SPI1CMD & SPIBUSY) {}                                        // Loop till SPI completes  
          delayMicroseconds(1);
                                                   
          // Sample and Hold then...
          // Start reading the results
              
          SPI1U1 = ((SPI1U1 & mask) | (((ADC_bits + 1) << SPILMOSI) | ((ADC_bits + 1) << SPILMISO))); // Set number of bits (n-1)
          SPI1W0 = 0xFFFF;                                                    // Pad buffer with ones (don't know why)
          SPI1CMD |= SPIBUSY;                                                 // Start the SPI clock

          // Do some loop housekeeping asynchronously while SPI runs.

          if((uint32_t)(millis()-startMs)>timeoutMs){                         // Something is very wrong
            trace(23);
            GPOS = ADC_VselectMask;                                           // shortcut back
            os_intr_unlock();
            return false;
          }
          
          if((*IsamplePtr > -3) && (*IsamplePtr < 3)) *IsamplePtr = 0;        // Filter noise from previous reading while SPI reads ADC
          
          // Now wait for SPI to complete
          
          while(SPI1CMD & SPIBUSY) {}                                         // Loop till SPI completes
          GPOS = ADC_VselectMask;                                             // digitalWrite(ADC_VselectPin, HIGH); Deselect the ADC
          fifoPtr8 = (volatile uint8_t *) &SPI1W0;                            // Use a pointer to bit twiddle the result
          rawV = (word((*fifoPtr8 & 0x3f), *(fifoPtr8+1)) >> (14 - ADC_bits)) - offsetV;  // Result is in left aligned in first FiFo word

          // Finish up loop cycle by checking for zero crossing.

          if(((rawV ^ lastV) & crossGuard) >> 15) {        // If crossed unambiguously (one but not both Vs negative and crossGuard negative 
            crossCount++;                                  // Count this crossing
            crossGuard = crossGuardReset;                  // No more crosses for awhile
            if(crossCount == 1){
              trace(20);
              firstCrossUs = micros();
            }
            else if(crossCount == crossLimit) {
              trace(21);
              lastCrossUs = micros();
              lastCrossMs = millis();
            }                               
          }
     
    } while(crossCount < crossLimit  || crossGuard > 0);                      // Keep going until prescribed crossings + phase shift overun
    
    *VsamplePtr = (lastV + rawV) / 2;                                         // Loose end
    trace(25);
    ESP.wdtFeed();
    trace(26);
    os_intr_unlock();                                                         // OK to interrupt now   
    trace(27);
    Aref = getAref();

    // sumIsq was accumulated in loop at no cost during SPI transfers.
    // Use it now to develop Irms in case caller needs it to get phase shift
    // as a function of current prior to post processing.

    double Iratio = calibration[Ichan] * Aref / float(ADC_range);
    Irms = Iratio * sqrt((float)  sumIsq / samples);
    frequency = (1000000.0 * cycles)  / float((uint32_t)(lastCrossUs - firstCrossUs));

    // Note the sample rate.
    // This is just a snapshot from single cycle sampling.
    // It can be a little off per cycle, but by damping the 
    // saved value we can get a pretty accurate average.

    samplesPerCycle = samplesPerCycle * .75 + (samples / cycles) * .25;
    cycleSamples++;
    
    return true;
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

//**********************************************************************************************
//
//        printSamples()  -  print one cycle of the current samples.
//        Diagnostic tool, not used.
//
//**********************************************************************************************

void printSamples()
{
  Serial.println(samplesPerCycle, 0);
  for(int i=0; i<(samplesPerCycle + 20); i++)
  {
    Serial.print(Vsample[i]);
    Serial.print(", ");
    Serial.print(Isample[i]);
    Serial.println();
  }
  return;
}


