#include "IotaWatt.h"
#include "IotaScript.h"

// Following literals must match enumeration in .h

const char*      unitstr[] = {
                    "Watts",
                    "Volts", 
                    "Amps", 
                    "VA",
                    "VAh", 
                    "Hz", 
                    "Wh", 
                    "kWh", 
                    "PF",
                    "VAR",
                    "VARh",
                    ""
                    };

uint8_t     unitsPrecision[] = { 
                    /*Watts*/ 2,
                    /*Volts*/ 2, 
                    /*Amps*/  3, 
                    /*VA*/    2, 
                    /*Hz*/    2, 
                    /*Wh*/    4, 
                    /*kWh*/   7, 
                    /*PF*/    3,
                    /*VAR*/   2,
                    /*VARh*/  4,
                    /*None*/  0 
                    };                   

Script::Script(JsonObject& JsonScript)
      :_next(nullptr)
      ,_name(nullptr)
      ,_parm(nullptr)
      ,_constants(nullptr)
      ,_tokens(nullptr)
      ,_units(Watts)
      
    {
      JsonVariant var = JsonScript["name"];
      if(var.success()){
        _name = charstar(var.as<char*>());
      }
    
      _units = Watts;
      var = JsonScript["units"];
      if(var.success()){
        for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(var.as<char*>(),unitstr[i]) == 0){
            _units = (units)i;
            break;
          } 
        }
      }

      var = JsonScript["script"];
      if(var.success()){
        encodeScript(var.as<char*>());
      }
    }

Script::Script(const char* name, const char* unit, const char* script)
      :_next(nullptr)
      ,_name(nullptr)
      ,_parm(nullptr)
      ,_constants(nullptr)
      ,_tokens(nullptr)
      ,_units(Watts)
       
    {
      _name = charstar(name);

       for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(unit,unitstr[i]) == 0){
            _units = (units)i;
            break;
          } 
        }
      encodeScript(script);
    }

Script::~Script() {
      delete[] _name;
      delete[] _tokens;
      delete[] _constants;
    }

Script*       Script::next() {return _next;}

const char*   Script::name() {return _name;} 

const char*   Script::getUnits() {return unitstr[_units];}

void          Script::setParm(void* value) {_parm = value;}

void*         Script::getParm() {return _parm;}

int           Script::precision() {return unitsPrecision[_units];}

size_t        ScriptSet::count() {return _count;}

Script*       ScriptSet::first() {return _listHead;}  

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
        int i = 0;     
        while(script[j]){
          if(script[j] == '@'){
            char* endptr;
            int n = strtol(&script[j+1], &endptr, 10);
            j = endptr - script;
            _tokens[i++] = getInputOp + n;
          } 
          else if (script[j] == '#'){
            _tokens[i++] = getConstOp + --constCount;
            char* endptr;
            _constants[constCount] = strtof(&script[j+1], &endptr);
            j = endptr - script;
          }
          else {
            _tokens[i++] = strchr(opChars, script[j++]) - opChars;
          }
        }
        _tokens[i] = 0;
        return true;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, const char* overideUnits){
        for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(overideUnits,unitstr[i]) == 0){
            return run(oldRec, newRec, elapsedHours, (units) i);
          } 
        }
        return 0;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours){
        double result = run(oldRec, newRec, elapsedHours, _units);
        return result;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units Units){
        uint8_t* tokens = _tokens;
        double result, va;
        union {
          double watts; 
          double wh;
         };
        union {
          double var;
          double varh;
        };
        switch(Units) {
          case Watts:
          case Volts:
          case Amps:
          case Hz:
          case Wh:
          case VAR:
          case VARh:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, Units); 
            break;

          case VA:
            var = runRecursive(&tokens, oldRec, newRec, elapsedHours, VAR); 
            watts = runRecursive(&tokens, oldRec, newRec, elapsedHours, Watts);
            result = sqrt(var * var + watts * watts);
            break;
          
          case VAh:
            varh = runRecursive(&tokens, oldRec, newRec, elapsedHours, VARh); 
            wh = runRecursive(&tokens, oldRec, newRec, elapsedHours, Wh);
            result = sqrt(varh * varh + wh * wh);
            break;

          case kWh:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, Wh) / 1000.0; 
            break;

          case PF:
            watts = runRecursive(&tokens, oldRec, newRec, elapsedHours, Watts);
            var = runRecursive(&tokens, oldRec, newRec, elapsedHours, VAR);
            result = watts / sqrt(var * var + watts * watts);
            break;

          default:
            result = 0.0;
        }
        
        if(result != result) return 0.0;
        return result;
                
}

