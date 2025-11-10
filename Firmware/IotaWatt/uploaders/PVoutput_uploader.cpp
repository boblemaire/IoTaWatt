#include "IotaWatt.h"
#include "PVoutput_uploader.h"

/******************************************************************************************************
*    PVoutput server class, modeled after framework by Brendon Costa and updated to
*    use newer facilities in IoTaWatt.  Thanks to Brendon for pioneering work,
*    help sorting out the specifications, and configuration code.
******************************************************************************************************/

// null pointer until configured (or if deleted);

PVoutput_uploader *PVoutput = nullptr;

const char P_getstatus[] PROGMEM = "getstatus.jsp";
const char P_getsystem[] PROGMEM = "getsystem.jsp";
const char P_addbatchstatus[] PROGMEM = "addbatchstatus.jsp";

    // This is the worm hole that the scheduler uses to get into the class state machine.
    // It invokes the tick method of the class.
    // On return, it checks for no-schedule return and invokes the stop() method. 

uint32_t PVoutputTick(struct serviceBlock* serviceBlock) {
    trace(T_PVoutput,0);
    PVoutput_uploader *pvoutput = (PVoutput_uploader*)serviceBlock->serviceParm;
    if(pvoutput){
        trace(T_PVoutput,1);
        uint32_t reschedule = pvoutput->tick(serviceBlock);
        trace(T_PVoutput,2);
        if(reschedule){
           return reschedule; 
        } else {
            log("PVoutput: Stopped.");
            if(pvoutput) pvoutput->stop();
        }
        trace(T_PVoutput,3);
    }
    return 0;
}

        // stop() - At the first opportunity, enter _state = stopped and idle.

void PVoutput_uploader::stop(){
    _stop = true;
    _restart = false;
}

        // restart() - At the first opportunity, restart the state sequence from the beginning (getSystemService)

void PVoutput_uploader::restart(){
    if(_state == stopped){
        _restart = true;
    } else {
        _stop = false;
        _restart = false;
    }
}

        // end() - At the first opportunity, delete this instance of PVoutput_uploader

void PVoutput_uploader::end(){
    _end = true;
    if(!_started){
        delete this;
    }
}

        // Get the current status as a Json object.

void PVoutput_uploader::getStatusJson(JsonObject& status){
    status.set(F("id"), _id);
    status.set(F("status"), (_started && _state != stopped) ? "running" : "stopped");
    status.set(F("lastpost"),local2UTC(_lastPostTime));
    if(_statusMessage){
        status.set(F("message"), _statusMessage);
    }
}

//********************************************************************************************************************
//          
//                                 TTTTT   III    CCC   K   K   
//                                   T      I    C   C  K  K                   
//                                   T      I    C      K K                  
//                                   T      I    C   C  K  K                  
//                                   T     III    CCC   K   K
// 
//******************************************************************************************************************/
uint32_t PVoutput_uploader::tick(struct serviceBlock* serviceBlock){
    trace(T_PVoutput,0);
    switch (_state) {
        case initialize:            {return handle_initialize_s();}
        case getSystemService:      {return tickGetSystemService();}
        case checkSystemService:    {return tickCheckSystemService();}
        case getStatus:             {return tickGetStatus();}
        case gotStatus:             {return tickGotStatus();}
        case uploadStatus:          {return tickUploadStatus();}
        case checkUploadStatus:     {return tickCheckUploadStatus();}

        case HTTPpost:              {return handle_HTTPpost_s();}
        case HTTPwait:              {return handle_HTTPwait_s();}
        case limitWait:             {return tickLimitWait();}
        case stopped:               {return handle_stopped_s();}
        default:
            log("%s: Unrecognize state, stopping", _id);
            return 0;
    }
}

uint32_t PVoutput_uploader::handle_stopped_s(){
    if(_end){
        delete_uploader(_id);
        delete this;
        return 0;
    }
    if(_restart){
        _restart = false;
        _stop = false;
        _state = getSystemService;
        return 1; 
    }
    if(_stop){
        return UTCtime() + 1;
    }
    _state = uploadStatus;
    return 1;
    
}

