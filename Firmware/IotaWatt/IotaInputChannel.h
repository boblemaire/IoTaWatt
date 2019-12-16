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
    float        _vmult;                      // Voltage multiplier (overides _double)
    float        _lastPhase; 
    int16_t*     _p50;                        // -> 50Hz phase correction array
    int16_t*     _p60;                        // -> 60Hz phase correction array
    uint16_t     _turns;                      // Turns ratio of current type CT	
    uint16_t     _offset;                     // ADC bias 
    uint8_t      _channel;                    // Internal identifying number
    uint8_t      _addr;                       // 0x08-bit ADC, 0x07 bits port on ADC
    uint8_t      _aRef;                       // Reference voltage address (_addr format)
    byte         _vchannel;                   // Voltage [input] channel associated with a power channel;
    channelTypes _type;                       // voltage, power, etc.
	  bool		     _active;	
    bool         _reversed;                   // True if negative power in being made positive (reversed CT)
    bool         _signed;                     // True if channel should not be reversed when negative (net metered main)
    bool         _reverse;                    // Reverse ADC input from CT (emulate physical reverse)
    bool         _double;                     // Double power (120/240V single CT)
    
    IotaInputChannel(uint8_t channel)
    :_name(nullptr)
	  ,_model(nullptr)
    ,_burden(24)
    ,_calibration(0)
    ,_phase(0)
    ,_vphase(0)
    ,_vmult(0)
    ,_p50(nullptr)
    ,_p60(nullptr)
    ,_turns(0)
    ,_offset(2048)
    ,_channel(channel)
    ,_addr(channel + channel / 8)
    ,_aRef(8)
    ,_vchannel(0)
	  ,_active(false)
    ,_reversed(false)
    ,_signed(false)
    ,_reverse(false)
    ,_double(false)
    {}

	  ~IotaInputChannel(){}

    void    reset();
    void    ageBuckets(uint32_t timeNow);
    void    setVoltage(float volts, float Hz);
    void    setVoltage(float volts);
    void    setHz(float Hz);
    void    setPower(float watts, float VA);	
    bool    isActive(){return _active;}
    void    active(bool _active_){_active = _active_;}
    double  getVoltage(){return dataBucket.volts;}	
    double  getPower(){return dataBucket.watts;}
    double  getPf(){return dataBucket.watts / dataBucket.VA;}
    float   getPhase(float var);
    float   lookupPhase(int16_t* pArray, float var);
	
  private:
};

#endif
