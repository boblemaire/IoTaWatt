#include "IotaWatt.h"
#include "Uploader.h"
#include "splitstr.h"

void Uploader::getStatusJson(JsonObject& status)
{
    // Set status information in callers's json object.


    trace(T_uploader,110);
    status.set(F("id"), _id);
    if(_state == stopped_s){
        status.set(F("status"), "stopped");
    } else {
        status.set(F("status"), "running");
    }
    status.set(F("lastpost"), _lastSent);
    if(_statusMessage){
        status.set(F("message"), _statusMessage);
    }
    trace(T_uploader,110);
}

void Uploader::end()
{
    trace(T_uploader,120);    
    _stop = true;
    _end = true;
}
 
    // This is the worm hole that the scheduler uses to get into the class state machine.
    // It invokes the dispatch method of the class.
    // On return, it checks for no_schedule return and invokes the stop() method. 

uint32_t uploader_dispatch(struct serviceBlock* serviceBlock) {
    trace(T_uploader,0);
    Uploader*_this = (Uploader*)serviceBlock->serviceParm;
    trace(T_uploader,1);
    uint32_t reschedule = _this->dispatch(serviceBlock);
    trace(T_uploader,1);
    if(reschedule){
        return reschedule;
    } 
    trace(T_uploader,0);
    return 0;
}


uint32_t Uploader::dispatch(struct serviceBlock *serviceBlock)
{
    trace(T_uploader,2,_state);
    switch (_state) {
        case initialize_s:
            return handle_initialize_s();
        case query_s:
            return handle_query_s();
        case checkQuery_s:
            return handle_checkQuery_s();
        case write_s:              
            return handle_write_s();
        case checkWrite_s:
            return handle_checkWrite_s();
        case stopped_s:
            return handle_stopped_s();
        case HTTPpost_s:
            return handle_HTTPpost_s();
        case HTTPwait_s:
            return handle_HTTPwait_s();
        case delay_s:
            return handle_delay_s();
    }
    trace(T_uploader,3,_state);
    log("%s: Unrecognized state, stopping;",_id);
    delete this;
    return 0;
}

void Uploader::stop(){
    log("%s: stopped, Last post %s", _id, localDateString(_lastSent).c_str());
    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;
    // delete _request;
    // _request = nullptr;
    reqData.flush();
    delete _url;
    _url = nullptr;
    trace(T_uploader, 7);
    _stop = true;
    _state = stopped_s;
}

uint32_t Uploader::handle_stopped_s(){
    trace(T_uploader,5);
    if(_end){
        trace(T_uploader,6);
        delete this;
        return 0;
    }
    if(_stop){
        return UTCtime() + 1;
    }
    delete[] _statusMessage;
    _statusMessage = nullptr;
    trace(T_uploader,8);
    _lastSent = 0;
    _state = query_s;
    return 1;
}