uint32_t PVoutput_uploader::handle_initialize_s(){
    trace(T_PVoutput,10);
    if( ! History_log.isOpen()){
        return UTCtime() + 10;
    }
    _id = charstar(F("PVoutput"));
    log("%s: started", _id);
    _started = true;
    _state = getSystemService;
    return 1;
}

uint32_t PVoutput_uploader::tickGetSystemService(){
    trace(T_PVoutput,20);
    reqData.print(F("donations=1"));
    HTTPPost(FPSTR(P_getsystem), checkSystemService);
    return 1;
}

uint32_t PVoutput_uploader::tickCheckSystemService(){
    trace(T_PVoutput,25);
    switch (_HTTPresponse) {
        default:{
            log("%s: Unrecognized HTTP completion, getSystemService %.40s", _id, response->peek(40).c_str());
            delete[] _statusMessage;
            _statusMessage = charstar(F("getservice failed, HTTPcode: "), String(request->responseHTTPcode()).c_str());
            _state = getSystemService;
            return UTCtime() + 60;
        }
        case OK: {
            trace(T_PVoutput,30);
            _interval = response->parsel(0,15) * 60;
            if(response->parsel(2,0)){
                _donator = true;
            }
            log("%s: System %s, interval %d%s  ", _id, response->parseString(0,0).c_str(), _interval/60, _donator ? ", donator mode" : ", freeload mode");
            delete response;
            response = nullptr;
            _state = getStatus;
            return 1;
        }
        case LOAD_IN_PROGRESS:
        case RATE_LIMIT:
        case HTTP_FAILURE: {
            delete[] _statusMessage;
            _statusMessage = charstar(F("getservice failed, HTTPcode: "), String(request->responseHTTPcode()).c_str());
            _state = getSystemService;
            return UTCtime() + 1;
        }
    }
}

uint32_t PVoutput_uploader::tickGetStatus(){
    trace(T_PVoutput,70);
    HTTPPost(FPSTR(P_getstatus), gotStatus);
    return 1;
}

uint32_t PVoutput_uploader::tickGotStatus(){
    trace(T_PVoutput,75);
    switch (_HTTPresponse) {
        default: {
            delete[] _statusMessage;
            _statusMessage = charstar(F("getstatus failed: "), response->peek(40).c_str());
            log("%s: %s", _id, _statusMessage);
            _state = getSystemService;
            return UTCtime() + 3600;
        }
        case LOAD_IN_PROGRESS:
        case RATE_LIMIT:
        case HTTP_FAILURE: {
            delete[] _statusMessage;
            _statusMessage = charstar(F("getstatus failed, HTTPcode: "), String(request->responseHTTPcode()).c_str());
            _state = getStatus; 
            return UTCtime() + 2;
        }   
        case NO_STATUS: {
            trace(T_PVoutput,76);
            _lastPostTime = 0;
            break;
        }
        case OK: {
            trace(T_PVoutput,77); 
            _lastPostTime = response->parseDate(0,0) + response->parseTime(0,1);
            break;
        } 
    }

    trace(T_PVoutput,78);
    uint32_t lookback = localTime() - UNIX_DAY * _donator ? PV_DONATOR_STATUS_DAYS : PV_DEFAULT_STATUS_DAYS;
    lookback -= lookback % UNIX_DAY;
    if(_lastPostTime < lookback || _reload){
        _lastPostTime = lookback;
    }
    if(UTC2Local(History_log.firstKey()) > _lastPostTime){
        _lastPostTime = UTC2Local(History_log.firstKey()) + _interval;
        _lastPostTime -= _lastPostTime % _interval;
    }
    if(_lastPostTime < _beginPosting){
        _lastPostTime = _beginPosting;
    }

    trace(T_PVoutput,78);
    if(_reload){
        log("%s: Reload status beginning %s", _id, datef(_lastPostTime + _interval).c_str());
    } else {
        log("%s: Start status beginning %s", _id, datef(_lastPostTime + _interval).c_str());
    }
    
    trace(T_PVoutput,79);
    delete response;
    response = nullptr;
    reqData.flush();
    _reqEntries = 0;
    _lastReqTime = _lastPostTime;
    _state = uploadStatus;
    return 1;
    
}
        // uploadStatus()
        // We have the (local) [date/]time of the last post in _lastPostTime.
        // This gets tricky because the datalog is in UTC and we are posting in local time.
        // Another wrinkle is that we use the power of the interval after the timestamp rather than before
        // as in other services, so the posting will be delayed by interval. Another way to say that is
        // we subtract interval to get the timestamp of each status.
        // Report maximum statuses unless current where we revert to single writes.

