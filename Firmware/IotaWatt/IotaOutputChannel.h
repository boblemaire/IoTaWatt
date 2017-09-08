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

bool IotaOutputChannel::setScript(String script){
		IS.encodeScript(script);
		//IS.printScript();
}

double IotaOutputChannel::runScript(double inputCallback(int)){
		return IS.runScript(inputCallback);
}

bool IotaScript::encodeScript(String script){
    int tokenCount = 0;
    int constCount = 0;
    delete[] _tokens;
    delete[] _constants;
    for(int i=0; i<script.length(); i++){
      if(script[i] == '#')constCount++;
      if((!isDigit(script[i])) && (script[i] != '.')) tokenCount++;
    }
    _tokens = new IStoken[tokenCount + 1];
    _constants = new float[constCount];

    int j = 0;      
		for(int i=0; i<tokenCount; i++){
      if(script[j] == '@'){
        int s = ++j;
        while(isDigit(script[++j])); 
        _tokens[i] = getInputOp + script.substring(s,j).toInt();
      } 
      else if (script[j] == '#'){
        _tokens[i] = getConstOp + --constCount;
        int s = ++j;
        while(isDigit(script[++j]) || script[j] == '.');
        _constants[constCount] = script.substring(s,j).toFloat();
      }
      else {
        _tokens[i] = opChars.indexOf(script[j++]);
      }      
		}
    _tokens[tokenCount] = 0;
}

double IotaScript::runScript(double inputCallback(int)){
		IStoken* tokens = _tokens;
		return recursiveRunScript(&tokens, inputCallback);
}

double IotaScript::recursiveRunScript(IStoken** tokens, double inputCallback(int)){
		double result = 0.0;
		double operand = 0.0;
		IStoken pendingOp = opAdd;
		IStoken* token = *tokens;
		do {
			// Serial.print(result);
			// Serial.print(", ");
			// Serial.print(pendingOp);
			// Serial.print(", ");
			// Serial.println(operand);
			if(*token >= opAdd && *token <= opDiv){
				result = evaluate(result, pendingOp, operand);
				pendingOp = *token;
				operand = 0;
				if(*token == opDiv || *token == opMult) operand = 1;
			}
			else if(*token == opAbs){
				if(operand < 0) operand *= -1;
			}				
			else if(*token == opPush){
				token++;
				operand = recursiveRunScript(&token, inputCallback);
			}
			else if(*token == opPop){ 
				*tokens = token;
				return evaluate(result, pendingOp, operand);
			}
			else if(*token == opEq){
				return evaluate(result, pendingOp, operand);
			}
		
			if(*token & getInputOp){
				operand = inputCallback(*token % 32);
			}
			if(*token & getConstOp){
				operand = _constants[*token % 32];
			}
		} while(token++);
}

double IotaScript::evaluate(double result, IStoken token, double operand){
		// Serial.print("evaluate: ");
		// Serial.print(result);
		// Serial.print(", ");
		// Serial.print(token);
		// Serial.print(", ");
		// Serial.println(operand);
		switch (token) {
			case opAdd:
				return result + operand;
			case opSub:
				return result - operand;
			case opMult:
				return result * operand;
			case opDiv:
				return result / operand;		
		}
}

void IotaScript::printScript(){
		IStoken* token = _tokens;
		String msg = "";
		while(*token){
			if(*token < getInputOp){
				msg += String(opChars[*token]) + " ";
			}
			else if(*token & getInputOp){
				msg += "@" + String(*token - getInputOp)+ " ";
			}
			else if(*token & getConstOp){
				msg += String(_constants[*token - getConstOp]) + " ";
			}
			else {
				msg += "token(" + String(*token) + " ";
			}
			token++;
		}
		Serial.println(msg);
}

#endif // IotaOutputChannel_h			
