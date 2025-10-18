#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "IotaScript.h"
class Script;

class integrator {

    public:
        integrator() :  _name(0),
                        _id(0),
                        _script(0),
                        _interval(5),
                        _synchronized(false),
                        _log(0),
                        _oldRec(0),
                        _newRec(0),
                        _state(initialize_s){};

        ~integrator();
       
        bool config(Script* script);
        void setScript(Script* script);
        void getStatusJson(JsonObject&);
        uint32_t dispatch(struct serviceBlock *serviceBlock);
        double run(IotaLogRecord *oldRecord, IotaLogRecord *newRecord, units Units, char method);
        void newLogEntry(IotaLogRecord *oldRecord, IotaLogRecord *newRecord);
        char *name();
        IotaLog* get_log();
        bool isSynchronized();
        void end();

    protected:
        char *_name;                    // Name of the integration
        char *_id;                      // ID used in messages
        Script *_script;                // --> integration Script
        int _interval;                  // aggregation interval
        bool _synchronized;             // integration log is up to date with datalog
        IotaLog *_log;                  // integration log
        IotaLogRecord *_oldRec;         // datalog records used during synchronization
        IotaLogRecord *_newRec;

        struct intRecord {
            uint32_t UNIXtime;          // Time period represented by this record
            int32_t serial;             // record number in file
            double sumPositive;         // Sum of positive intervals (import)
            double sumNegative;         // Sum of the negative intervals (export)
            double sumNet;                 // Superfluous but need to fill to factor of blocksize (32)
            intRecord() : UNIXtime(0), serial(0), sumPositive(0), sumNegative(0), sumNet(0){}; 
        } _intRec;

        intRecord _cache1, _cache2;     // The integration log cache reduces reads during queries
        intRecord *oldInt = &_cache1;
        intRecord *newInt = &_cache2;

        enum states {
            initialize_s,
            integrate_s,
            end_s
        } _state;

        uint32_t handle_initialize_s();
        uint32_t handle_integrate_s();
        uint32_t handle_end_s();
};

#endif