#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "iotawatt.h"

class integrator {
    public:
        integrator() :  _name(0),
                        _id(0),
                        _script(0),
                        _interval(60),
                        _log(0),
                        _oldRec(0),
                        _newRec(0),
                        _state(initialize_s){};

        ~integrator(){ 
            delete _log;
            delete[] _name;
            delete _oldRec;
            delete _newRec;
            };

        bool config(Script* script);
        void setScript(Script* script);
        void getStatusJson(JsonObject&);
        uint32_t dispatch(struct serviceBlock *serviceBlock);
        double run(IotaLogRecord *oldRecord, IotaLogRecord *newRecord, double elapsedHours);
        char *name();
        IotaLog* get_log();
        void end();

    protected:
        char *_name;
        char *_id;
        Script *_script;
        int _interval;
        bool _end;
        IotaLog *_log;
        IotaLog *_activelog;
        IotaLogRecord *_oldRec;
        IotaLogRecord *_newRec;

        struct intRecord {
            uint32_t UNIXtime;          // Time period represented by this record
            int32_t serial;             // record number in file
            double sumIntegral;         // Cummulative integration    
        } _intRec;

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