uint32_t PVoutput_uploader::tickUploadStatus(){
    trace(T_PVoutput,80);

            // This is the basic heartbeat of the service,
            // and also a clean place to stop it,
            // so check for a stop, restart, or end request.

    if(_stop || _end){
        trace(T_PVoutput,81);
        delete oldRecord;
        oldRecord = nullptr;
        delete newRecord;
        newRecord = nullptr;
        delete request;
        request = nullptr;
        delete response;
        response = nullptr;
        _state = stopped;
        return 1;
    }

    if(_restart){
        trace(T_PVoutput,82);
        _restart = false;
        _state = getSystemService;
        return 1;
    }
    
            // Insure base energy values for the current day are set.

    if(_baseTime != (_lastReqTime - _lastReqTime % UNIX_DAY)){
        trace(T_PVoutput,83);
        if( ! oldRecord){
            oldRecord = new IotaLogRecord;
        }
        oldRecord->UNIXtime = local2UTC(_lastReqTime - _lastReqTime % UNIX_DAY);
        History_log.readKey(oldRecord);
        Script* script = _outputs->first();
        while(script){
            if(strcmp(script->name(),"generation") == 0){
                _baseGeneration = script->run(nullptr, oldRecord, Wh);
            }
            else if(strcmp(script->name(),"consumption") == 0){
                _baseConsumption = script->run(nullptr, oldRecord, Wh);
            }
            script = script->next();
        }
        _baseTime = _lastReqTime - _lastReqTime % UNIX_DAY;
    }
  
            // Write buffer if not empty and any one of the following is true:
            // Maximum enries per write
            // reqData larger than PV_REQDATA_LIMIT
            // Last entry is current
            // Last entry is last entry for a day
            
    if(_reqEntries &&
      (_reqEntries >= (_donator ? PV_DONATOR_STATUS_LIMIT : PV_DEFAULT_STATUS_LIMIT) ||
      (reqData.available() >= PV_REQDATA_LIMIT) ||
      (_lastReqTime + _interval) > UTC2Local(History_log.lastKey()) ||
      (_lastReqTime % UNIX_DAY) == 0)){
        delete oldRecord;
        oldRecord = nullptr;
        delete newRecord;
        newRecord = nullptr;  
        HTTPPost(FPSTR(P_addbatchstatus), checkUploadStatus);
        _reqEntries = 0;
        return 1;
    }

            // If up-to-date, just return.

    if((_lastReqTime + _interval) > UTC2Local(History_log.lastKey())){
        trace(T_PVoutput,85);
        delete oldRecord;
        oldRecord = nullptr;
        delete newRecord;
        newRecord = nullptr;
        delete request;
        request = nullptr;
        delete response;
        response = nullptr;
        delete _POSTrequest;
        reqData.flush();
        _POSTrequest = nullptr;
        return UTCtime() + 1;
    }

            // Get the bracketing history log records (careful to convert to UTC)

    if( ! oldRecord){
        oldRecord = new IotaLogRecord;
    }
    if( ! newRecord) {
        newRecord = new IotaLogRecord;
    }
    if(oldRecord->UNIXtime != local2UTC(_lastReqTime)){
        trace(T_PVoutput,86);
        if(newRecord->UNIXtime == local2UTC(_lastReqTime)){
            IotaLogRecord* temp = oldRecord;
            oldRecord = newRecord;
            newRecord = temp;
        } else {
            oldRecord->UNIXtime = local2UTC(_lastReqTime);
            History_log.readKey(oldRecord);
        }
    }
    newRecord->UNIXtime = local2UTC(_lastReqTime + _interval);
    History_log.readKey(newRecord);

            // See if there was any measurement during this interval
            // Skip ahead if not.

    trace(T_PVoutput,87);
    double elapsedHours = newRecord->logHours - oldRecord->logHours;
    if(elapsedHours == 0.0){
        _lastReqTime += _interval;
        if(_reqEntries == 0){
            _lastPostTime = _lastReqTime;
        }
        return 1;
    }

            // Got all the ingredients,
            // prep reqData for new status.

    trace(T_PVoutput,87);
    if(_reqEntries++ == 0){
        reqData.print("data=");
    } else {
        reqData.print(';');
    }
    reqData.printf_P("%s,%s", datef(_lastReqTime,"YYYYMMDD").c_str(), datef(_lastReqTime, "hh:mm").c_str());

            // run the scripts to collect the data.

    double powerGeneration(-1);
    double powerConsumption(-1);
    double voltage(-1);
    double energyConsumption(-1);
    double energyGeneration(-1);
    double extended[6];
    int    extendedPrecision[6];
    int    lastExtended(-1); 
    bool   haveExtended[6]{false,false,false,false,false,false};

    Script* script = _outputs->first();
    trace(T_PVoutput,88);
    while(script){
        if(strcmp(script->name(),"generation") == 0){
            energyGeneration = script->run(nullptr, newRecord, Wh) - _baseGeneration;
            powerGeneration = script->run(oldRecord, newRecord, Watts);
        }
        else if(strcmp(script->name(),"consumption") == 0){
            energyConsumption = script->run(nullptr, newRecord, Wh) - _baseConsumption;
            powerConsumption = script->run(oldRecord, newRecord, Watts);  
        }
        else if(strcmp(script->name(),"voltage") == 0){
            voltage = script->run(oldRecord, newRecord, Volts);    
        }
        else if(strstr(script->name(),"extended_") == script->name()){
            long ndx = strtol(script->name()+9,nullptr,10) - 1;
            if(ndx >= 0 && ndx <=5){
                if(ndx > lastExtended){
                    lastExtended = ndx;
                }
                haveExtended[ndx] = true;
                extended[ndx] = script->run(oldRecord, newRecord);
                extendedPrecision[ndx] = script->precision();
            }
        }
        script = script->next();
    }

            // Add the collected data to the status

    trace(T_PVoutput,88);
    if(powerGeneration >= 0){
        reqData.printf(",%.0f,%.0f", energyGeneration, powerGeneration);
    } else {
        reqData.print(",,");
    }
    if(powerConsumption >= 0){
        reqData.printf(",%.0f,%.0f", energyConsumption, powerConsumption);
    } else {
        reqData.print(",,");
    }
    if(voltage >= 0){
        reqData.printf(",,%.1f", voltage);
    } else {
        reqData.print(",,");
    }
    if(_donator && lastExtended >= 0){
        for(int ndx=0; ndx<=lastExtended; ndx++){
            reqData.print(',');
            if(haveExtended[ndx]){
                reqData.printf("%.*f", extendedPrecision[ndx], extended[ndx]);
            }
        }
    }

            // Increment interval and do it again.

    trace(T_PVoutput,89);
    _lastReqTime += _interval;
    return 1;
}

