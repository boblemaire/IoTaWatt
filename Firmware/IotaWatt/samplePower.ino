  /***************************************************************************************************
  *  samplePower()  Sample a channel.
  *  
  ****************************************************************************************************/
void samplePower(int channel, int overSample){
  uint32_t timeNow = millis();
  
      // If it's a voltage channel, use voltage only sample, update and return.

  trace(T_POWER,0);
  if(inputChannel[channel]->_type == channelTypeVoltage){
    inputChannel[channel]->setVoltage(sampleVoltage(channel, inputChannel[channel]->_calibration));                                                                        
    return;
  }

         // Currently only voltage and power channels, so return if not one of those.
     
  if(inputChannel[channel]->_type != channelTypePower) return;

         // From here on, dealing with a power channel and associated voltage channel.

  trace(T_POWER,1);
  IotaInputChannel* Ichannel = inputChannel[channel];
  IotaInputChannel* Vchannel = inputChannel[Ichannel->_vchannel]; 
          
  byte Ichan = Ichannel->_channel;
  byte Vchan = Vchannel->_channel;
  
  double _Irms = 0;
  double _watts = 0;
  double _Vrms = 0;

  int16_t* VsamplePtr = Vsample;
  int16_t* IsamplePtr = Isample;
   
        // Invoke high speed sample collection.
        // If it fails, set power to zero and return.

  if( ! sampleCycle(Vchannel, Ichannel, 1, 0)) {
    trace(T_POWER,2);
    Ichannel->setPower(0.0, 0.0);
    return;
  }          
      
  int16_t rawV;
  int16_t rawI;  
  int32_t sumV = 0;
  int32_t sumI = 0;
  int32_t sumP = 0;
  int32_t sumVsq = 0;
  int32_t sumIsq = 0;  

      // Determine phase correction components.
      // stepCorrection is the number of I samples to add or subtract.
      // stepFraction is the interpolation to apply to the next I sample (0.0 - 1.0)
      // The phase correction is the net phase lead (+) of voltage computed as the 
      // (VT lead - CT lead) + any gross phase correction for 3 phase measurement.
      // Note that a reversed CT can be corrected by introducing a 180deg gross correction.

  float _phaseCorrection = (Vchannel->_phase - Ichannel->_phase) * samples / 360.0;  // fractional Isamples correction
  int stepCorrection = int(_phaseCorrection);                                                // whole steps to correct 
  float stepFraction = _phaseCorrection - stepCorrection;                                    // fractional step correction
  if(stepFraction < 0){                                                                      // if current lead
    stepCorrection--;                                                                        // One sample back
    stepFraction += 1.0;                                                                     // and forward 1-fraction
  }

  trace(T_POWER,3);
        // get sums, squares, and all that stuff.
  Isample[samples] = Isample[0];      
  int Iindex = (samples + stepCorrection) % samples;
  for(int i=0; i<samples; i++){  
    rawV = *VsamplePtr;
    rawI = Isample[Iindex]; 
    rawI += int(stepFraction * (Isample[Iindex + 1] - Isample[Iindex]));
    sumV += rawV;
    sumVsq += rawV * rawV;
    sumI += rawI;
    sumIsq += rawI * rawI;
    sumP += rawV * rawI;      
    VsamplePtr++;
    Iindex = (Iindex + 1) % samples;
  }
  
        // Adjust the offset values assuming symmetric waves but within limits otherwise.
 
  const uint16_t minOffset = ADC_RANGE / 2 - ADC_RANGE / 200;    // Allow +/- .5% variation
  const uint16_t maxOffset = ADC_RANGE / 2 + ADC_RANGE / 200;

  trace(T_POWER,4);
  if(sumV >= 0) sumV += samples / 2;
  else sumV -= samples / 2;
  int16_t offsetV = Vchannel->_offset + sumV / samples;
  if(offsetV < minOffset) offsetV = minOffset;
  if(offsetV > maxOffset) offsetV = maxOffset;
  Vchannel->_offset = offsetV;
  
  if(sumI >= 0) sumI += samples / 2;
  else sumI -= samples / 2;
  int16_t offsetI = Ichannel->_offset + sumI / samples;
  if(offsetI < minOffset) offsetI = minOffset;
  if(offsetI > maxOffset) offsetI = maxOffset;
  Ichannel->_offset = offsetI;
    
        // Voltage calibration is the ratio of line voltage to voltage presented at the input.
        // Input voltage is further attenuated with voltage dividing resistors (Vadj_3).
        // So ratio of voltage at ADC vs line is calibration * Vadj_3.
    
  double Vratio = Vchannel->_calibration * Vadj_3 * getAref(Vchan) / double(ADC_RANGE);

        // Iratio is straight Amps/ADC volt.
  
  double Iratio = Ichannel->_calibration * getAref(Ichan) / double(ADC_RANGE);

        // Now that the preliminaries are over, 
        // Getting Vrms, Irms, and Watts is easy.
  
  _Vrms = Vratio * sqrt((double)(sumVsq / samples));
  _Irms = Iratio * sqrt((double)(sumIsq / samples));
  _watts = Vratio * Iratio * (double)(sumP / samples);

        // If watts is negative and the channel is not explicitely signed, reverse it (backward CT).
        // If we do reverse it, and it's significant, mark it as such for reporting in the status API.

  if( ! Ichannel->_signed){
    Ichannel->_reversed = false;
    if(_watts < 0){
      _watts = -_watts;
      if(_watts > 0.5){
        Ichannel->_reversed = true;
      }
    }
  }

      // Update with the new power and voltage values.

  trace(T_POWER,5);
  Ichannel->setPower(_watts, _Irms);
  Vchannel->setVoltage(_Vrms);
  trace(T_POWER,9);                                                                               
  return;
}

  /**********************************************************************************************
  * 
  *  sampleCycle(Vchan, Ichan)
  *  
  *  This code accounts for up to 66% (60Hz) of the execution of IotaWatt.
  *  It collects voltage and current sample pairs and saves them away for 
  *    
  *  The approach is to start sampling voltage/current pairs in a tight loop.
  *  When voltage crosses zero, we start recording the pairs.
  *  When we have crossed zero cycles*2 more times we stop and return to compute the results.
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
  *  Although there is currently no way to configure the 3008, the support
  *  here and elsewhere is low profile enough that it was left in.  
  *  
  *  I've tried to segregate the bit-banging and document it well.
  *  For anyone interested in the low level registers, you can find 
  *  them defined in esp8266_peri.h.
  * 
  ****************************************************************************************************/
  
  boolean sampleCycle(IotaInputChannel* Vchannel, IotaInputChannel* Ichannel, int cycles, int overSamples){

  int Vchan = Vchannel->_channel;
  int Ichan = Ichannel->_channel;

  uint32_t dataMask = ((ADC_BITS + 6) << SPILMOSI) | ((ADC_BITS + 6) << SPILMISO);
  const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
  volatile uint8_t * fifoPtr8 = (volatile uint8_t *) &SPI1W0;
  
  uint8_t  Iport = inputChannel[Ichan]->_addr % 8;       // Port on ADC
  uint8_t  Vport = inputChannel[Vchan]->_addr % 8;
    
  int16_t offsetV = Vchannel->_offset;        // Bias offset
  int16_t offsetI = Ichannel->_offset;
  
  int16_t rawV;                               // Raw ADC readings
  int16_t lastV;
  int16_t rawI;
  int16_t lastI = 0;
        
  int16_t * VsamplePtr = Vsample;             // -> to sample storage arrays
  int16_t * IsamplePtr = Isample;
    
  int16_t crossLimit = cycles * 2 + 1;        // number of crossings in total
  int16_t crossCount = 0;                     // number of crossings encountered
  int16_t crossGuard = 3;                     // Guard against faux crossings (must be >= 2 initially)  

  uint32_t startMs = millis();                // Start of current half cycle
  uint32_t timeoutMs = 12;                    // Maximum time allowed per half cycle
  uint32_t firstCrossUs;                      // Time cycle at usec resolution for phase calculation
  uint32_t lastCrossUs;                       

  int16_t midCrossSamples;                    // Sample count at mid cycle and end of cycle
  int16_t lastCrossSamples;                   // Used to determine if sampling was interrupted

  byte ADC_IselectPin = ADC_selectPin[inputChannel[Ichan]->_addr >> 3];  // Chip select pin
  byte ADC_VselectPin = ADC_selectPin[inputChannel[Vchan]->_addr >> 3];
  uint32_t ADC_IselectMask = 1 << ADC_IselectPin;             // Mask for hardware chip select (pins 0-15)
  uint32_t ADC_VselectMask = 1 << ADC_VselectPin;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));

          // Make sure there is a voltage signal
 
  lastV = readADC(Vchan) - offsetV;
  do {
    if((millis() - startMs) > 2){
      return false;
    }
    rawV = readADC(Vchan) - offsetV;   
  } while(abs(rawV - lastV) < 20);
  
  *VsamplePtr = readADC(Vchan) - offsetV;             // Prime the pump
  samples = 0;                                        // Start with nothing

          // Have at it.

  ESP.wdtFeed();                                     // Red meat for the silicon dog
  WDT_FEED();     
  do{  
                      /************************************
                       *  Sample the Voltage (V) channel  *
                       ************************************/
                                               
        GPOC = ADC_VselectMask;                            // digitalWrite(ADC_VselectPin, LOW); Select the ADC

              // hardware send 5 bit start + sgl/diff + port_addr
                                            
        SPI1U1 = (SPI1U1 & mask) | dataMask;               // Set number of bits 
        SPI1W0 = (0x18 | Vport) << 3;                      // Data left aligned in low byte 
        SPI1CMD |= SPIBUSY;                                // Start the SPI clock  

              // Do some loop housekeeping asynchronously while SPI runs.
              
          *VsamplePtr = rawV;
          lastV = rawV;
          *IsamplePtr = (rawI + lastI) / 2;
          if(*IsamplePtr >= -1 && *IsamplePtr <= 1) *IsamplePtr = 0;
          lastI = rawI;
                 
          // if((*IsamplePtr > -3) && (*IsamplePtr < 3)) *IsamplePtr = 0;       // Filter noise from previous reading while SPI reads ADC  
              
          if(crossCount) {                                  // If past first crossing 
            VsamplePtr++;                                   // Accumulate samples
            IsamplePtr++; 
            if(crossCount < crossLimit){
              samples++;
              if(samples >= MAX_SAMPLES){                   // If over the legal limit
                trace(T_SAMP,0);                            // shut down and return
                GPOS = ADC_IselectMask;                     // (Chip select high) 
                PRINTL("Max samples exceeded.","")       
                return false;
              }
            }
          }
          crossGuard--;    
          lastV = rawV;
          
              // Now wait for SPI to complete
        
        while(SPI1CMD & SPIBUSY) {}                                         // Loop till SPI completes
        GPOS = ADC_VselectMask;                                             // digitalWrite(ADC_VselectPin, HIGH); Deselect the ADC 

              // extract the rawV from the SPI hardware buffer and adjust with offset. 
                                                                    
        rawV = (word(*fifoPtr8 & 0x01, *(fifoPtr8+1)) << 3) + (*(fifoPtr8+2) >> 5) - offsetV;
                                             
                      /************************************
                       *  Sample the Current (I) channel  *
                       ************************************/
         
        GPOC = ADC_IselectMask;                             // digitalWrite(ADC_IselectPin, LOW); Select the ADC
  
              // hardware send 5 bit start + sgl/diff + port_addr0
        
        SPI1U1 = (SPI1U1 & mask) | dataMask;
        SPI1W0 = (0x18 | Iport) << 3;
        SPI1CMD |= SPIBUSY;
        
              // Do some housekeeping asynchronously while SPI runs.
              
              // Check for timeout.  The clock gets reset at each crossing, so the
              // timeout value is a little more than a half cycle - 10ms @ 60Hz, 12ms @ 50Hz.
              // The most common cause of timeout here is unplugging the AC reference VT.  Since the
              // device is typically sampling 60% of the time, there is a high probability this
              // will happen if the adapter is unplugged.
              // So handling needs to be robust.
        
          if((uint32_t)(millis()-startMs)>timeoutMs){                       // Something is wrong
            trace(T_SAMP,2);                                                // Leave a meaningful trace
            trace(T_SAMP,Ichan);
            trace(T_SAMP,Vchan);
            GPOS = ADC_VselectMask;                                         // ADC select pin high 
            PRINTL("Timeout ", Ichan)                               
            return false;                                                   // Return a failure
          }
                              
              // Now wait for SPI to complete
        
        while(SPI1CMD & SPIBUSY) {}                                 
        GPOS = ADC_IselectMask;                           // digitalWrite(ADC_IselectPin, HIGH);  Deselect the ADC                       

              // extract the rawI from the SPI hardware buffer and adjust with offset.
 
        rawI = (word(*fifoPtr8 & 0x01, *(fifoPtr8+1)) << 3) + (*(fifoPtr8+2) >> 5) - offsetI;
   
       
        // Finish up loop cycle by checking for zero crossing.
        // Crossing is defined by I and V having different signs (Xor) and crossGuard negative.

        if(((rawV ^ lastV) & crossGuard) >> 15) {        // If crossed unambiguously (one but not both Vs negative and crossGuard negative 
          startMs = millis();                            // Reset the cycle clock 
          crossCount++;                                  // Count the crossings 
          crossGuard = 10;                               // No more crosses for awhile
          if(crossCount == 1){
            trace(T_SAMP,4);
            firstCrossUs = micros();
            samples++;   
            VsamplePtr++;                                 // Accumulate samples
            IsamplePtr++;  
          }
          else if(crossCount == crossLimit) {
            trace(T_SAMP,6);
            lastCrossUs = micros();                     // To compute frequency
            lastCrossMs = millis();                     // For main loop dispatcher to estimate when next crossing is imminent
            lastCrossSamples = samples;
            crossGuard = overSamples + 1;
          }
          else {
            midCrossSamples = samples;                               
          }
        }   
  } while(crossCount < crossLimit || crossGuard > 0); 

  *VsamplePtr = rawV;                                       
  *IsamplePtr = (rawI + lastI) >> 1;
   
  trace(T_SAMP,8);
  
          // Update damped frequency.

  frequency = (0.9 * frequency) + (0.1 * (1000000.0 * cycles)  / float((uint32_t)(lastCrossUs - firstCrossUs)));

          // Note the sample rate.
          // This is just a snapshot from single cycle sampling.
          // It can be a little off per cycle, but by damping the 
          // saved value we can get a pretty accurate average.

  samplesPerCycle = samplesPerCycle * .9 + (samples / cycles) * .1;
  cycleSamples++;
  
  return true;
}

