#include "influxDB_v2_uploader.h"
#include "splitstr.h"

influxDB_v2_uploader *influxDB_v2 = nullptr;

void influxDB_v2_uploader::getStatusJson(JsonObject& status)
{
    // Set status information in callers's json object.

    trace(T_influx2,110);    
    if(_state == stopped_s){
        status.set(F("status"), "stopped");
    } else {
        status.set(F("status"), "running");
    }
    status.set(F("lastpost"), _lastSent);
    if(_statusMessage){
        status.set(F("message"), _statusMessage);
    }
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
    // On return, it checks for no_schedule return and invokes the stop() method. 

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

//********************************************************************************************************************
//          
//                                 TTTTT   III    CCC   K   K   
//                                   T      I    C   C  K  K                   
//                                   T      I    C      K K                  
//                                   T      I    C   C  K  K                  
//                                   T     III    CCC   K   K
// 
//******************************************************************************************************************/
uint32_t influxDB_v2_uploader::tick(struct serviceBlock *serviceBlock)
{
    trace(T_influx2,2,_state);
    switch (_state) {
        case initialize_s:
            return tickInitialize();
        case buildLastSent_s:
            return tickBuildLastSent();
        case checkLastSent_s:
            return tickCheckLastSent();
        case buildPost_s:              
            return tickBuildPost();
        case checkPost_s:
            return tickCheckPost();
        case stopped_s:
            return tickStopped();
        case HTTPpost_s:
            return tickHTTPPost();
        case HTTPwait_s:
            return tickHTTPWait();
        case delay_s:
            return tickDelay();
    }
    trace(T_influx2,3,_state);
    log("influxDB_v2: Unrecognized state, stopping;");
    influxDB_v2 = nullptr;
    delete this;
    return 0;
}

uint32_t influxDB_v2_uploader::stop(){
    log("influxDB_v2: stopped, Last post %s", localDateString(_lastSent).c_str());
    _state = stopped_s;
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
        delete oldRecord;
        oldRecord = nullptr;
        delete newRecord;
        newRecord = nullptr;
        delete _request;
        _request = nullptr;
        reqData.flush();
        delete _url;
        _url = nullptr;
        trace(T_influx2, 7);
        delete[] _statusMessage;
        _statusMessage = nullptr;
        return UTCtime() + 1;
    }
    trace(T_influx2,8);
    _lookbackHours = 0;
    _state = buildLastSent_s;
    return 1;
}

uint32_t influxDB_v2_uploader::tickInitialize(){
    trace(T_influx2,10);
    log("influxDB_v2: Starting, bucket:%s, interval:%d, url:%s", _bucket, _interval, _url->build().c_str());
    _state = buildLastSent_s;
    return 1;
}

uint32_t influxDB_v2_uploader::tickBuildLastSent(){
    trace(T_influx2,20);

    // Set range for lookback.
    // Initially 1 hour, then x10 each successive iteration.

    uint32_t rangeFloor = MAX(Current_log.firstKey(), _uploadStartDate);
    uint32_t timenow = UTCtime();
    timenow -= timenow % _interval;
    uint32_t rangeStop = MAX(timenow - (_lookbackHours * 3600), rangeFloor);
    _lookbackHours = _lookbackHours ? (_lookbackHours * 10) : 1;
    uint32_t rangeBegin = MAX(timenow - (_lookbackHours * 3600), rangeFloor);
    
    // If entire range searched (with no success),
    // Set to start at _uploadStartDate or now()
    
    if(rangeBegin >= rangeStop){
        _lastSent = timenow;
        if(_uploadStartDate){
            _lastSent = rangeFloor;
        }
        _lastSent -= _lastSent % _interval;
        log("influxDB_v2: Start posting %s", localDateString(_lastSent + _interval).c_str());
        _state = buildPost_s;
        return 1;
    }

    // Build a flux query to find last record in set of measurements.
    
    reqData.flush();
    reqData.printf_P(PSTR("from(bucket: \"%s\")\n  |> range(start: %d, stop: %d)\n  |> "), _bucket, rangeBegin, rangeStop);
    reqData.print(F("filter(fn: (r) => "));
    trace(T_influx2,20);
    if(_tagSet){
        trace(T_influx2,21);    
        influxTag* tag = _tagSet;
        while(tag){
            String tagValue = tag->value;
            if( ! (strstr(tag->value,"$name") || strstr(tag->value,"$units"))){
                reqData.printf_P(PSTR("r[\"%s\"] == \"%s\" and "), tag->key, tag->value);
                tag = tag->next;
            }
        }
    }
    String prefix = "(";
    Script *script = _outputs->first();
    trace(T_influx2,20);
    while(script)
    {
        trace(T_influx2,21);
        reqData.printf_P(PSTR("%sr[\"_measurement\"] == \"%s\""), prefix.c_str(), varStr(_measurement, script).c_str());
        prefix = " or ";
        script = script->next();
    }
    trace(T_influx2,20);
    reqData.print(F("))\n  |> last()\n  |> map(fn: (r) => ({_measurement: r._measurement, _time: (uint(v:r._time)) / uint(v:1000000000)}))\n"));
    reqData.print(F("  |> sort (columns: [\"_time\"], desc: true)\n"));

    // Initiate the HTTP request.

    String endpoint = "/query?orgID=";
    endpoint += _orgID;
    HTTPPost(endpoint.c_str(), checkLastSent_s, "application/vnd.flux");
    return 1;
}