uint32_t PVoutput_uploader::tickCheckUploadStatus(){
    trace(T_PVoutput,90);
    switch (_HTTPresponse) {
        default:{
            delete[] _statusMessage;
            _statusMessage = charstar(F("Unrecognized HTTP completion, upload "), response->peek(80).c_str());
            log("%s: %s", _id, _statusMessage);
            _state = getSystemService;
            return UTCtime() + 1800;
        }

        case DATE_TOO_OLD:
        case DATE_IN_FUTURE:
        case OK: 
            trace(T_PVoutput,91);
            _lastPostTime = _lastReqTime;   
            _state = uploadStatus;
            return 1;
        
        case LOAD_IN_PROGRESS: 
            delete[] _statusMessage;
            _statusMessage = charstar(F("Load in progress?"));
            _state = uploadStatus;
            return UTCtime() + 15;

        case RATE_LIMIT:  
        case HTTP_FAILURE:   
            trace(T_PVoutput,92);
            if(request->responseHTTPcode() == _errorCode){
                if(++_errorCount == 10){
                    log("%s: %s", _id, _statusMessage);
                }
            }
            else {
                _errorCode = request->responseHTTPcode();
                delete[] _statusMessage;
                _statusMessage = charstar(F("HTTP completion, upload "), String(_errorCode).c_str());
                _errorCount = 0;
            }
            _state = uploadStatus;
            return UTCtime() + 15;

    }
}