//**********************************************************************************************
//
//        readADC(uint8_t channel)
//
//**********************************************************************************************

int readADC(uint8_t channel){ 
  uint32_t align = 0;               // SPI requires out and in to be word aligned                                                                 
  uint8_t ADC_out [4] = {0, 0, 0, 0};
  uint8_t ADC_in  [4] = {0, 0, 0, 0};  
  uint8_t ADCselectPin;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));  // SD may have changed this
  ADCselectPin = ADC_selectPin[inputChannel[channel]->_addr >> 3];    
  ADC_out[0] = 0x18 | (inputChannel[channel]->_addr & 0x07);
  digitalWrite(ADCselectPin, LOW);                  // Lower the chip select
  SPI.transferBytes(ADC_out, ADC_in, 3);            // Do business
  digitalWrite(ADCselectPin, HIGH);                 // Raise the chip select to deselect and reset
  return (word(ADC_in[1] & 0x3F, ADC_in[2]) >> (14 - ADC_BITS)); // Put the result together and return
}

/****************************************************************************************************
 * sampleVoltage() is used during calibration of a Voltage channel.
 * It samples one cycle and returns the voltage corresponding to the supplied calibration factor.
 ****************************************************************************************************/
float sampleVoltage(uint8_t Vchan, float Vcal){
  IotaInputChannel* _input = inputChannel[Vchan];
  uint32_t sumVsq = 0;
  int16_t rawV = 0;
  int16_t lastV = 0;
  int16_t offsetV = _input->_offset;
  int16_t* VsamplePtr = Vsample;
  uint32_t startMs = millis();
  uint32_t firstCrossUs;
  uint32_t timeoutMs = 600 / frequency;
  int16_t crossCount = 0;
  int16_t crossLimit = 3;
  int16_t crossGuard = 0;
  samples = 0;

  lastV = readADC(Vchan) - offsetV;
  do {
    if((millis() - startMs) > 2){
      return 0.0;
    } 
    rawV = readADC(Vchan) - offsetV;   
  } while(abs(rawV - lastV) < 20);
  
  while(crossCount < crossLimit){
    lastV = rawV;
    rawV = readADC(Vchan) - offsetV;
    *VsamplePtr = rawV;
    
    if(((rawV ^ lastV) & crossGuard) >> 15) {  
      crossCount++;
      crossGuard = 10;
      startMs = millis();
      if(crossCount == 1){
        firstCrossUs = micros();     
      }
    }
    crossGuard--;
    
    if(crossCount){
      VsamplePtr++;
      sumVsq += rawV * rawV;
      samples++;
    }     
    
    if((uint32_t)(millis()-startMs) > timeoutMs){
      return 0;
    } 
  }
  float Hz = 1000000.0  / float(micros() - firstCrossUs);
  _input->setHz(Hz);
  frequency = (0.8 * frequency) + (0.2 * Hz);
  samplesPerCycle = samplesPerCycle * .9 + samples * .1;
  cycleSamples++;
  double Vratio = Vcal * Vadj_3 * getAref(Vchan) / double(ADC_RANGE);
  if(getAref(Vchan) < 1.5) Vratio *= (double)Vadj_1 / Vadj_3;
  return  Vratio * sqrt((double)(sumVsq / samples));
}
//**********************************************************************************************
//
//        getAref()  -  Get the current value of Aref
//
//**********************************************************************************************