uint32_t influxDB_v2_uploader::tickCheckLastSent(){
    trace(T_influx2,30);

    // Deal with failure.

    if(_request->responseHTTPcode() != 200){
        trace(T_influx2,31);
        log("HTTPcode %d\n", _request->responseHTTPcode());
        delete[] _statusMessage;
        _statusMessage = charstar("Last sent query failed");
        Serial.println(_request->responseText());
        delay(60, checkLastSent_s);
        return 1;
    }

    // retrieve the response and parse first line.

    String response = _request->responseText();
    trace(T_influx2,30);
    splitstr headline(response.c_str());
    trace(T_influx2,32);

    // If no second line, query again.

    char *datapos = strchr(response.c_str(), '\n');
    trace(T_influx2,32);
    if(!datapos){
        trace(T_influx2,33);
        _state = buildLastSent_s;
        return 1;
    }

    // Have second line, parse that

    trace(T_influx2,34);
    splitstr dataline(datapos + 1);
    trace(T_influx2,34);
    if(dataline.length() == 0 || dataline.length() != headline.length()){
        _state = buildLastSent_s;
        return 1;
    }
    for (int i = 0; i < headline.length(); i++){
        if(strcmp(headline[i],"_time")){
            char *data = dataline[i];
            if(data){
                _lastSent = strtol(data, 0, 10);
                if(_lastSent >= MAX(Current_log.firstKey(), _uploadStartDate)){
                    log("influxDB_v2: Resume posting %s", localDateString(_lastSent + _interval).c_str());
                    _state = buildPost_s;
                    return 1;
                }
            }
        }
    }
    delete _statusMessage;
    _statusMessage = charstar("Last sent query invalid response stopping");
    _stop = true;
    stop();
    return 1;
}

uint32_t influxDB_v2_uploader::tickBuildPost(){
    trace(T_influx2,60);

    // This is the predominant state of the uploader
    // It will wait until there is enough data for the next post
    // then build the payload and initiate the post operation.

    if(_stop){
        stop();
        return 1;
    }

    // If not enough data to post, set wait and return.

    if(Current_log.lastKey() < (_lastSent + _interval + (_interval * _bulkSend))){
        return UTCtime() + 1;
    }

    // If datalog buffers not allocated, do so now and prime latest.

    trace(T_influx2,60);
    if(! oldRecord){
        trace(T_influx2,61);
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent + _interval;
        Current_log.readKey(newRecord);
    }

    // Build post transaction from datalog records.

    while(reqData.available() < _bufferLimit && newRecord->UNIXtime < Current_log.lastKey()){

        // Swap newRecord top oldRecord, read next into newRecord.

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

        // Build measurements for this interval

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
                        reqData.printf(" %d\n", oldRecord->UNIXtime);
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
        _lastPost = oldRecord->UNIXtime;
        reqData.printf(" %d\n", _lastPost);
    }

    // Free the buffers

    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;

    // Initiate HTTP post.

    String endpoint = "/write?precision=s&orgID=";
    endpoint += _orgID;
    endpoint += "&bucket=";
    endpoint += _bucket;
    HTTPPost(endpoint.c_str(), checkPost_s, "text/plain");
    return 1;
}