//********************************************************************************************************************
//
//                      H   H   TTTTT   TTTTT   PPPP
//                      H   H     T       T     P   P
//                      HHHHH     T       T     PPPP
//                      H   H     T       T     P
//                      H   H     T       T     P

//*********************************************************************************************************************

const char reqHeaderApikey[] PROGMEM = "X-Pvoutput-Apikey";
const char reqHeaderRate[] PROGMEM = "X-Rate-Limit";
const char reqHeaderSystemId[] PROGMEM = "X-Pvoutput-SystemId";
const char respHeaderRemaining[] PROGMEM = "X-Rate-Limit-Remaining";
const char respHeaderLimit[] PROGMEM = "X-Rate-Limit-Limit";
const char respHeaderReset[] PROGMEM = "X-Rate-Limit-Reset";
const char reqHeaderContentType[] PROGMEM = "Content-type";

//void PVoutput_uploader::HTTPPost(const __FlashStringHelper *URI, states completionState, const __FlashStringHelper *contentType){
    
void PVoutput_uploader::HTTPPost(const __FlashStringHelper *URI, states completionState, const char* contentType){
    trace(T_PVoutput,100);
    delete _POSTrequest;
    _POSTrequest = new POSTrequest;
    _POSTrequest->URI = charstar(URI);
    if(contentType){
        _POSTrequest->contentType = charstar(contentType);
    } else {
        _POSTrequest->contentType = charstar(F("application/x-www-form-urlencoded"));
    }
    _POSTrequest->completionState = completionState;
    _state = HTTPpost;
}

uint32_t PVoutput_uploader::handle_HTTPpost_s(){
    trace(T_PVoutput,110);
    if( ! WiFi.isConnected()){
        return UTCtime() + 1;
    }

    _HTTPtoken = HTTPreserve(T_influx);
    if( ! _HTTPtoken){
        return 15;
    }

    if( ! request){
        request = new asyncHTTPrequest;
    }
    request->setTimeout(3);
    request->setDebug(false);
    {
        String URL;
        if(HTTPSproxy){
            URL = HTTPSproxy;
        }
        else {
            URL = "HTTP://pvoutput.org";
        }
        URL = URL + "/service/r2/" + _POSTrequest->URI;

        if( ! request->open("POST", URL.c_str())){
            HTTPrelease(_HTTPtoken);
            return UTCtime() + 10;
        }
    }
    if(HTTPSproxy){
        request->setReqHeader(F("X-proxypass"),  "HTTPS://pvoutput.org");
    }
    request->setReqHeader(FPSTR(reqHeaderApikey), _apiKey);
    request->setReqHeader(FPSTR(reqHeaderSystemId), _systemID);
    request->setReqHeader(FPSTR(reqHeaderRate), "1");
    if(_POSTrequest->contentType){
        request->setReqHeader(FPSTR(reqHeaderContentType), _POSTrequest->contentType);
    }
    if(reqData.available()){
        request->send(&reqData, reqData.available());
    } else {
        request->send();
    }
    _state = HTTPwait;
    return 1;
}

