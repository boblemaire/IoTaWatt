#ifndef IotaInputChannel_h
#define IotaInputChannel_h

#include <Arduino.h>

enum channelTypes:byte {channelTypeUndefined=0,
                        channelTypeVoltage=1,
                        channelTypePower=2};
						
union dataBuckets {
	  struct {
        double value1;
        double value2;
        double accum1;
        double accum2;
		uint32 timeThen;
      };
      struct {
        double  volts;
        double  Hz;
        double  voltHrs;
        double  HzHrs;
      };
      struct {
        double  watts;
        double  amps;
        double  wattHrs;
        double  ampHrs;
      };	  
	  dataBuckets(){value1=0; value2=0; accum1=0; accum2=0; timeThen=millis();}
    };
	
class IotaInputChannel {
  public:
    channelTypes _type;                       // voltage, power, etc.
    String       _name;                       // External name
	  String		 _model;					  // VT or CT (or ?) model	
    uint8_t      _channel;                    // Internal identifying number
	  uint8_t		 _ADCbits;					  // ADC resolution		
    uint8_t      _addr;                       // Highbyte ADC, Lowbyte port on ADC
    uint8_t      _aRef;                       // Reference voltage address (Highbyte ADC, Lowbyte port on ADC)
    uint16_t     _offset;                     // ADC bias 
    byte         _vchannel;                   // Voltage [input] channel associated with a power channel;
	  float		 _burden;					  // Value of on-board burden resistor, zero if none	
    float        _calibration;                // Calibration factor
    float        _phase;                      // Phase correction in degrees (+lead, - lag);
	  bool		 _active;	
    bool         _reversed;                   // True if negative power in being made positive (reversed CT)
    bool         _signed;                     // True if channel should not be reversed when negative (net metered main)
    const double MS_PER_HOUR = 3600000UL;     // useful constant
    dataBuckets   dataBucket;
    

    IotaInputChannel(uint8_t channel){
    _name = "";
	  _model = "";
    _channel = channel;
    _addr = channel + channel / 8;
    _aRef = 8;
	  _ADCbits = 12;
    _offset = 1 << (_ADCbits-1);
    _vchannel = 0;
	  _burden = 0;
    _calibration = 0;
    _phase = 0;
	  _active = false;
    _reversed = false;
    _signed = false;
    }
	~IotaInputChannel(){
		
	}
	
	void reset(){
	  _name = "";
	  _model = "";
	  _vchannel = 0;
	  _burden = 0;
    _calibration = 0;
    _phase = 0;
	  _active = false;
    _reversed = false;
    _signed = false;
	}
	
    void ageBuckets(uint32_t timeNow) {
		double elapsedHrs = double((uint32_t)(timeNow - dataBucket.timeThen)) / MS_PER_HOUR;
		dataBucket.accum1 += dataBucket.value1 * elapsedHrs;
		dataBucket.accum2 += dataBucket.value2 * elapsedHrs;
		dataBucket.timeThen = timeNow;    
    }

	void setVoltage(float volts, float Hz){
		if(_type != channelTypeVoltage) return;
		setVoltage(volts);
		dataBucket.Hz = Hz;	
	}	
    void setVoltage(float volts){
		if(_type != channelTypeVoltage) return;
		ageBuckets(millis());
		dataBucket.volts = volts;
    }
	
	void setHz(float Hz){
		if(_type != channelTypeVoltage) return;
		dataBucket.Hz = Hz;
    }
	
	void setPower(float watts, float amps){
		if(_type != channelTypePower) return;
		ageBuckets(millis());
		dataBucket.watts = watts;
		dataBucket.amps = amps;
	}
	
	bool isActive(){return _active;}
	void active(bool _active_){_active = _active_;}
	
	double getVoltage(){return dataBucket.volts;}	
	double getPower(){return dataBucket.watts;}
	double getAmps(){return dataBucket.amps;}
	
  private:
};

#endif // IotaInputChannel_h
