#include "iotawatt.h"
 
    // This is the worm hole that the scheduler uses to get into the class state machine.
    // It invokes the dispatch method of the class.
    // On return, it checks for no_schedule return and invokes the stop() method. 

uint32_t integrator_dispatch(struct serviceBlock* serviceBlock) {
    trace(T_integrator,0);
    integrator *_this = (integrator *)serviceBlock->serviceParm;
    trace(T_integrator,1);
    uint32_t reschedule = _this->dispatch(serviceBlock);
    trace(T_integrator,1);
    if(reschedule){
        return reschedule;
    } 
    trace(T_integrator,0);
    return 0;
}

uint32_t integrator::dispatch(struct serviceBlock *serviceBlock)
{
    trace(T_integrator,2,_state);
    switch (_state) {
        case initialize_s:
            serviceBlock->priority = priorityLow;
            return handle_initialize_s();
        case integrate_s:
            return handle_integrate_s();
        case end_s:
            return handle_end_s();
    }
    trace(T_integrator,3,_state);
    log("%s: Unrecognized state, stopping;",_id);
    delete this;
    return 0;
}

uint32_t integrator::handle_initialize_s(){
    trace(T_integrator,10);

    // Wait until history log is open.

    if(!History_log.isOpen()){
        return 5;
    }

    log("%s: Started", _id);

    // Insure /iotawatt/integrations exists.

    char *dir = charstar(F("/iotawatt/integrations"));

    if( ! SD.exists(dir)){
        if(!SD.mkdir(dir)){
            log("%s: could not create integration directory.", _id);
            delete this;
            return 0;
        }
    }

    // Open the integration log

    trace(T_integrator,10);
    String filepath(dir);
    filepath += '/';
    filepath += _name;
    _log = new IotaLog(16, 60, 3650);
    trace(T_integrator,10);
    if(_log->begin(filepath.c_str())){
        log("%s: Couldn't open integration file %s.", _id, filepath.c_str());
        delete _log;
        delete this;
        return 0;
    }

    uint32_t lastWrite = MAX(_log->lastKey(), History_log.firstKey());

    // Initialize the intRecord;

    _intRec.UNIXtime = lastWrite;
    _intRec.serial = 0;
    _intRec.sumIntegral = 0.0;

    if(_log->lastKey()){
        _log->readKey((IotaLogRecord*)&_intRec);
        log("%s: Last log entry %s", _id, localDateString(_log->lastKey()).c_str());
    }
    else {
        log("%s: New log starting %s", _id, localDateString(lastWrite).c_str());
    }

            _activelog = &Current_log;
    if((_intRec.UNIXtime + _interval + _interval) < Current_log.firstKey()){
        _activelog = &History_log;
    }

    _state = integrate_s;
    return 1;
}

uint32_t integrator::handle_integrate_s(){

    // While data is available

    while(Current_log.lastKey() >= _intRec.UNIXtime + _interval){

        if(!_oldRec){
            _oldRec = new IotaLogRecord;
            _newRec = new IotaLogRecord;
            _newRec->UNIXtime = _intRec.UNIXtime;
            _activelog->readKey(_newRec);
        }

        if(_activelog == &History_log && _intRec.UNIXtime >= Current_log.firstKey()){
            _activelog = &Current_log;
            _activelog->readKey(_newRec);
        }

        while (_newRec->UNIXtime < _intRec.UNIXtime + _interval){
            IotaLogRecord *swapRec = _oldRec;
            _oldRec = _newRec;
            _newRec = swapRec;
            _newRec->UNIXtime = _oldRec->UNIXtime;
            _newRec->serial = _oldRec->serial;
            _activelog->readNext(_newRec);

            // If this record is within interval, add integral.

            if(_newRec->UNIXtime < _intRec.UNIXtime + _interval){
                double elapsed = _newRec->logHours - _oldRec->logHours;
                if(elapsed == elapsed && elapsed > 0){
                    _intRec.sumIntegral += _script->run(_oldRec, _newRec, elapsed);
                }
            }
        }

        // Integration complete for this interval, log it.

        _intRec.UNIXtime += _interval;
        _log->write((IotaLogRecord *)&_intRec);

        if(millis() >= nextCrossMs - 1){
            return 1;
        }
    }
    
    delete _oldRec;
    _oldRec = nullptr;
    delete _newRec;
    _newRec = nullptr;
    return UTCtime() + 1;
}

char *integrator::name(){
    return _name;
}

IotaLog* integrator::get_log(){
    return _log;
}

void integrator::setScript(Script* script){
    _script = script;
}

void integrator::end(){
    _state = end_s;
}

uint32_t integrator::handle_end_s(){
    log("%s: ended. Last entry %s", _id, localDateString(_intRec.UNIXtime).c_str());
    delete this;
    return 0;
}

bool integrator::config(Script* script){
    trace(T_integrator,105);
    _id = charstar(script->name());
    _name = _id;
    _script = script;
    if(_state == initialize_s) {
        trace(T_integrator,105);
        serviceBlock *sb = NewService(integrator_dispatch, T_integrator);
        sb->serviceParm = (void *)this;
    }
    trace(T_integrator,106);
    return true;
}