uint32_t PVoutput_uploader::handle_HTTPwait_s(){
    trace(T_PVoutput,120);
    if(request->readyState() != 4){
        return 1;
    }
    HTTPrelease(_HTTPtoken);
    _state = _POSTrequest->completionState;
    delete _POSTrequest;
    _POSTrequest = nullptr;
    trace(T_PVoutput,121);

            // Get the flow control headers from PVoutput

    trace(T_PVoutput,120);
    if(request->respHeaderExists(FPSTR(respHeaderLimit))){
        _rateLimitLimit = strtol(request->respHeaderValue(FPSTR(respHeaderLimit)),nullptr,10);
    }
    if(request->respHeaderExists(FPSTR(respHeaderRemaining))){
        _rateLimitRemaining = strtol(request->respHeaderValue(FPSTR(respHeaderRemaining)),nullptr,10);
    }
    if(request->respHeaderExists(FPSTR(respHeaderReset))){
        _rateLimitReset = strtoul(request->respHeaderValue(FPSTR(respHeaderReset)),nullptr,10);
    }
    trace(T_PVoutput,122);

            // If communication failure,
            // Set completion 

    trace(T_PVoutput,120);
    if(request->responseHTTPcode() < 0){
        _HTTPresponse = HTTP_FAILURE;
        return UTCtime() + 3;
    }
    _errorCode = 0;
    _errorCount = 0;
    delete[] _statusMessage;
    _statusMessage = nullptr;
    trace(T_PVoutput,122);

            // Capture response.

    trace(T_PVoutput,121);
    delete response;
    response = new PVresponse(request); 
    trace(T_PVoutput,122);

            // Interpret response code.

    if(request->responseHTTPcode() == 200){
        trace(T_PVoutput,122);
        _HTTPresponse = OK;
        return 1;
    }
    
    if(request->responseHTTPcode() == 403 && response->contains(F("Exceeded")) && response->contains(F("requests per hour"))){
        _statusMessage = charstar(F("Transaction Rate-Limit exceeded.  Resume at "), datef(UTC2Local(_rateLimitReset), "hh:mm").c_str());
        log("%s: %s", _id, _statusMessage);
        trace(T_PVoutput,123);
        _HTTPresponse = RATE_LIMIT;
        _resumeState = _state;
        _state = limitWait;        
        return 1;
    }

    if(request->responseHTTPcode() == 400){
        trace(T_PVoutput,124);
        if(response->contains(F("Load in progress"))){
            _HTTPresponse = LOAD_IN_PROGRESS;
            return UTCtime() + 30;
        }
        if(response->contains(F("Date is in the future"))){
            _HTTPresponse = DATE_IN_FUTURE;
            return 1;
        }
        if(response->contains(F("Date is older than"))){
            _HTTPresponse = DATE_TOO_OLD;
            return 1;
        }
        if(response->contains(F("Moon powered"))){
            _HTTPresponse = MOON_POWERED;
            return 1;
        }
        if(response->contains(F("No status"))){
            _HTTPresponse = NO_STATUS;
            return 1;
        }
    }

                // Unrecognized response.

    Serial.println(response->peek(response->length()));
    _HTTPresponse = UNRECOGNIZED;
    return 1;
}

uint32_t PVoutput_uploader::tickLimitWait(){
    if(_stop || _end){
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _state = uploadStatus;
        return 1;
    }
    if(UTCtime() > _rateLimitReset){
        trace(T_PVoutput,131);
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _state = _resumeState;
        return 1;
    }
    trace(T_PVoutput,130);

                // Wait a second.

    return 1000;
}

//********************************************************************************************************************
//
//               CCC     OOO    N   N   FFFFF   III    GGG
//              C   C   O   O   NN  N   F        I    G   
//              C       O   O   N N N   FFF      I    G  GG
//              C   C   O   O   N  NN   F        I    G   G
//               CCC     OOO    N   N   F       III    GGG
//
//********************************************************************************************************************
bool PVoutput_uploader::config(const char* configObj){
    DynamicJsonBuffer Json;
    JsonObject& config = Json.parseObject(configObj);
    if( ! config.success()){
        log("%s: Json parse failed.", _id);
        return false; 
    }
    trace(T_PVoutput,200);
    if(config["type"].as<String>() != "pvoutput"){
        return false;
    }
    trace(T_PVoutput,201);
    if(_revision == config["revision"]){
        return true;
    }
    _revision = config["revision"];
    _reload = config["reload"].as<bool>();
    if(_reload && _started){
        _restart = true;
    }
    _stop = config["stop"].as<bool>();
    trace(T_PVoutput,202);
    _beginPosting = config.get<uint32_t>("begdate");
    delete[] _apiKey;
    _apiKey = charstar(config["apikey"].as<char*>());
    delete[] _systemID;
    _systemID = charstar(config["systemid"].as<char*>());
    trace(T_PVoutput,203);
    delete _outputs;
    JsonVariant var = config["outputs"];
    if(var.success()){
        trace(T_PVoutput,204);
        _outputs = new ScriptSet(var.as<JsonArray>());
    }
    trace(T_PVoutput,205);
    if( ! _started) {
        trace(T_PVoutput,206);
        serviceBlock *sb = NewService(PVoutputTick, T_PVoutput);
        sb->serviceParm = (void *)this;
        _started = true;
    }
    trace(T_PVoutput,207);
    return true; 
}

