class Script {
  
  friend class ScriptSet;

  public:
    
    Script(JsonObject& JsonScript) {
      _next = NULL;
      JsonVariant var = JsonScript["name"];
      if(var.success()){
        _name = new char[strlen(var.as<char*>())+1];
        strcpy(_name, var.as<char*>());
      }
      var = JsonScript["units"];
      if(var.success()){
        _units = new char[strlen(var.as<char*>())+1];
        strcpy(_units, var.as<char*>());
      }
      var = JsonScript["script"];
      if(var.success()){
        encodeScript(var.as<char*>() );
      }
    }

    ~Script() {
      delete[] _name;
      delete[] _units;
      delete[] _tokens;
      delete[] _constants;
    }
    
    char*   name();     // name associated with this Script
    char*   units();    // units associated with this Script
    Script*   next();     // -> next Script in set
    
    double    run(double inputCallback(int)); // Run this Script
    void    print();
      
  private:
  
    Script*     _next;      // -> next in list  
    char*       _name;      // name associated with this Script
    char*       _units;     // units associated with this Script
    uint8_t*    _tokens;    // Script tokens
    float*     _constants;   // Constant values referenced in Script
    const byte  getInputOp = 32;
    const byte  getConstOp = 64;
    enum        opCodes {
                opEq  = 0,
                opAdd   = 1,
                opSub   = 2,
                opMult  = 3,
                opDiv   = 4,
                opAbs   = 5,
                opPush  = 6,
                opPop   = 7};
    const char* opChars = "=+-*/|()";
    
    double    runRecursive(uint8_t**, double inputCallback(int));
    double    evaluate(double, byte, double);
    bool      encodeScript(const char* script);   

};  
    
class ScriptSet {
  
  public:
    ScriptSet (JsonArray& JsonScriptSet) {
      _count = JsonScriptSet.size();
      JsonObject& obj = JsonScriptSet.get<JsonObject>(0);
      _listHead = new Script(obj);
      Script* script = _listHead;
      for(int i=1; i<_count; ++i){
        JsonObject& obj = JsonScriptSet.get<JsonObject>(i);
        script->_next = new Script(obj);
        script = script->_next;
      }
    }
    
    ~ScriptSet(){
      Script* script;
      while(script = _listHead){
        _listHead = script->next();
        delete script;
      }
    }
    
    size_t    count();      // Retrieve count of Scripts in the set.
    Script*   first();      // Get -> first Script in set
      
  private:

    size_t    _count;       // The actual count
    Script*   _listHead;      // -> first Script
  
};

Script*   Script::next() {return _next;}

char*     Script::name() {return _name;} 

char*   Script::units(){return _units;}

size_t    ScriptSet::count() {return _count;}

Script*   ScriptSet::first() {return _listHead;}  

void    Script::print() {
        uint8_t* token = _tokens;
        String string = "Script:";
        string += _name;
        string += ",units:";
        string += _units;
        string += ' ';
        while(*token){
          if(*token < getInputOp){
            string += String(opChars[*token]);
          }
          else if(*token & getInputOp){
            string += "@" + String(*token - getInputOp);
          }
          else if(*token & getConstOp){
            string += String(_constants[*token - getConstOp],4);
            while(string.endsWith("0")) string.remove(string.length()-1);
            if(string.endsWith(".")) string += '0';
          }
          else {
            string += "token(" + String(*token) + ")";
          }
          token++;
        }
        Serial.println(string);
}

bool    Script::encodeScript(const char* script){
        int tokenCount = 0;
        int constCount = 0;
        int consts = constCount;
        for(int i=0; i<strlen(script); i++){
          if(script[i] == '#')constCount++;
          if((!isDigit(script[i])) && (script[i] != '.')) tokenCount++;
        }
        _tokens = new uint8_t[tokenCount + 1];
        _constants = new float[constCount];
        int j = 0;      
        for(int i=0; i<tokenCount; i++){
          if(script[j] == '@'){
            int n = script[++j] - '0';
            while(isDigit(script[++j])) n = n * 10 + (script[j] - '0');
            _tokens[i] = getInputOp + n;
          } 
          else if (script[j] == '#'){
            _tokens[i] = getConstOp + --constCount;
            String number;
            while(isDigit(script[++j]) || script[j] == '.') number += script[j];
            _constants[constCount] = number.toFloat();
          }
          else {
            for(int k=0; k<strlen(opChars); ++k){
              if(script[j] == opChars[k]){
                _tokens[i] = k;
                break;
              }
            }
            j++;
          }   
        }
        _tokens[tokenCount] = 0;
}

double  Script::run(double inputCallback(int)){
        uint8_t* tokens = _tokens;
        return runRecursive(&tokens, inputCallback);
                
}

double  Script::runRecursive(uint8_t** tokens, double inputCallback(int)){
        double result = 0.0;
        double operand = 0.0;
        uint8_t pendingOp = opAdd;
        uint8_t* token = *tokens;
        do {
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
            operand = runRecursive(&token, inputCallback);
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

double    Script::evaluate(double result, uint8_t token, double operand){
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
