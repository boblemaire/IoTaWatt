#include "influxDB_v2_uploader.h"

influxDB_v2_uploader *influxDB_v2 = nullptr;

void influxDB_v2_uploader::getStatusJson(JsonObject& status)
{
    trace(T_influx2,110);    
    if(_state == stopped){
        status.set(F("status"), _stop ? "stopped" : "starting");
    } else {
        status.set(F("status"), _stop ? "stopping" : "running");
    }
    status.set(F("lastpost"), _lastSent);
    trace(T_influx2,110);
}

void influxDB_v2_uploader::end()
{
    trace(T_influx2,120);    
    _stop = true;
    _end = true;
}

    // This is the worm hole that the scheduler uses to get into the class state machine.
    // It invokes the tick method of the class.
    // On return, it checks for no-schedule return and invokes the stop() method. 

uint32_t influxDB_v2_tick(struct serviceBlock* serviceBlock) {
    trace(T_influx2,0);
    if(influxDB_v2){
        trace(T_influx2,1);
        uint32_t reschedule = influxDB_v2->tick(serviceBlock);
        trace(T_influx2,1);
        if(reschedule){
           return reschedule;
        } 
        log("influxDB_v2: Stopped.");
    }
    trace(T_influx2,0);
    return 0;
}

uint32_t influxDB_v2_uploader::tick(struct serviceBlock *serviceBlock)
{
    trace(T_influx2,2,_state);
    switch (_state) {
        case initialize:
            return tickInitialize();
        case getLastPost:
            return tickGetLastSent();
        case post:              
            return tickPost();
        case sendPost:
            return tickSendPost();
        case waitPost:
            return tickWaitPost();
        case stopped:
            return tickStopped();
    }
    trace(T_influx2,3,_state);
    log("influxDB_v2: Unrecognized state, stopping;");
    influxDB_v2 = nullptr;
    delete this;
    return 0;
}

uint32_t influxDB_v2_uploader::tickStopped(){
    trace(T_influx2,5);
    if(_end){
        trace(T_influx2,6);
        influxDB_v2 = nullptr;
        delete this;
        return 0;
    }
    if(_stop){
        trace(T_influx2,7);
        return UTCtime() + 1;
    }
    trace(T_influx2,8);
    _state = post;
    return 1;
}

uint32_t influxDB_v2_uploader::tickInitialize(){
    trace(T_influx2,10);
    log("influxDB_v2: Starting");
    _state = getLastPost;
    return 1;
}

uint32_t influxDB_v2_uploader::tickGetLastSent(){
    trace(T_influx2,20);
    _lastSent = 1611551520UL;     //UTCtime();
    _lastSent -= _lastSent % _interval;
    log("influxDB_v2: Started. Begin after %s", localDateString(_lastSent).c_str());
    _state = post;
    return 1;
}