float getAref(int channel) {
   
  uint32_t align = 0;               // SPI requires out and in to be word aligned                                                                 
  uint8_t ADC_out [4] = {0, 0, 0, 0};
  uint8_t ADC_in  [4] = {0, 0, 0, 0};  
  uint8_t ADCselectPin;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));  // SD may have changed this
  ADCselectPin = ADC_selectPin[inputChannel[channel]->_aRef >> 3];    
  ADC_out[0] = 0x18 | (inputChannel[channel]->_aRef & 0x07);            

  digitalWrite(ADCselectPin, LOW);                  // Lower the chip select
  SPI.transferBytes(ADC_out, ADC_in, 3);            // Start reading the results
  digitalWrite(ADCselectPin, HIGH);                 // Raise the chip select to deselect and reset
                                                    // Put the result together and return
  uint16_t ADCvalue = (word(ADC_in[1] & 0x3F, ADC_in[2]) >> (14 - ADC_BITS));
  if(ADCvalue == 4095 | ADCvalue == 0) return 0;    // no ADC
  return VrefVolts * ADC_RANGE / ADCvalue;  
}

//**********************************************************************************************
//
//        printSamples()  -  print the current samples.
//        fileSamples() - write the current samples to an SD file ("samples.txt").
//        These are diagnostic tools, not currently used.
//
//**********************************************************************************************