double  Script::runRecursive(uint8_t** tokens, IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units Units){
        double result = 0.0;
        double operand = 0.0;
        int vchannel;
        uint8_t pendingOp = opAdd;
        uint8_t* token = *tokens;
        do {
          if(*token >= opAdd && *token <= opMax){
            result = operate(result, pendingOp, operand);
            pendingOp = *token;
            operand = 0;
            if(*token == opDiv || *token == opMult) operand = 1;
          }
          else if(*token == opAbs){
            if(operand < 0) operand = 0 - operand;
          }       
          else if(*token == opPush){
            token++;
            operand = runRecursive(&token, oldRec, newRec, elapsedHours, Units);
          }
          else if(*token == opPop){ 
            *tokens = token;
            return operate(result, pendingOp, operand);
          }
          else if(*token == opEq){
            return operate(result, pendingOp, operand);
          }
          if(*token & getConstOp){
            operand = _constants[*token % 32];
          }

          if(*token & getInputOp){
            union {
              double va;
              double vah;
            };
            union {
              double watts;
              double wh;
            };
            switch(Units) {

              case Watts:
                operand = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0)) / elapsedHours;
                break;

              case Volts:
                vchannel = inputChannel[*token % 32]->_vchannel;
                operand = (newRec->accum1[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0.0)) / elapsedHours;
                break;
                
              case Amps:
                va = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
                vchannel = inputChannel[*token % 32]->_vchannel;
                operand = ((newRec->accum1[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0.0)) / elapsedHours);
                if(operand != 0.0){
                  operand = va / (operand * inputChannel[*token % 32]->_vmult);
                }
                break;

              case VA:
                operand = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
                break;

              case VAh:
                operand = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0));
                break;

              case Hz:
                vchannel = inputChannel[*token % 32]->_vchannel;
                operand = (newRec->accum2[vchannel] - (oldRec ? oldRec->accum2[vchannel] : 0.0)) / elapsedHours;
                break;

              case Wh:
                operand = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0));
                break;

              case VAR:
                va = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
                watts = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0)) / elapsedHours;
                operand = sqrt(va*va - watts*watts);
                break;

              case VARh:
                vah = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0));
                wh = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0));
                operand = sqrt(vah*vah - wh*wh);
                break;

              default:
                operand = 0.0;
                break;
            }
          }
          else {
            operand = 0.0;
          }

          if(operand != operand) operand = 0;

        } while(token++);
        return 0.0;
}

double    Script::operate(double result, uint8_t token, double operand){
        switch (token) {
          case opAdd:  return result + operand;
          case opSub:  return result - operand;
          case opMult: return result * operand;
          case opDiv:  return operand == 0 ? 0 : result / operand;
          case opMin:  return result < operand ? result : operand;
          case opMax:  return result > operand ? result : operand;
          default:     return 0;        
        }
}

          // Sort the Scripts in the set
          // Uses callback comparison
          // Simple bubble sort

void  ScriptSet::sort(std::function<int(Script*, Script*)> scriptCompare){
  int count = _count;
  while(--count){
    Script* link = nullptr;
    Script* a = _listHead;
    Script* b = a->_next;
    for(int i=0; i<count; i++){
      int comp = scriptCompare(a, b);
      if( comp > 0){
        a->_next = b->_next;
        b->_next = a;
        if(link){
          link->_next = b;
        } else {
          _listHead = b;
        }
        link = b;
      } else {
        link = a;
      }
      a = link->_next;
      b = a->_next;
    }
  }
}