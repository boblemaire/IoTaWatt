#ifndef IotaOutputChannel_h
#define IotaOutputChannel_h

#include <Arduino.h>

class IotaScript {
	typedef uint8_t IStoken;
	
	
	public:
		IotaScript (){
			_tokens = nullptr;
      _constants = nullptr;
		}
		~IotaScript(){
			delete[] _tokens;
			if(_constants) delete[] _constants;
		}
		
		bool encodeScript(String script);
		double runScript(double function(int));
		void printScript();
	
	private:
		const byte getInputOp = 32;
		const byte getConstOp = 64;
		enum  opCodes	   {
							opEq	  = 0,
							opAdd 	= 1,
							opSub 	= 2,
							opMult 	= 3,
							opDiv 	= 4,
							opAbs 	= 5,
							opPush 	= 6,
							opPop 	= 7};
		
    // opcode         01234567
    String opChars = "=+-*/|()";
    
		IStoken *_tokens = NULL;						// The script operators
		float *_constants = NULL;				// Numeric constants
		
		
		double recursiveRunScript(IStoken**, double function(int));
		double evaluate(double, byte, double);	
};

class IotaOutputChannel {
	
	
	public:
		IotaOutputChannel(const char* name, const char* units, String script){
			_name = new char[sizeof(name)+1];
			strcpy(_name, name);
			IS.encodeScript(script);
			_units = new char[sizeof(units)+1];
			strcpy(_units, units);
		}
		~IotaOutputChannel(){
			delete[] _name;
			delete[] _units;
		}
		
		char*     	_name;          // External name
		char*		    _units;					// Watts or Volts
		uint8_t     _channel;       // Internal identifying number
		IotaScript	IS;					  	// Incidence of script interpreter
			
		bool setScript(String);
		double runScript(double function(int));
		void printScript();
			
	private:	 
		
		
};

#endif // IotaOutputChannel_h			
