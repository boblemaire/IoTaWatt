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
	
class IoTaInputChannel {
  public:
    channelTypes _type;                       // voltage, power, etc.
    char*        _name;                       // External name
	char*		 _model;					  // VT or CT (or ?) model	
    uint8_t      _channel;                    // Internal identifying number
	uint8_t		 _ADCbits;					  // ADC resolution		
    uint8_t      _addr;                       // Highbyte ADC, Lowbyte port on ADC
    uint8_t      _aRef;                       // Reference voltage address (Highbyte ADC, Lowbyte port on ADC)
    uint16_t     _offset;                     // ADC bias 
    byte         _vchannel;                   // Voltage [input] channel associated with a power channel;
    float        _calibration;                // Calibration factor
    float        _phase;                      // Phase correction in degrees (+lead, - lag);
    bool         _reversed;                   // True if negative power in being made positive (reversed CT)
    bool         _signed;                     // True if channel should not be reversed when negative (net metered main)
    const double MS_PER_HOUR = 3600000UL;     // useful constant
    dataBuckets   dataBucket;
    

    IoTaInputChannel(uint8_t channel, uint8_t chanAddr, uint8_t chanAref, uint8_t ADCbits){
      _name = "";
	  _model = "";
      _channel = channel;
      _addr = chanAddr;
      _aRef = chanAref;
	  _ADCbits = ADCbits;
      _offset = 1 << (ADCbits-1);
      _vchannel = 0;
      _calibration = 0;
      _phase = 0;
      _reversed = false;
      _signed = false;
    }
	~IoTaInputChannel(){
		if(_name) delete _name;
		if(_model) delete _model;
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
	
	double getVoltage(){return dataBucket.volts;}	
	double getPower(){return dataBucket.watts;}
	double getAmps(){return dataBucket.amps;}
	
  private:
};