//******************************************************************************************************************
//
//              PPPP    V   V   RRRR    EEEEE    SSS    PPPP     OOO    N   N    SSS    EEEEE
//              P   P   V   V   R   R   E       S       P   P   O   O   NN  N   S       E
//              PPPP    V   V   RRRR    EEEE     SSS    PPPP    O   O   N N N    SSS    EEEE
//              P        V V    R  R    E           S   P       O   O   N  NN       S   E
//              P         V     R   R   EEEEE    SSS    P        OOO    N   N    SSS    EEEEE
//
//******************************************************************************************************************

PVresponse::PVresponse(asyncHTTPrequest* request){
    _response = new char[request->available()+1];
    _response[request->available()] = 0;
    request->responseRead((uint8_t*)_response, request->available());
}

PVresponse::~PVresponse(){
    delete[] _response;
}

size_t  PVresponse::sections(){
    char* pointer = _response;
    if(*pointer == 0) return 0;
    size_t section = 0;
    while(*pointer){
        if(*(pointer++) == ';') section++;
    }
    return section + 1;
}

size_t PVresponse::items(int section){
    char* pointer = parsePointer(section, 0);
    if( ! pointer || *pointer == 0) return 0;
    size_t item = 0;
    while(*pointer){
        if(*(pointer) == ';') break;
        if(*(pointer++) == ',') item++;
    }
    return item + 1;
}

char* PVresponse::parsePointer(int section, int item){
    char* pointer = _response;
    while(section){
        if( ! *pointer) return nullptr;
        if(*(pointer++) == ';') section--;
    }
    while(item){
        if( ! *pointer) return nullptr;
        if(*(pointer++) == ',') item--;
    }
    return pointer;
}

int32_t     PVresponse::parsel(int section, int item){
    char* pointer = parsePointer(section, item);
    if( ! pointer) return 0;
    return strtol(pointer, nullptr, 10);
}

uint32_t    PVresponse::parseul(int section, int item){
    char* pointer = parsePointer(section, item);
    if( ! pointer) return 0;
    return strtoul(pointer, nullptr, 10);
}

String      PVresponse::parseString(int section, int item){
    char* pointer = parsePointer(section, item);
    String str;
    while(*pointer && *pointer != ',' && *pointer != ';'){
        str += *(pointer++);
    }
    return str;
}

uint32_t    PVresponse::parseDate(int section, int item){
    char* pointer = parsePointer(section, item);
    return YYYYMMDD2Unixtime(parseString(section, item).c_str());
}

uint32_t    PVresponse::parseTime(int section, int item){
    char* pointer = parsePointer(section, item);
    return HHMMSS2daytime(parseString(section, item).c_str(), "%2d:%2d");
}

void    PVresponse::print(){
    Serial.println(_response);
}

String  PVresponse::peek(int length, int offset){
    int len = strlen(_response);
    if(offset >= len) return String();
    if((length + offset) > len){
        length = len - offset;
    }
    char* str = new char[length+1];
    memcpy(str, _response+offset, length);
    str[length] = 0;
    return String(str);
}

size_t  PVresponse::length(){
    return strlen(_response);
}

bool    PVresponse::contains(const char* string){
    return strstr(_response, string);
}

bool    PVresponse::contains(const __FlashStringHelper *str){
    return strstr_P(_response, (PGM_P)str);
}
