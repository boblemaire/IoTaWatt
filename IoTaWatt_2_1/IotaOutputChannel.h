class IotaScript {
	typedef uint8_t IStoken;
	
	
	public:
		IotaScript (){
			
		}
		~IotaScript(){
			delete[] _tokens;
			if(_constants) delete[] _constants;
		}
		
		bool encodeScript(JsonArray& script);
		double runScript(double function(int));
		void printScript();
	
	private:
		const byte getInputOp = 32;
		const byte getConstOp = 64;
		enum  opCodes	   {
							opEq	= 0,
							opAdd 	= 1,
							opSub 	= 2,
							opMult 	= 3,
							opDiv 	= 4,
							opAbs 	= 5,
							opPush 	= 6,
							opPop 	= 7};
		char opChars[8] = {'=','+','-','*','/','|','(',')'};
		IStoken *_tokens = NULL;						// The script operators
		float *_constants = NULL;				// Numeric constants
		
		
		double recursiveRunScript(IStoken**, double function(int));
		double evaluate(double, byte, double);	
};

class IotaOutputChannel {
	
	
	public:
		IotaOutputChannel(const char* name, const char* units, JsonArray& script){
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
		
		char*     	_name;                  // External name
		char*		_units;					// Watts or Volts
		uint8_t     _channel;               // Internal identifying number
		IotaScript	IS;						// Incidence of script interpreter
			
		bool setScript(JsonArray&);
		double runScript(double function(int));
		void printScript();
			
	private:	 
		
		
};

bool IotaOutputChannel::setScript(JsonArray& script){
		IS.encodeScript(script);
		//IS.printScript();
}

double IotaOutputChannel::runScript(double inputCallback(int)){
		return IS.runScript(inputCallback);
}

bool IotaScript::encodeScript(JsonArray& script){
		IStoken *newTokens = new IStoken[script.size()+1];
		size_t newTokenCount = 0;
		float newConstants[30];
		size_t newConstantCount = 0;
				
		for(int i=0; i<script.size(); i++){
			
			if(script[i]["oper"] == "const"){
				newTokens[newTokenCount++] = getConstOp + newConstantCount;
			 	newConstants[newConstantCount++] = script[i]["value"].as<float>();
				continue;
			}
			
			else if(script[i]["oper"] == "binop"){
				char opCode = script[i]["value"].as<char *>()[0];
				for(int i=0; i<sizeof(opChars); i++){
					if(opCode == opChars[i]){
						newTokens[newTokenCount++] = i;
						break;
					}
				}
			}
			
			else if(script[i]["oper"] == "push"){
				newTokens[newTokenCount++] = opPush;
			}
			
			else if(script[i]["oper"] == "pop"){
				newTokens[newTokenCount++] = opPop;
			}
			
			else if(script[i]["oper"] == "abs"){
				newTokens[newTokenCount++] = opAbs;
			}
			
			else if(script[i]["oper"] == "input"){
				newTokens[newTokenCount++] = getInputOp + script[i]["value"].as<unsigned int>();
			}
		}
		
		newTokens[newTokenCount] = 0;
		if(_tokens) delete[] _tokens;
		_tokens = newTokens;
		
		if(_constants){
			delete[] _constants;
			_constants = NULL;
		} 
		if(newConstantCount){
			_constants = new float[newConstantCount];
			for(int i=0; i<newConstantCount; i++){
				_constants[i] = newConstants[i];
			}
		}
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
		}while(*token++);
	return result;
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

			
