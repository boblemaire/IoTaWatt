#include <IotaWatt.h>

/**********************************************************************************************
  * 
  *  sampleCycle(Vchan, Ichan)
  *  
  *  This code accounts for up to 66% (60Hz) of the execution of IotaWatt.
  *  Voltage and current sample pairs are collected and some preliminary
  *  metrics are produced.
  *     
  *  The approach is to start sampling voltage/current pairs in a tuned balanced loop
  *  that maximizes sample rate and minimizes sampling phase-shift.
  *  When voltage crosses zero, recording of the sample pairs is started.
  *  After two more zero crosses, a complete cycle is recorded and sampling stops.
  *  Preliminary results are developed before returning.
  * 
  *  The loop is controlled by the voltage signal which should be a good sine-wave with
  *  reasonable amplitude.  The crossguard counter insures that crossings are unambiguous.
  *  Voltage and Current signals are synchronized by averaging successive voltage samples,
  *  effectively producing a new signal via linear interpolation.  This technique relies
  *  on balancing of the time required to take the voltage and current ADC readings.
  * 
  *  There are various timeout safeguards in place to detect absence or loss of signal, as well
  *  as incomplete sample sets that can be caused by processor interrupts. 
  *  
  *  Note:  If ever there was a time for low-level hardware manipulation, this is it.
  *  The tighter and faster the samples can be taken, the more accurate the results will be.
  *  The ESP8266 has pretty good SPI functions, but even using them optimally, it's only possible
  *  to achieve about 350 sample pairs per cycle.
  *  
  *  By manipulating the SPI chip select pin through hardware registers and
  *  running the SPI for only the required bits, again using the hardware 
  *  registers, it's possible to get about 640 sample pairs per cycle (60Hz) running
  *  the SPI at 2MHz, which is the maximum data rate for the MCP3208.
  *  
  *  I've tried to segregate the bit-banging and document it well.
  *  For anyone interested in the low level registers, you can find 
  *  them defined in esp8266_peri.h.
  *
  *  Return codes are:
  *   0 - success
  *   1 - low quality sample (low sample rate, probably interrupted)
  *   2 - failure (probably no voltage reference or voltage unplugged during sampling)
  *   
  ****************************************************************************************************/

