#include "iotawatt.h"
 
    // The integrator Service is created at startup to synchronize the 
    // integration log with the datalog, including [re]creating.
    // When the log catches up to the datalog, the datalog Service
    // takes over with direct calls to create new entries at the
    // same time as datalog records, eliminating race conditions. 

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

uint32_t integrator::handle_initialize_s(){
    trace(T_integrator,10);

    if( ! Current_log.isOpen()){
        return UTCtime() + 5;
    }

    log("%s: Started", _id);

    // Insure /iotawatt/integrations exists.

    char *dir = charstar(F(IOTA_INTEGRATIONS_DIR));

    if( ! SD.exists(dir)){
        if(!SD.mkdir(dir)){
            log("%s: could not create integration directory.", _id);
            delete[] dir;
            return 0;
        } 
    }

    // Open the integration log

    trace(T_integrator,10);
    String filepath(dir);
    delete[] dir;
    filepath += '/';
    filepath += _name;
    filepath += ".log";
    _log = new IotaLog(sizeof(intRecord), 5, 366, 128);
    trace(T_integrator,10);
    if(_log->begin(filepath.c_str())){
        log("%s: Couldn't open integration file %s.", _id, filepath.c_str());
        delete _log;
        return 0;
    }
    
    uint32_t lastWrite = MAX(_log->lastKey(), Current_log.lastKey() - (1800 * 24 * 1));

    // Initialize the intRecord;

    _intRec.UNIXtime = lastWrite;
    _intRec.serial = 0;
    _intRec.sumPositive = 0.0;

    if(_log->lastKey()){
        _log->readKey((IotaLogRecord*)&_intRec);
        log("%s: Last log entry %s", _id, localDateString(_log->lastKey()).c_str());
    }
    else {
        log("%s: New log starting %s", _id, localDateString(lastWrite).c_str());
    }

    _log->writeCache(true);
    _state = integrate_s;
    return 1;
}

uint32_t integrator::handle_integrate_s(){
    static uint32_t runus = 0;
    static uint32_t reads = 0;
    static uint32_t writes = 0;
    static int runCount = 0;
    uint32_t startus = micros();
    
    // While data is available

    while(Current_log.lastKey() >= _intRec.UNIXtime + _interval){

        if(!_oldRec){
            _oldRec = new IotaLogRecord;
            _newRec = new IotaLogRecord;
            _newRec->UNIXtime = _intRec.UNIXtime;
            Current_log.readKey(_newRec);
            reads++;
        }

        if(_newRec->UNIXtime < _intRec.UNIXtime + _interval){
            IotaLogRecord *swapRec = _oldRec;
            _oldRec = _newRec;
            _newRec = swapRec;
            _newRec->UNIXtime = _oldRec->UNIXtime + _interval;
            _newRec->serial = _oldRec->serial;
            Current_log.readNext(_newRec);
            reads++;
        }

        // If this record is within interval, add to integral.

        if(_newRec->UNIXtime <= _intRec.UNIXtime + _interval){
            double elapsed = _newRec->logHours - _oldRec->logHours;
            if(elapsed == elapsed && elapsed > 0){
                double value = _script->run(_oldRec, _newRec, "Wh");
                if(value > 0){
                    _intRec.sumPositive += value;
                }
            }
        }

        // If integration complete for this interval, log it.

        if(_newRec->UNIXtime >= _intRec.UNIXtime + _interval){
            _intRec.UNIXtime += _interval;
            _log->write((IotaLogRecord *)&_intRec);
            writes++;
        }

        if((micros() + 2500) >= bingoTime || reads > 500){
            runCount++;
            runus += micros() - startus;
            if(runCount >= 100){
                runCount = 0;
                runus = 0;
                reads = 0;
                writes = 0;
            }
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
            if(value > 0){
                _intRec.sumPositive += value;
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

double integrator::run(IotaLogRecord *oldRecord, IotaLogRecord *newRecord, units Units){
    trace(T_integrator, 0);
    double elapsedHours = newRecord->logHours - oldRecord->logHours;
    if( ! _synchronized || elapsedHours == 0 || oldRecord->UNIXtime < _log->firstKey()){
        return 0;
    }
    trace(T_integrator, 1);

        // See if we already have either record (very likely).
        // If so, move to correct position.

    if(oldRecord->UNIXtime == newInt->UNIXtime || newRecord->UNIXtime == oldInt->UNIXtime){
        trace(T_integrator, 1);
        intRecord *swap = oldInt;
        oldInt = newInt;
        newInt = swap;
    }

        // Now read if necessary.

    if(oldRecord->UNIXtime != oldInt->UNIXtime){
        oldInt->UNIXtime = oldRecord->UNIXtime;
        trace(T_integrator, 2);
        int rtc = _log->readKey((IotaLogRecord*)oldInt); 
        trace(T_integrator, 2, rtc);
    }

    if(newRecord->UNIXtime != newInt->UNIXtime){
        newInt->UNIXtime = newRecord->UNIXtime;
        trace(T_integrator, 3);
        int rtc = _log->readKey((IotaLogRecord*)newInt);
        trace(T_integrator, 3, rtc);
    }

    double value = newInt->sumPositive - oldInt->sumPositive;
    if(Units == Watts){
        trace(T_integrator, 4);
        return value / elapsedHours;
    }
    return value;
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