uint32_t Uploader::handle_initialize_s(){
    trace(T_uploader,10);
    log("%s: Starting, interval:%d, url:%s", _id, _interval, _url->build().c_str());
    _state = query_s;
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

void Uploader::HTTPPost(const char* endpoint, states completionState, const char* contentType){
    HTTP("POST", endpoint, completionState, contentType);
}

void Uploader::HTTPGet(const char* endpoint, states completionState){
    HTTP("GET", endpoint, completionState);
}

void Uploader::HTTP(const char *method, const char* endpoint, states completionState, const char* contentType){
    
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
    _POSTrequest->method = strcmp(method,"GET") == 0 ? method_GET : method_POST;
    _state = HTTPpost_s;
}

uint32_t Uploader::handle_HTTPpost_s(){

    // Initiate the post request.
    // If WiFi not connected or can't get semaphore
    // just return.

    trace(T_uploader,120);
    if( ! WiFi.isConnected()){
        return UTCtime() + 1;
    }

    if(_useProxyServer && HTTPSproxy == nullptr){
        log("%s: No HTTPS proxy - stopping.", _id);
        _statusMessage = charstar(F("No HTTPS proxy configured."));
        stop();
        return 1;
    }

    _HTTPtoken = HTTPreserve(T_uploader);
    if( ! _HTTPtoken){
        return 15;
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
    trace(T_uploader,120);
    {
        char URL[200];
        size_t len;
        if(_useProxyServer){
            trace(T_uploader,121);
            len = snprintf_P(URL, 200, PSTR("%s%s"), HTTPSproxy, _POSTrequest->endpoint);
        }
        else {
            trace(T_uploader,122);
            len = snprintf_P(URL, 200,  PSTR("%s%s"),  _url->build().c_str(), _POSTrequest->endpoint);
        }

        trace(T_uploader,123);
        if( ! _request->open(_POSTrequest->method == method_POST ? "POST" : "GET", URL)){
            trace(T_uploader,123);
            HTTPrelease(_HTTPtoken);
            delete _request;
            _request = nullptr;
            return UTCtime() + 5;
        }
    }
    if(_useProxyServer){
        _request->setReqHeader(F("X-proxypass"),  _url->build().c_str());
    }
    bool sent = true;
    if(_POSTrequest->method == method_GET){
        trace(T_uploader,124,1);
        setRequestHeaders();
        sent = _request->send();
    }
    else {
        trace(T_uploader,124,2);
        _request->setReqHeader(F("content-type"), _POSTrequest->contentType);
        setRequestHeaders();
        sent = _request->send(&reqData, reqData.available());
    }
    if(!sent){
        trace(T_uploader,125);
        HTTPrelease(_HTTPtoken);
        reqData.flush();
        delete _request;
        _request = nullptr;
        _lastPost = _lastSent;
        return UTCtime() + 5;
    }
    reqData.flush();
    trace(T_uploader,126);
    _state = HTTPwait_s;
    return 10; 
}

uint32_t Uploader::handle_HTTPwait_s(){
    trace(T_uploader,90);
    if(_request && _request->readyState() == 4){
        HTTPrelease(_HTTPtoken);
        trace(T_uploader,91);
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _state = _POSTrequest->completionState;
        delete _POSTrequest;
        _POSTrequest = nullptr;
        trace(T_uploader,9);
        return 1;
    }
    trace(T_uploader,93);
    return 10;
}

void Uploader::setRequestHeaders(){};

void Uploader::delay(uint32_t seconds, states resumeState){
    _delayResumeTime = UTCtime() + seconds;
    _delayResumeState = resumeState;
    _state = delay_s;
}

uint32_t Uploader::handle_delay_s(){
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

bool Uploader::config(const char *jsonConfig)
{
    trace(T_uploader, 100);

    // Parse json configuration

    DynamicJsonBuffer Json;
    JsonObject &config = Json.parseObject(jsonConfig);
    if (!config.success())
    {
        log("%s: Config parse failed", _id);
        return false;
    }

    // If config not changed, return success.

    trace(T_uploader, 100);
    if (_revision == config["revision"]) 
    {
        return true;
    }
    _revision = config["revision"];

    // parse and validate url

    trace(T_uploader, 100);
    if(!_url){
        _url = new xurl;
    }
    if (! _url->parse(config.get<char *>("url")))
    {
        log("%s: invalid URL", _id);
        return false;
    }
    _url->query(nullptr);
    _useProxyServer = false;
    if(strcmp_ci(_url->method(),"https://")== 0){
        if(HTTPSproxy){
            _useProxyServer = true;
        }
        else {
            log("%s: HTTPS specified but no HTTPSproxy server configured, stopping.", _id);
            return false;
        }
    }

    // Gather and check parameters

    trace(T_uploader, 100);
    _interval = config.get<unsigned int>("postInterval");
    if (!_interval || (_interval % 5 != 0))
    {
        log("%s: Invalid interval", _id);
        return false;
    }
    _bulkSend = config.get<unsigned int>("bulksend");
    _bulkSend = constrain(_bulkSend, 1, 10);
    _stop = config.get<bool>("stop");
    _uploadStartDate = config.get<unsigned int>("begdate");

        // Build the measurement scriptset

    trace(T_uploader,101);
    delete _outputs;
    JsonVariant var = config["outputs"];
    if(var.success()){
        trace(T_uploader,102);
        _outputs = new ScriptSet(var.as<JsonArray>());
    }
    else {
        log("%s: No outputs, stopping.", _id);
        return false;
    }

    // Callback to derived class for unique configuration requirements
    // if that goes OK (true) then start the Service.

    if (configCB(config))
    {
        trace(T_uploader,105);    
        if(_state == initialize_s) {
            trace(T_uploader,105);
            serviceBlock *sb = NewService(uploader_dispatch, T_uploader);
            sb->serviceParm = (void *)this;
        }
        trace(T_uploader,106);
        return true;
    }

    // Callback failed, return false;

    return false;
}

bool Uploader::configCB(JsonObject &) { return true; };