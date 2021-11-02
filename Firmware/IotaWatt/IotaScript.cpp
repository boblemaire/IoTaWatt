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

const char  opChars[] = "=+-*/<>|()";

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
        if( ! encodeScript(var.as<char*>())){
          log("Script: invalid Script in output %s.", _name);
        }
      }
      print();
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
        trace(T_Script, 0);
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
          uint8_t tokenType = *token & TOKEN_TYPE_MASK;
          uint8_t tokenDetail = *token & ~TOKEN_TYPE_MASK;
          if(tokenType == tokenOperator){
            string += String(opChars[tokenDetail]);
          }
          else if(tokenType == tokenInput) {
            string += SCRIPT_CHAR_INPUT + String(tokenDetail);
          }
          else if(tokenType == tokenConstant){
            string += String(_constants[tokenDetail - 1],4);
            while(string.endsWith("0")) string.remove(string.length()-1);
            if(string.endsWith(".")) string += '0';
          }
          else if(tokenType == tokenVirtual){
            string += SCRIPT_CHAR_VIRTUAL;
          }
          else if(tokenType == tokenIntegration){
            int index = tokenDetail;
            string += SCRIPT_CHAR_INTEGRATION + String(tokenDetail);
            string += char(*(++token));
          }
          else {
            string += "token(" + String(*token) + String(tokenType) + ")";
          }
          token++;
        }
}

bool    Script::encodeScript(const char* script){
  trace(T_Script, 5);
  char parseChars[16] = {SCRIPT_CHAR_CONSTANT, SCRIPT_CHAR_INPUT, SCRIPT_CHAR_INTEGRATION, SCRIPT_CHAR_VIRTUAL};
  trace(T_Script, 5);
  strcpy(&parseChars[4], opChars);
  int tokenCount = 0;
  int constantCount = 0;

  // Count the tokens, constants and integrations.

  const char *scan = script;
  trace(T_Script, 5);
  do {
    scan = strpbrk(scan, parseChars);
    if(scan){
      tokenCount++;
      if(*scan == SCRIPT_CHAR_CONSTANT){
        constantCount++;
      }
      scan++;
    }
  } while (scan);
  trace(T_Script, 6, tokenCount);
  
  // Allocate storage and encode.
  
  _tokens = new uint8_t[tokenCount + 1];
  _constants = new float[constantCount];
  int j = 0;
  int i = 0;     
  while(script[j]){

    // Input operand

    if(script[j] == SCRIPT_CHAR_INPUT){
      char* endptr;
      int n = strtol(&script[j+1], &endptr, 10);
      trace(T_Script, 10, n);
      j = endptr - script;
      _tokens[i++] = tokenInput + n;
    } 

    // Constant operand

    else if (script[j] == SCRIPT_CHAR_CONSTANT){
      trace(T_Script, 15);
      _tokens[i++] = tokenConstant + constantCount--;
      char* endptr;
      _constants[constantCount] = strtof(&script[j+1], &endptr);
      j = endptr - script;
    }
   
    // Virtual operand

    else if(script[j] == SCRIPT_CHAR_VIRTUAL){
      char* endptr;
      int n = strtol(&script[j+1], &endptr, 10);
      trace(T_Script, 20, n);
      j = endptr - script;
      _tokens[i++] = tokenVirtual + n;
    }

    // Integration operand

    else if(script[j] == SCRIPT_CHAR_INTEGRATION){
      trace(T_Script, 25, script[j+1]);
      char method = 'N';
      if(script[j+1] == '+'){
        method = '+';
        j++;
      }
      else if(script[j+1] == '-'){
        method = '-';
        j++;
      }
      int len = strcspn(&script[j + 1], opChars);
      char name[32];
      memcpy(name, &script[j+1], len);
      name[len] = 0;
      int index = 0;
      Script* integration = integrations->first();
      while(integration){
        if(strcmp(name, integration->name()) == 0){
          break;
        }
        integration = integration->next();
        index++;
      }
      if(integration){
        trace(T_Script, 28, index);
        _tokens[i++] = tokenIntegration + index;
        _tokens[i++] = method;
      }
      else {
        trace(T_Script, 29);
        log("Script: integration %s not defined in script %s.", name, this->_name);
        _tokens[i++] = tokenConstant;
      }
      j += len + 1;
    }

    // Operator

    else if(strchr(opChars, script[j])){
      trace(T_Script, 30);
      _tokens[i++] = tokenOperator + strchr(opChars, script[j++]) - opChars;
    }

    // invalid token/script

    else {
      _tokens[0] = opEq;
      return false;
    }
  }
  _tokens[i] = 0;
  return true;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, const char* overideUnits){
        for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(overideUnits,unitstr[i]) == 0){
            return run(oldRec, newRec, (units) i);
          } 
        }
        return 0;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec){
        return run(oldRec, newRec, _units);
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, units Units){
  double elapsedHours = 1.0;
  if(oldRec){
    elapsedHours = newRec->logHours - oldRec->logHours;
  }
  //Serial.printf("run Script %s, units %s\n", this->_name, unitstr[Units]);
  uint8_t *tokens = _tokens;
  double result;

  switch (Units)
  {
  case Watts:
  case Volts:
  case Amps:
  case Hz:
  case Wh:
  case VAR:
  case VARh:
    result = runRecursive(&tokens, oldRec, newRec, Units);
    break;

  case VA:
  {
    double var = runRecursive(&tokens, oldRec, newRec, VAR);
    double watts = runRecursive(&tokens, oldRec, newRec, Watts);
    result = sqrt(var * var + watts * watts);
    break;
          }
          
          case VAh:
          {
            double varh = runRecursive(&tokens, oldRec, newRec, VARh); 
            double wh = runRecursive(&tokens, oldRec, newRec, Wh);
            result = sqrt(varh * varh + wh * wh);
            break;
          }

          case kWh:
            result = runRecursive(&tokens, oldRec, newRec, Wh) / 1000.0; 
            break;

          case PF:
          {
            double watts = runRecursive(&tokens, oldRec, newRec, Watts);
            double va = runRecursive(&tokens, oldRec, newRec, VA);
            result = watts / va;
            break;
          }

          default:
            result = 0.0;
        }
        
        if(result != result) return 0.0;
        return result;
                
}

