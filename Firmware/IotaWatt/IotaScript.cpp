#include "IotaWatt.h"
#include "IotaScript.h"

Script::Script(JsonObject& JsonScript) {
      _next = NULL;
      JsonVariant var = JsonScript["name"];
      if(var.success()){
        _name = charstar(var.as<char*>());
      }
      _units = charstar("Watts");
      _accum = 0;
      var = JsonScript["units"];
      if(var.success()){
             if(strcmp_ci(var.as<char*>(), "Watts") == 0){_units = charstar("Watts"); _dec = 2;}
        else if(strcmp_ci(var.as<char*>(), "Volts") == 0){_units = charstar("Volts"); _dec = 2;}
        else if(strcmp_ci(var.as<char*>(), "Amps" ) == 0){_units = charstar("Amps"); _dec=3; _accum = 1;}
        else if(strcmp_ci(var.as<char*>(), "Hz"   ) == 0){_units = charstar("Hz"); _dec=2; _accum = 1;}
        else if(strcmp_ci(var.as<char*>(), "pf"   ) == 0){_units = charstar("pf"); _dec=3; _accum = 1;}
        else if(strcmp_ci(var.as<char*>(), "VA"   ) == 0){_units = charstar("VA"); _dec=2; _accum = 1;}
        else if(strcmp_ci(var.as<char*>(), "Wh"   ) == 0){_units = charstar("Wh"); _dec=4; _accum = 1;}
        else if(strcmp_ci(var.as<char*>(), "kWh"  ) == 0){_units = charstar("kWh"); _dec=7; _accum = 1;}  
      }
      var = JsonScript["script"];
      if(var.success()){
        encodeScript(var.as<char*>() );
      }
    }

Script::~Script() {
      delete[] _name;
      delete[] _units;
      delete[] _tokens;
      delete[] _constants;
    }

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

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours){
        uint8_t* tokens = _tokens;
        if(strcmp(_units, "Wh") == 0){
          elapsedHours = 1.0;
        }
        else if(strcmp(_units, "kWh") == 0){
          elapsedHours = 1000.0;
        }
        double result = runRecursive(&tokens, oldRec, newRec, elapsedHours);
        if(strcmp(_units, "pf") == 0){
          _accum = 0;
          result = runRecursive(&tokens, oldRec, newRec, elapsedHours) / result;
          if(result > 1.1) result = 0.0;
          _accum = 1;
        }
        if(result != result) return 0.0;
        return result;
                
}

double  Script::runRecursive(uint8_t** tokens, IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours){
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
            operand = runRecursive(&token, oldRec, newRec, elapsedHours);
          }
          else if(*token == opPop){ 
            *tokens = token;
            return evaluate(result, pendingOp, operand);
          }
          else if(*token == opEq){
            return evaluate(result, pendingOp, operand);
          }
        
          if(*token & getInputOp){
            if(_accum == 0){
              operand = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0)) / elapsedHours;
            }
            else {
              operand = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
            }
            if(operand != operand) operand = 0;
            if(strcmp(_units, "Amps") == 0){
              int vchannel = inputChannel[*token % 32]->_vchannel;
              operand /= (newRec->accum1[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0.0)) / elapsedHours;
            }
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
            if(operand == 0){
              return 0;
            }
            return result / operand;        
        }
}
