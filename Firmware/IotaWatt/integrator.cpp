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
    delete _this;
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

    if(!Current_log.isOpen()){
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
    _log = new IotaLog(sizeof(intRecord), 5, 366, 32);
    trace(T_integrator,10);
    if(_log->begin(filepath.c_str())){
        log("%s: Couldn't open integration file %s.", _id, filepath.c_str());
        delete _log;
        return 0;
    }

    uint32_t lastWrite = MAX(_log->lastKey(), Current_log.lastKey() - (3600 * 40));

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

        IotaLogRecord *swapRec = _oldRec;
        _oldRec = _newRec;
        _newRec = swapRec;
        _newRec->UNIXtime = _oldRec->UNIXtime;
        _newRec->serial = _oldRec->serial;
        Current_log.readNext(_newRec);
        reads++;

        // If this record is within interval, add integral.

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
                Serial.printf("integrations %d, avg time: %u us, %u reads, %u writes\n", runCount, runus / runCount, reads, writes);
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
    return UTCtime() + 5;
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
    delete _log;
    _state = end_s;
}

uint32_t integrator::handle_end_s(){
    log("%s: ended. Last entry %s", _id, localDateString(_intRec.UNIXtime).c_str());
    return 0;
}

double integrator::run(IotaLogRecord *oldRecord, IotaLogRecord *newRecord, units Units){
    trace(T_integrator, 0);
    double elapsedHours = newRecord->logHours - oldRecord->logHours;
    if(elapsedHours == 0){
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
        int rtc = _log->readKey((IotaLogRecord*)oldInt);
        trace(T_integrator, 2, rtc);
    }

    if(newRecord->UNIXtime != newInt->UNIXtime){
        newInt->UNIXtime = newRecord->UNIXtime;
        int rtc = _log->readKey((IotaLogRecord*)newInt);
        trace(T_integrator, 2, rtc);
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