#ifndef IotaScript_h
#define IotaScript_h

#include <Arduino.h>
#include <ArduinoJson.h>

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