uint32_t influxDB_v2_uploader::tickCheckPost(){
    trace(T_influx2,91);

    // Check the result of a write transaction.
    // Usually success (204) just note the lastSent and
    // return to buildPost.

    if(_request->responseHTTPcode() == 204){
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _lastSent = _lastPost; 
        _state = buildPost_s;
        trace(T_influx2,93);
        return 1;
    }

    // Deal with failure.

    trace(T_influx2,92);
    char msg[100];
    sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
    delete[] _statusMessage;
    _statusMessage = charstar(msg);
    delete _request;
    _request = nullptr; 
    _state = buildPost_s;
    return UTCtime() + 2;
}

//********************************************************************************************************************
//
//                      H   H   TTTTT   TTTTT   PPPP
//                      H   H     T       T     P   P
//                      HHHHH     T       T     PPPP
//                      H   H     T       T     P
//                      H   H     T       T     P
//
//*********************************************************************************************************************    

// Subsystem to initiate HTTP transactions and wait for completion.
// Handles directing to HTTPS proxy when configured and requested.

void influxDB_v2_uploader::HTTPPost(const char* endpoint, states completionState, const char* contentType){
    
    // Build a request control block for this request,
    // set state to handle the request and return to caller.
    // Actual post is done in next tick handler.
    
    if( ! _POSTrequest){
        _POSTrequest = new POSTrequest;
    }
    delete _POSTrequest->endpoint;
    _POSTrequest->endpoint = charstar(endpoint);
    delete _POSTrequest->contentType;
    _POSTrequest->contentType = charstar(contentType);
    _POSTrequest->completionState = completionState;
    _state = HTTPpost_s;
}

uint32_t influxDB_v2_uploader::tickHTTPPost(){

    // Initiate the post request.
    // If WiFi not connected or can't get semaphore
    // just return.

    trace(T_influx2,120);
    if( ! WiFi.isConnected()){
        return UTCtime() + 1;
    }
    _HTTPtoken = HTTPreserve(T_influx2);
    if( ! _HTTPtoken){
        return 1;
    }

    // Setup request.

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
    trace(T_influx2,120);
    {
        char URL[128];
        if(_useProxyServer && HTTPSproxy){
            trace(T_influx2,121);
            size_t len = sprintf_P(URL, PSTR("%s/api/v2%s"), HTTPSproxy, _POSTrequest->endpoint);
        }
        else {
            trace(T_influx2,122);
            size_t len = sprintf_P(URL, PSTR("%s/api/v2%s"),  _url->build().c_str(), _POSTrequest->endpoint);
        }
        trace(T_influx2,123);
        if( ! _request->open("POST", URL)){
            trace(T_influx2,123);
            HTTPrelease(_HTTPtoken);
            delete _request;
            _request = nullptr;
            return UTCtime() + 5;
        }
    }
    if(_useProxyServer){
        _request->setReqHeader(F("X-proxypass"),  _url->build().c_str());
    }
    _request->setReqHeader(F("content-type"), _POSTrequest->contentType);
    trace(T_influx2,124);
    String auth = "Token ";
    auth += _token;
    _request->setReqHeader(F("Authorization"), auth.c_str());
    if( ! _request->send(&reqData, reqData.available())){
        trace(T_influx2,125);
        HTTPrelease(_HTTPtoken);
        delete _request;
        _request = nullptr;
        _lastPost = _lastSent;
        return UTCtime() + 5;
    }
    trace(T_influx2,126);
    _state = HTTPwait_s;
    return 1; 
}

uint32_t influxDB_v2_uploader::tickHTTPWait(){
    trace(T_influx2,90);
    if(_request && _request->readyState() == 4){
        HTTPrelease(_HTTPtoken);
        trace(T_influx2,91);
        if(_request->responseHTTPcode() == 429){
            log("influxDB2_v2: Rate exceeded");
            Serial.println(_request->responseText());
            delay(5, HTTPpost_s);
            return 1;
        }
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _state = _POSTrequest->completionState;
        delete _POSTrequest;
        _POSTrequest = nullptr;
        trace(T_influx2,9);
        return 1;
    }
    trace(T_influx2,93);
    return 1;
}

