#ifndef IotaScript_h
#define IotaScript_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IotaLog.h"

enum        units {
            unitsWatts = 0,
            unitsVolts = 1,
            unitsAmps = 2,
            unitsVA = 3,
            unitsHz = 4,
            unitsWh = 5,
            unitskWh = 6,
            unitsPF = 7,
            unitsNone = 8
            };         // Units to be computed   

class Script {

  friend class ScriptSet;

  public:

    Script(JsonObject&); 
    ~Script();

    char*   name();     // name associated with this Script
    const char*   getUnits();    // units associated with this Script
    void    setUnits(const char*);
    Script* next();     // -> next Script in set

    double  run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours); // Run this Script
    double  run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units); // Run w/overide units
    void    print();
    int     precision();

  private:

    Script*     _next;      // -> next in list
    char*       _name;      // name associated with this Script
    float*      _constants; // Constant values referenced in Script
    uint8_t*    _tokens;    // Script tokens
    units       _units;     // Units to be computed              
    uint8_t     _accum;               // Accumulators to use in fetching operands
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

    double    runRecursive(uint8_t**, IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, char type);
    double    evaluate(double, byte, double);
    bool      encodeScript(const char* script);

};

class ScriptSet {

  public:
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

    size_t    count();      // Retrieve count of Scripts in the set.
    Script*   first();      // Get -> first Script in set

  private:

    size_t    _count;       // The actual count
    Script*   _listHead;      // -> first Script

};


#endif // IotaScript_h
