#include "IotaWatt.h"
 
    // The integrator Service is created at startup to synchronize the 
    // integration log with the datalog, including [re]creating.
    // When the log catches up to the datalog, the datalog Service
    // takes over with direct calls to create new entries at the
    // same time as datalog records, eliminating race conditions. 

const char intDirectory_P[] PROGMEM = IOTA_INTEGRATIONS_DIR;

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
    return 0;
}

integrator::~integrator(){
    _log->end();
    String filepath(FPSTR(intDirectory_P));
    filepath += "/";
    filepath += _name;
    filepath += ".log";
    SD.remove(filepath.c_str());
    log("%s: Integration log %s deleted.", _id, _name);
    delete _log;
    delete[] _name;
    delete _oldRec;
    delete _newRec;
};

uint32_t integrator::handle_initialize_s(){
    trace(T_integrator,10);

    if( ! Current_log.isOpen()){
        return UTCtime() + 5;
    }

    log("%s: Started", _id);

    // Insure /iotawatt/integrations exists.

    if( ! SD.exists(FPSTR(intDirectory_P))){
        if(!SD.mkdir(FPSTR(intDirectory_P))){
            log("%s: could not create integration directory.", _id);
            return 0;
        } 
    }

    // Open the integration log

    trace(T_integrator,10);
    String filepath(FPSTR(intDirectory_P));
    filepath += '/';
    filepath += _name;
    filepath += ".log";
    _log = new IotaLog(sizeof(intRecord), 5, 366, 32);
    trace(T_integrator,10);
    if(_log->begin(filepath.c_str())){
        log("%s: Couldn't open integration file %s.", _id, filepath.c_str());
        delete _log;
        return 0;
    }
    trace(T_integrator,10);

    // Initialize the intRecord;

    _intRec.UNIXtime = _log->lastKey();
    _intRec.serial = 0;
    _intRec.sumPositive = 0;
    _intRec.sumNegative = 0;
    _intRec.sumNet = 0;

    if(_intRec.UNIXtime){
        _log->readKey((IotaLogRecord*)&_intRec);
        log("%s: Last log entry %s", _id, localDateString(_log->lastKey()).c_str());
    }
    else {
        _intRec.UNIXtime = MAX(Current_log.lastKey() - (3600 * 24), Current_log.firstKey());
        log("%s: New log starting %s", _id, localDateString(_intRec.UNIXtime).c_str());
    }

    _log->writeCache(true);
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
            Current_log.readKey(_newRec);
        }

        if(_newRec->UNIXtime < _intRec.UNIXtime + _interval){
            IotaLogRecord *swapRec = _oldRec;
            _oldRec = _newRec;
            _newRec = swapRec;
            _newRec->UNIXtime = _oldRec->UNIXtime + _interval;
            _newRec->serial = _oldRec->serial;
            Current_log.readNext(_newRec);
        }

        // If this record is within interval, add to integral.

        if(_newRec->UNIXtime <= _intRec.UNIXtime + _interval){
            double elapsed = _newRec->logHours - _oldRec->logHours;
            if(elapsed == elapsed && elapsed > 0){
                double value = _script->run(_oldRec, _newRec, "Wh");
                if(value > 0){
                    _intRec.sumPositive += value;
                }
                else {
                    _intRec.sumNegative += value;
                }
                _intRec.sumNet += value;
            }
        }

        // If integration complete for this interval, log it.

        if(_newRec->UNIXtime >= _intRec.UNIXtime + _interval){
            _intRec.UNIXtime += _interval;
            _log->write((IotaLogRecord *)&_intRec);
        }

        if((micros() + 2500) >= bingoTime){
            return 10;
        }
    }
    
    delete _oldRec;
    _oldRec = nullptr;
    delete _newRec;
    _newRec = nullptr;
    _log->writeCache(false);
    _synchronized = true;
    return 0;
}

            // This method is invoked from the datalog Service when after a new entry is written.
            // If the integration log is up to date, it will write a corresponding integration log record.
            // Because the integration log is an extension of the datalog, race conditions can
            // produce invalid results if there is a window between datalog and integration log records.
            // This routine tightly couples the two to eliminate that window in normal operation.

