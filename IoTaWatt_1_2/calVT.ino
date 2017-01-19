boolean calibrateVT(){

  int Vchan = calibrationVchan;
  int refChan = calibrationRefChan; 

  int16_t rawV;
  int16_t rawI;
  
  uint32_t sumVsq = 0.0;
  uint32_t sumIsq = 0.0;
  uint32_t sumVIsq = 0.0;
  
  float Aref = getAref();
  double _Irms;

  int16_t * VsamplePtr = Vsample;
  int16_t * IsamplePtr = Isample;
  
  if(!sampleCycle(Vchan, refChan, _Irms)) return false; 
  if(_Irms < 1) return false;
    
  for(int i=0; i<samples; i++)
  {   
    rawV = *VsamplePtr;
    rawI = *IsamplePtr;
    sumVsq += rawV * rawV;
    sumIsq += rawI * rawI;
    sumVIsq += (rawI + rawV) * (rawI + rawV);
    VsamplePtr++;
    IsamplePtr++;  
  }

  float _Vrms = sqrt((float)sumVsq / samples) * Aref / float(ADC_range);
  _Irms = sqrt((float)sumIsq / samples) * Aref / float(ADC_range);
  float _VIrms = sqrt((float)sumVIsq / samples) * Aref / float(ADC_range);

  if(_Irms < .5) return false;

  calibrationCal = _Irms / _Vrms;                    // This ratio * calibration cord resistance in Kohms = Vcal
  calibrationPhase = 180.0 - 57.29577952 * acos(((_Vrms *_Vrms) + (_Irms * _Irms) - (_VIrms * _VIrms)) / (2.0 * _Vrms * _Irms));

  return true;
}

