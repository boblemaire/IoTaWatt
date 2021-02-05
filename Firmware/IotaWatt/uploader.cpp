#include "iotawatt.h"
#include "uploader.h"
#include "splitstr.h"

void uploader::getStatusJson(JsonObject& status)
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

void uploader::end()
{
    trace(T_influx2,120);    
    _stop = true;
    _end = true;
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
    // This is the worm hole that the scheduler uses to get into the class state machine.
    // It invokes the tick method of the class.
    // On return, it checks for no_schedule return and invokes the stop() method. 

uint32_t uploader_tick(struct serviceBlock* serviceBlock) {
    trace(T_influx2,0);
    uploader *_this = (uploader *)serviceBlock->serviceParm;
    trace(T_influx2,1);
    uint32_t reschedule = _this->tick(serviceBlock);
    trace(T_influx2,1);
    if(reschedule){
        return reschedule;
    } 
    trace(T_influx2,0);
    return 0;
}


uint32_t uploader::tick(struct serviceBlock *serviceBlock)
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
    log("%s: Unrecognized state, stopping;",_id);
    delete this;
    return 0;
}

void uploader::stop(){
    log("%s: stopped, Last post %s", _id, localDateString(_lastSent).c_str());
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
    _stop = true;
    _state = stopped_s;
}

uint32_t uploader::tickStopped(){
    trace(T_influx2,5);
    if(_end){
        trace(T_influx2,6);
        delete this;
        return 0;
    }
    if(_stop){
        return UTCtime() + 1;
    }
    delete[] _statusMessage;
    _statusMessage = nullptr;
    trace(T_influx2,8);
    _state = buildLastSent_s;
    return 1;
}

uint32_t uploader::tickInitialize(){
    trace(T_influx2,10);
    log("%s: Starting, interval:%d, url:%s", _id, _interval, _url->build().c_str());
    _state = buildLastSent_s;
    return 1;
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

void uploader::HTTPPost(const char* endpoint, states completionState, const char* contentType){
    
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

uint32_t uploader::tickHTTPPost(){

    // Initiate the post request.
    // If WiFi not connected or can't get semaphore
    // just return.

    trace(T_influx2,120);
    if( ! WiFi.isConnected()){
        return UTCtime() + 1;
    }

    if(_useProxyServer && HTTPSproxy == nullptr){
        log("%s: No HTTPS proxy - stopping.", _id);
        _statusMessage = charstar(F("No HTTPS proxy configured."));
        stop();
        return 1;
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
        if(_useProxyServer){
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
    setRequestHeaders();
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

uint32_t uploader::tickHTTPWait(){
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

void uploader::setRequestHeaders(){};

void uploader::delay(uint32_t seconds, states resumeState){
    _delayResumeTime = UTCtime() + seconds;
    _delayResumeState = resumeState;
    _state = delay_s;
}

uint32_t uploader::tickDelay(){
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

