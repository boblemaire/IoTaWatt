#ifndef IotaInputChannel_h
#define IotaInputChannel_h

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
        double  VA;
        double  wattHrs;
        double  VAHrs;
      };
      dataBuckets()
      :value1(0)
      ,value2(0)
      ,accum1(0)
      ,accum2(0)
      ,timeThen(millis()){}
      };
	
class IotaInputChannel {
  public:
    dataBuckets  dataBucket;
    char*        _name;                       // External name
	  char* 		   _model;					            // VT or CT (or ?) model
    float		     _burden;					            // Value of on-board burden resistor, zero if none	
    float        _calibration;                // Calibration factor
    float        _phase;                      // Phase correction in degrees (+lead, - lag);
    float        _vphase;                     // Phase offset for 3-phase voltage reference	
    uint16_t     _offset;                     // ADC bias 
    uint8_t      _channel;                    // Internal identifying number
    uint8_t      _addr;                       // Highbyte ADC, Lowbyte port on ADC
    uint8_t      _aRef;                       // Reference voltage address (Highbyte ADC, Lowbyte port on ADC)
    byte         _vchannel;                   // Voltage [input] channel associated with a power channel;
    channelTypes _type;                       // voltage, power, etc.
	  bool		     _active;	
    bool         _reversed;                   // True if negative power in being made positive (reversed CT)
    bool         _signed;                     // True if channel should not be reversed when negative (net metered main)
    
    IotaInputChannel(uint8_t channel)
    :_name(nullptr)
	  ,_model(nullptr)
    ,_channel(channel)
    ,_addr(channel + channel / 8)
    ,_aRef(8)
    ,_offset(2048)
    ,_vchannel(0)
	  ,_burden(0)
    ,_calibration(0)
    ,_phase(0)
    ,_vphase(0)
	  ,_active(false)
    ,_reversed(false)
    ,_signed(false)
   {}
	~IotaInputChannel(){
		
	}

	void reset(){
    delete[] _name;
	  _name = nullptr;
    delete[] _model;
	  _model = nullptr;
	  _vchannel = 0;
	  _burden = 0;
    _calibration = 0;
    _phase = 0;
	  _active = false;
    _reversed = false;
    _signed = false;
	}
	
    void ageBuckets(uint32_t timeNow) {
		double elapsedHrs = double((uint32_t)(timeNow - dataBucket.timeThen)) / 3600000E0;
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
	
	void setPower(float watts, float VA){
		if(_type != channelTypePower) return;
		ageBuckets(millis());
		dataBucket.watts = watts;
		dataBucket.VA = VA;
	}
	
	bool isActive(){return _active;}
	void active(bool _active_){_active = _active_;}
	
	double getVoltage(){return dataBucket.volts;}	
	double getPower(){return dataBucket.watts;}
	double getPf(){return dataBucket.watts / dataBucket.VA;}
	
  private:
};
#endif