int sampleCycle(IotaInputChannel *Vchannel, IotaInputChannel *Ichannel, int cycles)
{

  int Vchan = Vchannel->_channel;
  int Ichan = Ichannel->_channel;

  uint32_t dataMask = ((ADC_BITS + 6) << SPILMOSI) | ((ADC_BITS + 6) << SPILMISO);
  const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
  volatile uint8_t * fifoPtr8 = (volatile uint8_t *) &SPI1W0;
  
  uint8_t  Iport = inputChannel[Ichan]->_addr % 8;       // Port on ADC
  uint8_t  Vport = inputChannel[Vchan]->_addr % 8;
    
  int16_t offsetV = Vchannel->_offset;        // Bias offset
  int16_t offsetI = Ichannel->_offset;
  
  int16_t rawV = 0;                               // Raw ADC readings
  int16_t lastV = 0;
  int16_t avgV;
  int16_t rawI = 0;
        
  int16_t * VsamplePtr = Vsample;             // -> to sample storage arrays
  int16_t * IsamplePtr = Isample;
    
  int16_t crossLimit = cycles * 2 + 1;        // number of crossings in total
  int16_t crossCount = 0;                     // number of crossings encountered
  int16_t crossGuard = 4;                     // Guard against faux crossings (must be >= 2 initially), more to detect no voltage

  uint32_t startMs = millis();                // Start of current half cycle
  uint32_t timeoutMs = 12;                    // Maximum time allowed per half cycle
  
  int16_t midCrossSamples;                    // Sample count at mid cycle and end of cycle
  int16_t lastCrossSamples;                   // Used to determine if sampling was interrupted

  byte ADC_IselectPin = ADC_selectPin[inputChannel[Ichan]->_addr >> 3];  // Chip select pin
  byte ADC_VselectPin = ADC_selectPin[inputChannel[Vchan]->_addr >> 3];
  uint32_t ADC_IselectMask = 1 << ADC_IselectPin;             // Mask for hardware chip select (pins 0-15)
  uint32_t ADC_VselectMask = 1 << ADC_VselectPin;

  bool Vsensed = false;                       // Voltage greater than 5 counts sensed.
  bool Vreverse = inputChannel[Vchan]->_reverse;
  bool Ireverse = inputChannel[Ichan]->_reverse;
  
  SPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));
 
  rawV = readADC(Vchan) - offsetV;                    // Prime the pump
  samples = 0;                                        // Start with nothing

          // Have at it.

  ESP.wdtFeed();                                     // Red meat for the silicon dog
  WDT_FEED();
  do{  
                      /************************************
                       * Sample the Current (I) channel   *
                       ************************************/
                                               
        GPOC = ADC_IselectMask;                            // digitalWrite(ADC_IselectPin, LOW); Select the ADC

              // hardware send 5 bit start + sgl/diff + port_addr
                                            
        SPI1U1 = (SPI1U1 & mask) | dataMask;               // Set number of bits 
        SPI1W0 = (0x18 | Iport) << 3;                      // Data left aligned in low byte 
        SPI1CMD |= SPIBUSY;                                // Start the SPI clock  

              // Do some loop housekeeping asynchronously while SPI runs.

          *IsamplePtr = rawI;
          *VsamplePtr = avgV = (rawV + lastV)  >> 1;
          lastV = rawV;
          if(crossCount) {                                // If past first crossing 
            VsamplePtr++;                                 // Accumulate samples
            IsamplePtr++;                                 
            samples++;                                    // Count samples
            if(samples >= MAX_SAMPLES){                   // If over the legal limit
              trace(T_SAMP,0);                            // shut down and return
              GPOS = ADC_IselectMask;                     // (Chip select high) 
              Serial.println(F("Max samples exceeded."));
              return 2;
            }
          }

              // Check for a significant voltage reading before first cross.
              // Will abort sample after initial crossGuard if not found.

          else {
            if(rawV < -10 || rawV > 10){
              Vsensed = true;
            }
          }
          crossGuard--;    
          
              // Now wait for SPI to complete
        
        while(SPI1CMD & SPIBUSY) {}                                         // Loop till SPI completes
        GPOS = ADC_IselectMask;                                             // digitalWrite(ADC_IselectPin, HIGH); Deselect the ADC 

              // extract the rawV from the SPI hardware buffer and adjust with offset. 
                                                                    
        rawI = (word(*fifoPtr8 & 0x01, *(fifoPtr8+1)) << 3) + (*(fifoPtr8+2) >> 5) - offsetI;

                      /************************************
                       *  Sample the Voltage (V) channel  *
                       ************************************/
         
        GPOC = ADC_VselectMask;                             // digitalWrite(ADC_VselectPin, LOW); Select the ADC
  
              // hardware send 5 bit start + sgl/diff + port_addr0
        
        SPI1U1 = (SPI1U1 & mask) | dataMask;
        SPI1W0 = (0x18 | Vport) << 3;
        SPI1CMD |= SPIBUSY;
        
              // Do some housekeeping asynchronously while SPI runs.
              
              // Check for timeout.  The clock gets reset at each crossing, so the
              // timeout value is a little more than a half cycle - 10ms @ 60Hz, 12ms @ 50Hz.
                      
          if((uint32_t)(millis()-startMs)>timeoutMs){                   // Something is wrong
            trace(T_SAMP,2,Ichan);                                      // Leave a meaningful trace
            trace(T_SAMP,2,Vchan);
            GPOS = ADC_VselectMask;                                     // ADC select pin high
            lastCrossUs = micros();                       
            return 2;                                                   // Return a failure
          }

              // Check that a significant voltage reading was found before first zero cross.
              // If not, abort cycle sample.

          else if(!crossGuard && !Vsensed){
            trace(T_SAMP,3,Ichan);                                      // Leave a meaningful trace
            trace(T_SAMP,3,Vchan);
            GPOS = ADC_VselectMask;                                     // ADC select pin high
            lastCrossUs = micros();                       
            return 2;                                                   // Return a failure
          }
          if(rawI >= -1 && rawI <= 1) rawI = 0;
                              
              // Now wait for SPI to complete
        
        while(SPI1CMD & SPIBUSY) {}                                 
        GPOS = ADC_VselectMask;                           // digitalWrite(ADC_VselectPin, HIGH);  Deselect the ADC                       

              // extract the rawI from the SPI hardware buffer and adjust with offset.
 
        rawV = (word(*fifoPtr8 & 0x01, *(fifoPtr8+1)) << 3) + (*(fifoPtr8+2) >> 5) - offsetV;
               
        // Finish up loop cycle by checking for zero crossing.
        // Crossing is defined by voltage changing signs  (Xor) and crossGuard negative.

        if(((rawV ^ lastV) & crossGuard) >> 15) {        // If crossed unambiguously (one but not both Vs negative and crossGuard negative 
          startMs = millis();                            // Reset the cycle clock 
          crossCount++;                                  // Count the crossings 
          if(crossCount == 1){
            trace(T_SAMP,4);
            firstCrossUs = micros();
            crossGuard = 10;                              // No more crosses for awhile  
          }
          else if(crossCount == crossLimit) {
            trace(T_SAMP,6);
            lastCrossUs = micros();                       // To compute frequency
            *VsamplePtr = (lastV + rawV) >> 1;                                       
            *IsamplePtr = rawI;                           // For main loop dispatcher to estimate when next crossing is imminent
            lastCrossSamples = samples;
            crossGuard = 0;                               // No more crosses for awhile
          }
          else if(crossCount == ((crossLimit + 1) / 2)){
            midCrossSamples = samples;
            crossGuard = 10;                               // No more crosses for awhile                               
          }
        }   
  } while(crossCount < crossLimit || crossGuard > 0); 

  trace(T_SAMP,8);

          // Process raw samples.
          // Add them to check the offset.
          // Reverse if required.

  VsamplePtr = Vsample;
  IsamplePtr = Isample;
  int32_t sumI = 0;
  int32_t sumV = 0;
  sumVsq = 0;
  sumIsq = 0;
  sumVI = 0;
  for(int i=0; i<samples; i++){
    if(*IsamplePtr == -1 || *IsamplePtr == 1){
      *IsamplePtr == 0;
    }
    sumV += *VsamplePtr;
    sumI += *IsamplePtr;
    if(Vreverse) *VsamplePtr = - *VsamplePtr;
    if(Ireverse) *IsamplePtr = - *IsamplePtr;
    sumVsq += *VsamplePtr * *VsamplePtr;
    sumIsq += *IsamplePtr * *IsamplePtr;
    sumVI  += *IsamplePtr * *VsamplePtr;
    VsamplePtr++;
    IsamplePtr++;
  }

    // A sample (V & I pair) should take 26.04us.
    // If we get 10 or more less than that, or less than 320,
    // reject the cycle.
  
  if(samples < MAX(320, (lastCrossUs - firstCrossUs) * 100 / 2604 - 10)){
    Serial.printf_P(PSTR("Low sample count %d\r\n"), samples);
    return 1;
  }

    // The sample count for each half of the cycle should be equal.
    // The zero crossings can be a sample or two off. 
    // Reject the cycle if the difference is more than 8.

  if(abs(samples - (midCrossSamples * 2)) > 8){
      // Serial.printf_P(PSTR("Sample imbalance %d %d\r\n"), midCrossSamples, samples - midCrossSamples);
      return 1;
  }

        // Adjust the offset values assuming symmetric waves but within limits otherwise.
 
