#include <IotaWatt.h>

void IotaInputChannel::reset(){
    delete[] _name;
	_name = nullptr;
    delete[] _model;
	_model = nullptr;
	_vchannel = 0;
	_burden = 0;
    _calibration = 0;
    _phase = 0;
    _p50 = nullptr;
    _p60 = nullptr;
	_active = false;
    _reversed = false;
    _signed = false; 
    _reverse = false;
    _double = false;
}

void IotaInputChannel::ageBuckets(uint32_t timeNow) {
    if(timeNow > dataBucket.timeThen){
        double elapsedHrs = double((uint32_t)(timeNow - dataBucket.timeThen)) / 3600000E0;
        dataBucket.accum1 += dataBucket.value1 * elapsedHrs;
        dataBucket.accum2 += dataBucket.value2 * elapsedHrs;
        dataBucket.timeThen = timeNow;
    }    
}

void IotaInputChannel::setVoltage(float volts, float Hz){
    if(_type != channelTypeVoltage) return;
    dataBucket.Hz = Hz;	
    setVoltage(volts);
}

void IotaInputChannel::setVoltage(float volts){
    if(_type != channelTypeVoltage) return;
    dataBucket.volts = volts;
    ageBuckets(millis());
}

void IotaInputChannel::setHz(float Hz){
    if(_type != channelTypeVoltage) return;
    dataBucket.Hz = Hz;
}

void IotaInputChannel::setPower(float watts, float VA){
    if(_type != channelTypePower) return;
    dataBucket.watts = watts;
    dataBucket.VA = VA;
    ageBuckets(millis());
}

float IotaInputChannel::getPhase(const float var){
    float frequency;
    float phase = _phase;                                   
    if(_type == channelTypeVoltage){
        frequency = dataBucket.Hz;
    } else {
        frequency = inputChannel[_vchannel]->dataBucket.Hz;
    }
    if(frequency < 55 && _p50){
        phase = lookupPhase(_p50, var);
    }
    if(frequency >= 55 && _p60){
        phase = lookupPhase(_p60, var);
    }

            // Pre 5.0 PCB had 10uF capacitor on channel 0
            // Add capacitive shift if so

    if(_channel == 0 && deviceMajorVersion < 5){
        phase += (frequency >= 55) ? 1.45 : 1.71;
    }
    return phase;
}

float IotaInputChannel::lookupPhase(int16_t* pArray, const float var){
    int16_t* array = pArray;
    int16_t intVar = var * 100;
    while(*(array+1) && *(array+1) <= intVar){
        array += 2;
    }
    return (float)*array / 100.0;
} 