void printSamples() {
  Serial.println(samples);
  for(int i=0; i<(samples + 2); i++)
  {
    Serial.print(i);
    Serial.print(", ");
    Serial.print(Vsample[i]);
    Serial.print(", ");
    Serial.print(Isample[i]);
    Serial.println();
  }
  return;
}

void fileSamples() {
  File sampleFile;
  SD.remove("samples.txt");
  sampleFile = SD.open("samples.txt",FILE_WRITE);
  sampleFile.println(samples);
  for(int i=0; i<(samples + 50); i++)
  {
    sampleFile.print(Vsample[i]);
    sampleFile.print(", ");
    sampleFile.print(Isample[i]);
    sampleFile.println();
  }
  sampleFile.close();
  return;
}

float samplePhase(uint8_t Vchan, uint8_t Ichan, uint16_t Ishift){
  
  int16_t cycles = 1;
    
  IotaInputChannel* Vchannel = inputChannel[Vchan]; 
  IotaInputChannel* Ichannel = inputChannel[Ichan];
  
  int16_t rawV;                               // Raw ADC readings
  int16_t rawI;
    
  double sumVsq = 0;
  double sumIsq = 0;
  double sumVI = 0;

  sampleCycle(Vchannel, Ichannel, cycles, Ishift);
  PRINTL("samples: ",samples)
  PRINTL("Vsample[0]: ", Vsample[0])
  PRINTL("Vsample[1]: ", Vsample[1])
  PRINTL("Vsample[n-1]: ", Vsample[samples-1])
  PRINTL("Vsample[n]: ", Vsample[samples])

  double IshiftDeg = (double)Ishift * 360.0 / (samples / cycles);
  PRINTL("ShiftDeg: ", IshiftDeg) 

  for(int i=0; i<samples; i++){
    sumVsq += Vsample[i] * Vsample[i];
    sumIsq += Isample[(i + Ishift) % samples] * Isample[(i + Ishift) % samples];
    sumVI += Vsample[i] * Isample[(i + Ishift) % samples];
  }

  double Vrms = sqrt(sumVsq / (double)samples);
  double Irms = sqrt(sumIsq / (double)samples);
  double VI = sumVI / (double)samples;

  float phaseDiff = (double)57.29578 * acos(VI / (Vrms * Irms)) - IshiftDeg;
  PRINTL("phaseDiff: ",phaseDiff)
  return phaseDiff;
}