//   const uint16_t minOffset = ADC_RANGE / 2 - ADC_RANGE / 200;    // Allow +/- .5% variation
//   const uint16_t maxOffset = ADC_RANGE / 2 + ADC_RANGE / 200;

//   trace(T_SAMP,9);

//   if(sumV >= 0) sumV += samples / 2; 
//   else sumV -= samples / 2;
//   offsetV = Vchannel->_offset + sumV / samples;
//   if(offsetV < minOffset) offsetV = minOffset;
//   if(offsetV > maxOffset) offsetV = maxOffset;
//   Vchannel->_offset = offsetV;
  
//   if(sumI >= 0) sumI += samples / 2;
//   else sumI -= samples / 2;
//   offsetI = Ichannel->_offset + sumI / samples;
//   if(offsetI < minOffset) offsetI = minOffset;
//   if(offsetI > maxOffset) offsetI = maxOffset;
//   Ichannel->_offset = offsetI; 

            // Update damped frequency.

  float Hz = 1000000.0  / float((uint32_t)(lastCrossUs - firstCrossUs));
  Vchannel->setHz(Hz);
  frequency = (0.9 * frequency) + (0.1 * Hz);

          // Note the sample rate.
          // This is just a snapshot from single cycle sampling.
          // It can be a little off per cycle, but by damping the 
          // saved value we can get a pretty accurate average.

  samplesPerCycle = samplesPerCycle * .9 + (samples / cycles) * .1;
  cycleSamples++;
  
  return 0;
}