uint32_t influxDB_v2_uploader::tickPost(){
    trace(T_influx2,60);
    if(_stop){
        _state = stopped;
        return 1;
    }

    if(Current_log.lastKey() < (_lastSent + _interval * _bulkSend)){
        return UTCtime() + 1;
    }

    trace(T_influx2,60);
    if(! oldRecord){
        trace(T_influx2,61);
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent;
        Current_log.readKey(newRecord);
    }

    while(reqData.available() < _bufferLimit && newRecord->UNIXtime < Current_log.lastKey()){

        trace(T_influx2,60);
        IotaLogRecord *swap = oldRecord;
        oldRecord = newRecord;
        newRecord = swap;

        newRecord->UNIXtime = oldRecord->UNIXtime + _interval;
        Current_log.readKey(newRecord);


        // Compute the time difference between log entries.
        // If zero, don't bother.

        trace(T_influx2,62);    
        double elapsedHours = newRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
            trace(T_influx2,63);
            if((newRecord->UNIXtime + _interval) <= Current_log.lastKey()){
                return 1;
            }
            return UTCtime() + 1;
        }

        trace(T_influx2,62); 
        String lastMeasurement;
        String thisMeasurement;
        Script *script = _outputs->first();
        while(script)
        {
            trace(T_influx2,63);     
            double value = script->run(oldRecord, newRecord, elapsedHours);
            if(value == value){
                trace(T_influx2,64);   
                thisMeasurement = influx2VarStr(_measurement, script);
                if(_staticKeySet && thisMeasurement.equals(lastMeasurement)){
                    reqData.printf_P(PSTR(",%s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                } else {
                    if(lastMeasurement.length()){
                        reqData.printf(" %d\n", newRecord->UNIXtime);
                    }
                    reqData.write(thisMeasurement);
                    if(_tagSet){
                        trace(T_influx2,64);
                        influxTag* tag = _tagSet;
                        while(tag){
                            reqData.printf_P(PSTR(",%s=%s"), tag->key, varStr(tag->value, script).c_str());
                            tag = tag->next;
                        }
                    }
                    trace(T_influx2,65);
                    reqData.printf_P(PSTR(" %s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                }
            }
            lastMeasurement = thisMeasurement;
            script = script->next();
        }
        _lastPost = newRecord->UNIXtime;
        reqData.printf(" %d\n", _lastPost);

        if(millis() == nextCrossMs - 1){
            return 1;
        }
    }

    // Write the data

    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;
    _state = sendPost;
    return 1;
}

uint32_t influxDB_v2_uploader::tickSendPost(){
    trace(T_influx2,80);
    if(_stop){
        _state = stopped;
        return 1;
    }
    if( ! WiFi.isConnected()){
        return UTCtime() + 1;
    }
    _HTTPtoken = HTTPreserve(T_influx2);
    if( ! _HTTPtoken){
        return 1;
    }
    if( ! _request){
        _request = new asyncHTTPrequest;
    }
    _request->setTimeout(3);
    _request->setDebug(false);
    if(_request->debug())    {
        Serial.println(ESP.getFreeHeap()); 
        Serial.println(datef(localTime(),"hh:mm:ss"));
        Serial.println(reqData.peekString(reqData.available()));
    }
    trace(T_influx2,80);
    {
        char URL[128];
        if(_useProxyServer && HTTPSproxy){
            trace(T_influx2,81);
            size_t len = sprintf_P(URL, PSTR("%s/api/v2/write?precision=s&orgID=%s&bucket=%s"), HTTPSproxy, _orgID, _bucket);
        }
        else {
            trace(T_influx2,82);
            size_t len = sprintf_P(URL, PSTR("%s/api/v2/write?precision=s&orgID=%s&bucket=%s"), _url.build().c_str(), _orgID, _bucket);
        }
        trace(T_influx2,83);
        if( ! _request->open("POST", URL)){
            trace(T_influx2,83);
            HTTPrelease(_HTTPtoken);
            delete _request;
            _request = nullptr;
            _lastPost = _lastSent;
            _state = post;
            return UTCtime() + 1;
        }
    }
    if(_useProxyServer){
        _request->setReqHeader("X-proxypass", _url.build().c_str());
    }
    trace(T_influx2,84);
    String auth = "Token ";
    auth += _token;
    _request->setReqHeader("Authorization", auth.c_str());
    if( ! _request->send(&reqData, reqData.available())){
        trace(T_influx2,85);
        HTTPrelease(_HTTPtoken);
        delete _request;
        _request = nullptr;
        _lastPost = _lastSent;
        _state = post;
        return UTCtime() + 1;
    }
    trace(T_influx2,86);
    _state = waitPost;
    return 1; 
}

uint32_t influxDB_v2_uploader::tickWaitPost(){
    trace(T_influx2,90);
    if(_request && _request->readyState() == 4){
        HTTPrelease(_HTTPtoken);
        if(_stop){
            _state = stopped;
            return 1;
        }
        trace(T_influx2,91);
        if(_request->responseHTTPcode() != 204){
            trace(T_influx2,92);
            char msg[100];
            sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
            delete[] _statusMessage;
            _statusMessage = charstar(msg);
            delete _request;
            _request = nullptr; 
            _state = post;
            return UTCtime() + 2;
        }
        _lastSent = _lastPost; 
        _state = post;
        trace(T_influx2,9);
        return 1;
    }
    trace(T_influx2,93);
    return 1;
}

bool influxDB_v2_uploader::config(const char *jsonConfig)
{  
    trace(T_influx2,100);

    // Parse json configuration

    DynamicJsonBuffer Json;
    JsonObject& config = Json.parseObject(jsonConfig);
    if( ! config.success()){
        log("influxDB_v2: Config parse failed");
        return false;
    }

    // If config not changed, return success.

    trace(T_influx2,100);
    if(_revision == config["revision"]){
        return true;
    }
    _revision = config["revision"];

    // parse and validate url

    trace(T_influx2,100);
    if(!_url.parse(config.get<char*>("url"))){
        log("influxDB_v2: invalid URL");
        return false;
    }
    _url.query(nullptr);

    // Gather and check parameters

    trace(T_influx2,100);
    _interval = config.get<unsigned int>("postInterval");
    if(!_interval || (_interval % 5 != 0)){
        log("influxDB_v2: Invalid interval");
        return false;
    }
    _bulkSend = config.get<unsigned int>("bulksend");
    _bulkSend = constrain(_bulkSend, 1, 10);

    trace(T_influx2,101);
    delete[] _bucket;
    _bucket = charstar(config.get<char*>("bucket"));
    if(strlen(_bucket) == 0){
        log("influxDB_v2: Bucket not specified");
        return false;
    }
        
    trace(T_influx2,101);
    delete[] _orgID;
    _orgID = charstar(config.get<const char*>("orgid"));
    if(strlen(_orgID) != 16){
        log("influxDB_v2: Invalid organization ID");
        return false;
    }
    trace(T_influx2,101);
    delete[] _token;
    _token = charstar(config.get<const char*>("authtoken"));
    if(_token && strlen(_token) != 88){
        log("influxDB_v2: Invalid authorization token");
        return false;
    }
    trace(T_influx2,101);
    delete[] _measurement;
    _measurement = charstar(config.get<const char*>("measurement"));
    if( ! _measurement){
        _measurement = charstar("$name");
    }
    trace(T_influx2,102);
    delete[] _fieldKey;;
    _fieldKey = charstar(config.get<const char*>("fieldkey"));
    if( ! _fieldKey){
        _fieldKey = charstar("value");
    }
    trace(T_influx2,102);
    _stop = config.get<bool>("stop");

    // Build tagSet

    trace(T_influx2,103);
    delete _tagSet;
    _tagSet = nullptr;
    JsonArray& tagset = config["tagset"];
    _staticKeySet = true;
    if(tagset.success()){
        trace(T_influx2,103);
        for(int i=tagset.size(); i>0;){
            i--;
            influxTag* tag = new influxTag;
            tag->next = _tagSet;
            _tagSet = tag;
            tag->key = charstar(tagset[i]["key"].as<const char*>());
            tag->value = charstar(tagset[i]["value"].as<const char*>());
            if((strstr(tag->value,"$units") != nullptr) || (strstr(tag->value,"$name") != nullptr)) _staticKeySet = false;

        }
    }
    
        // Build the measurement scriptset

    trace(T_influx2,104);
    delete _outputs;
    _outputs = nullptr;
    JsonVariant var = config["outputs"];
    if(var.success()){
        trace(T_influx2,105);
        _outputs = new ScriptSet(var.as<JsonArray>());
    }
    else {
        log("influxDB_v2: No measurements.");
        return false;
    }

            // sort the measurements by measurement name
    const char *measurement = _measurement;
    _outputs->sort([this](Script* a, Script* b)->int {
        return strcmp(varStr(_measurement, a).c_str(), varStr(_measurement, b).c_str());
    });

    trace(T_influx2,105);    
    if(_state == initialize) {
        trace(T_influx2,105);
        NewService(influxDB_v2_tick, T_influx2);
    }
    trace(T_influx2,106);

    return true; 
}

String influxDB_v2_uploader::varStr(const char* in, Script* script){
  String out;
  while(*in){ 
    if(memcmp(in,"$device",7) == 0){
      out += deviceName;
      in += 7;
    }
    else if(memcmp(in,"$name",5) == 0){
      out += script->name();
      in += 5;
    }
    else if(memcmp(in,"$units",6) == 0){
      out += script->getUnits();
      in += 6;
    }
    else {
      out += *(in++);
    }
  } 
  return out;
}