void influxDB_v2_uploader::delay(uint32_t seconds, states resumeState){
    _delayResumeTime = UTCtime() + seconds;
    _delayResumeState = resumeState;
    _state = delay_s;
}

uint32_t influxDB_v2_uploader::tickDelay(){
    if(_stop){
        stop();
        return 1;
    }
    if(UTCtime() >= _delayResumeTime){
        _state = _delayResumeState;
        return 1;
    }
    return UTCtime() + 1;
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

    bool influxDB_v2_uploader::config(const char *jsonConfig)
    {
        trace(T_influx2, 100);

        // Parse json configuration

        DynamicJsonBuffer Json;
        JsonObject &config = Json.parseObject(jsonConfig);
        if (!config.success())
        {
            log("influxDB_v2: Config parse failed");
            return false;
        }

        // If config not changed, return success.

        trace(T_influx2, 100);
        if (_revision == config["revision"])
        {
            return true;
        }
        _revision = config["revision"];

        // parse and validate url

        trace(T_influx2, 100);
        if(!_url){
            _url = new xurl;
        }
        if (! _url->parse(config.get<char *>("url")))
        {
            log("influxDB_v2: invalid URL");
            return false;
        }
         _url->query(nullptr);

        // Gather and check parameters

        trace(T_influx2, 100);
        _interval = config.get<unsigned int>("postInterval");
        if (!_interval || (_interval % 5 != 0))
        {
            log("influxDB_v2: Invalid interval");
            return false;
        }
        _bulkSend = config.get<unsigned int>("bulksend");
        _bulkSend = constrain(_bulkSend, 1, 10);

        trace(T_influx2, 101);
        delete[] _bucket;
        _bucket = charstar(config.get<char *>("bucket"));
        if (strlen(_bucket) == 0)
        {
            log("influxDB_v2: Bucket not specified");
            return false;
        }

        trace(T_influx2, 101);
        delete[] _orgID;
        _orgID = charstar(config.get<const char *>("orgid"));
        if (strlen(_orgID) != 16)
        {
            log("influxDB_v2: Invalid organization ID");
            return false;
        }
        trace(T_influx2, 101);
        delete[] _token;
        _token = charstar(config.get<const char *>("authtoken"));
        if (_token && strlen(_token) != 88)
        {
            log("influxDB_v2: Invalid authorization token");
            return false;
        }
        trace(T_influx2, 101);
        delete[] _measurement;
        _measurement = charstar(config.get<const char *>("measurement"));
        if (!_measurement)
        {
            _measurement = charstar("$name");
        }
        trace(T_influx2, 102);
        delete[] _fieldKey;
        ;
        _fieldKey = charstar(config.get<const char *>("fieldkey"));
        if (!_fieldKey)
        {
            _fieldKey = charstar("value");
        }
        trace(T_influx2, 102);
        _stop = config.get<bool>("stop");

        // Build tagSet

        trace(T_influx2, 103);
        delete _tagSet;
        _tagSet = nullptr;
        JsonArray &tagset = config["tagset"];
        _staticKeySet = true;
        if (tagset.success())
        {
            trace(T_influx2, 103);
            for (int i = tagset.size(); i > 0;)
            {
                i--;
                influxTag *tag = new influxTag;
                tag->next = _tagSet;
                _tagSet = tag;
                tag->key = charstar(tagset[i]["key"].as<const char *>());
                tag->value = charstar(tagset[i]["value"].as<const char *>());
                if ((strstr(tag->value, "$units") != nullptr) || (strstr(tag->value, "$name") != nullptr))
                    _staticKeySet = false;
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
    if(_state == initialize_s) {
        trace(T_influx2,105);
        NewService(influxDB_v2_tick, T_influx2);
    }
    trace(T_influx2,106);

    return true; 
}

String influxDB_v2_uploader::varStr(const char* in, Script* script)
{
    // Return String with variable substitutions.

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