double  Script::runRecursive(uint8_t** tokens, IotaLogRecord* oldRec, IotaLogRecord* newRec, units Units){
  double elapsedHours = 1.0;
  if(oldRec){
    elapsedHours = newRec->logHours - oldRec->logHours;
  }
  double result = 0.0;
  double operand = 0.0;
  uint8_t pendingOp = opAdd;
  int vchannel;
  uint8_t *token = *tokens;
  do
  {
    uint8_t tokenType = *token & TOKEN_TYPE_MASK;
    uint8_t tokenDetail = *token & ~TOKEN_TYPE_MASK;
    switch (tokenType)
    {
      case tokenOperator:

        switch (tokenDetail)
        {
          case opAdd:
          case opSub:
          case opMult:
          case opDiv:
          case opMin:
          case opMax:
            result = operate(result, pendingOp, operand);
            pendingOp = *token;
            operand = 0;
            if (tokenDetail == opDiv || tokenDetail == opMult)
              operand = 1;
            break;

          case opAbs:
            if (operand < 0)
              operand = 0 - operand;
            break;

          case opPush:
            token++;
            operand = runRecursive(&token, oldRec, newRec, Units);
            break;

          case opPop:
            *tokens = token;
            return operate(result, pendingOp, operand);

          case opEq:
            return operate(result, pendingOp, operand);
        } // switch (tokenDetail)
        break;

      case tokenConstant:
        if(tokenDetail){
          operand = _constants[tokenDetail - 1];
        }
        else {
          operand = 0;
        }
        break;

      case tokenInput:
      {
        int input = *token & ~TOKEN_TYPE_MASK;
        double accum1 = newRec->accum1[input] - (oldRec ? oldRec->accum1[input] : 0);
        double accum2 = newRec->accum2[input] - (oldRec ? oldRec->accum2[input] : 0);
        ;
        vchannel = inputChannel[input]->_vchannel;
        double volts = newRec->accum1[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0) * inputChannel[input]->_vmult;
        double hz = newRec->accum2[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0);

        switch (Units)
        {

          case Watts:
            operand = accum1 / elapsedHours;
            break;

          case Volts:
            operand = volts / elapsedHours;
            break;

          case Amps:
          {
            double va = accum2 / elapsedHours;
            operand = volts / elapsedHours;
            if (operand != 0.0)
            {
              operand = va / operand;
            }
            break;
          }

          case VA:
            operand = accum2 / elapsedHours;
            break;

          case VAh:
            operand = accum2;
            break;

          case Hz:
            operand = hz / elapsedHours;
            break;

          case Wh:
            operand = accum1;
            break;

          case VAR:
          {
            double va = accum2 / elapsedHours;
            double watts = accum1 / elapsedHours;
            operand = sqrt(va * va - watts * watts);
            break;
          }

          case VARh:
          {
            double vah = accum2;
            double wh = accum1;
            operand = sqrt(vah * vah - wh * wh);
            break;
          }

          default:
            operand = 0.0;
            break;
             
        } // switch (units)
        break;
      }

      case tokenVirtual:
      {
        operand = 0;
        if (tokenDetail == 0 && simsolar)
        {
          if (oldRec) {
            double elapsed = newRec->logHours - oldRec->logHours;
            operand = simsolar->energy(localTime(oldRec->UNIXtime), localTime(newRec->UNIXtime)) * elapsed / (double(newRec->UNIXtime - oldRec->UNIXtime) / 3600);
            if(Units == Watts){
              operand /= double(newRec->UNIXtime - oldRec->UNIXtime) / 3600;
            }
          }
          else {
            operand = simsolar->power(localTime(newRec->UNIXtime));
          }
        }
        break;
      }

      case tokenIntegration:
      {
        trace(T_Script, 30);
        int index = tokenDetail;
        Script *integration = integrations->first();
        while (index && integration)
        {
          integration = integration->next();
        }
        if (!integration)
        {
          trace(T_Script, 31);  
          return 0;
        }
        integrator *_integrator = (integrator *)(integration->getParm());
        char method = *(++token);

        // Get integrated value

        trace(T_Script, 30);
        operand = _integrator->run(oldRec, newRec, Units, method);
        trace(T_Script, 30);
        break;
      }
    } // switch (tokenType)

    if (operand != operand)
      operand = 0;

  } while(*token++);
  return 0;
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

Script* ScriptSet::script(const char *name){
  Script *script = _listHead;
  while (script) {
    if(strcmp(script->name(), name) == 0){
      return script;
    }
    script = script->next();
  }
  return nullptr;
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