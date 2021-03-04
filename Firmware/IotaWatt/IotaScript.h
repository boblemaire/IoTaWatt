#ifndef IotaScript_h
#define IotaScript_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IotaLog.h"

// Following enumeration must match unitstr[] in .cpp.

enum units     // Must match unitstr[] in .cpp
{           
  Watts = 0,
  Volts = 1,
  Amps = 2,
  VA = 3,
  VAh = 4,
  Hz = 5,
  Wh = 6,
  kWh = 7,
  PF = 8,
  VAR = 9,
  VARh = 10,
  unitsNone = 11
};         
    
enum opCodes // Must match opChars[] in .cpp
{   
  opEq  = 0,
  opAdd   = 1,
  opSub   = 2,
  opMult  = 3,
  opDiv   = 4,
  opMin   = 5,
  opMax   = 6,
  opAbs   = 7,
  opPush  = 8,
  opPop   = 9
};

enum tokenTypes
{
  tokenOperator = 0,
  tokenInput = 0x20,
  tokenConstant = 0x40,
};

class Script {

  friend class ScriptSet;

  public:

    Script(JsonObject&);
    Script(const char* name, const char* units, const char* script); 
    ~Script();

    const char*   name();     // name associated with this Script
    const char*   getUnits();    // units associated with this Script
    void    setUnits(const char*);
    void    setParm(void*);      // Set parm value
    void*   getParm();           // Retrieve parm value
    Script* next();           // -> next Script in set

    double  run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours); // Run this Script
    double  run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units); // Run w/overide units
    double  run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, const char* overideUnits);

    void    print();
    int     precision();

  private:

    Script*     _next;      // -> next in list
    char*       _name;      // name associated with this Script
    void*       _parm;      // External parameter
    union {
      float *_constants;    // Constant values referenced in Script
      Script **_integrations;
    };
    uint8_t*    _tokens;    // Script tokens
    units       _units;     // Units to be computed              

    double    runRecursive(uint8_t**, IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units Units);
    double    operate(double, byte, double);
    bool      encodeScript(const char* script);

};

class ScriptSet {

  public:
    ScriptSet()
      :_count(0)
      ,_listHead(0)
      {}


    ScriptSet (JsonArray& JsonScriptSet) {
      _count = JsonScriptSet.size();
      _listHead = nullptr;
      if(_count){
        JsonObject& obj = JsonScriptSet.get<JsonObject>(0);
        _listHead = new Script(obj);
        Script* script = _listHead;
        for(int i=1; i<_count; ++i){
          JsonObject& obj = JsonScriptSet.get<JsonObject>(i);
          script->_next = new Script(obj);
          script = script->_next;
        }
      }
    }

    ~ScriptSet(){
      Script* script;
      while(script = _listHead){
        _listHead = script->next();
        delete script;
      }
    }

    //typedef std::function<int(Script*, Script*)> scriptCompare;

    void sort(std::function<int(Script*, Script*)> scriptCompare);

    size_t    count();      // Retrieve count of Scripts in the set.
    Script*   first();      // Get -> first Script in set
    Script*   script(const char *name);

  private:

    size_t    _count;       // The actual count
    Script*   _listHead;      // -> first Script

};


#endif // IotaScript_h