void integrator::newLogEntry(IotaLogRecord* oldRecord, IotaLogRecord* newRecord){
    if(_synchronized){
        double elapsed = newRecord->logHours - oldRecord->logHours;
        if(elapsed == elapsed && elapsed > 0){
            double value = _script->run(oldRecord, newRecord, "Wh");
            if(value >= 0){
                _intRec.sumPositive += value;
            }
            else {
                _intRec.sumNegative += value;
            }
        }
        _intRec.UNIXtime += _interval;
        _log->write((IotaLogRecord *)&_intRec);
    }
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

bool integrator::isSynchronized(){
    return _synchronized;
}

void integrator::end(){
    if(_synchronized){
        delete this;
    }
    else {
        _state = end_s;
    }
}

uint32_t integrator::handle_end_s(){
    log("%s: ended. Last entry %s", _id, localDateString(_intRec.UNIXtime).c_str());
    delete this;
    return 0;
}

double integrator::run(IotaLogRecord *oldRecord, IotaLogRecord *newRecord, units Units, char method){
    trace(T_integrator, 0);
    if( ! _synchronized || newRecord->UNIXtime < _log->firstKey()){
        return 0;
    }
    
    // If being called for stats, no need for integration

    if (!oldRecord)
    {
        trace(T_integrator, 1);
        double operand = _script->run(oldRecord, newRecord, "Watts");
        trace(T_integrator, 1);

        if (method == '+' && operand < 0)
        {
            return 0;
        }
        else if (method == '-' && operand > 0)
        {
            return 0;
        }
        return operand;
    }

    
    double elapsedHours = 0;
    if(oldRecord->UNIXtime >= _log->firstKey()){
        trace(T_integrator, 3);
        elapsedHours = newRecord->logHours - oldRecord->logHours;
    }
    else {
        trace(T_integrator, 4);
        IotaLogRecord baseRecord;
        baseRecord.UNIXtime = _log->firstKey();
        Current_log.readKey(&baseRecord);
        elapsedHours = newRecord->logHours - baseRecord.logHours;
    }
    trace(T_integrator, 5);

    // If elapsed time is zero, result is zero.

    if(elapsedHours <= 0){
        return 0;
    }

    // Set to produce Wh or Watts as appropriate;

    if(Units == Wh){
        trace(T_integrator, 6);
        elapsedHours = 1;
    }

    // Fetch the integration log records.

    trace(T_integrator, 8);

    // See if we already have either record (very likely).
    // If so, move to correct position.

    if(oldRecord->UNIXtime == newInt->UNIXtime || newRecord->UNIXtime == oldInt->UNIXtime){
        trace(T_integrator, 8, 1);
        intRecord *swap = oldInt;
        oldInt = newInt;
        newInt = swap;
    }

        // Now read if necessary.

    if(oldRecord->UNIXtime != oldInt->UNIXtime){
        oldInt->UNIXtime = oldRecord->UNIXtime;
        trace(T_integrator, 9);
        int rtc = _log->readKey((IotaLogRecord*)oldInt); 
        trace(T_integrator, 9, rtc);
    }

    if(newRecord->UNIXtime != newInt->UNIXtime){
        newInt->UNIXtime = newRecord->UNIXtime;
        trace(T_integrator, 10);
        int rtc = _log->readKey((IotaLogRecord*)newInt);
        trace(T_integrator, 10, rtc);
    }

    trace(T_integrator, 11);

    if(method == '+'){
        trace(T_integrator, 12,1);
        return (newInt->sumPositive - oldInt->sumPositive) / elapsedHours;
    }
    else if(method == '-'){
        trace(T_integrator, 12,2);
        return (newInt->sumNegative - oldInt->sumNegative) / elapsedHours;
    }
    else if(method == 'N'){
        trace(T_integrator, 12,3);
        return (newInt->sumNegative - oldInt->sumNegative + newInt->sumPositive - oldInt->sumPositive) / elapsedHours;
    }
    trace(T_integrator, 13);
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