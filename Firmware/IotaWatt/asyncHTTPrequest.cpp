#include "asyncHTTPrequest.h"

//**************************************************************************************************************
asyncHTTPrequest::asyncHTTPrequest():
    _debug(DEBUG_IOTA_HTTP_SET),
    _headers(nullptr),
    _RxTimeout(DEFAULT_RX_TIMEOUT),
    _AckTimeout(DEFAULT_ACK_TIMEOUT),
    _readyState(readyStateUnsent),
    _HTTPcode(0),
    _URL(nullptr),
    _client(nullptr),
    request(nullptr),
    response(nullptr),
    _contentLength(-1),
    _host(nullptr),
    _connecting(false),
    _timeoutCBhandler(nullptr)
    {DEBUG_HTTP("New request.");}

//**************************************************************************************************************
asyncHTTPrequest::~asyncHTTPrequest(){
    end();
}

//**************************************************************************************************************
void	asyncHTTPrequest::setDebug(bool debug){
    _debug = debug;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::open(const char* method, const char* URL){
    DEBUG_HTTP("Open: %s, URL: %s\r\n", method, URL);
    if(_readyState != readyStateUnsent) {
        end();
    }   
	if(method == "GET"){
		_HTTPmethod = HTTPmethodGET;
	}
	else if(method == "POST"){
		_HTTPmethod = HTTPmethodPOST;
	}
	else return false;

    _requestStartTime = millis();
    if( ! _parseURL(URL)) return false;
    _addHeader("host",_URL->host);
	return true;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::send(){
    DEBUG_HTTP("Send no body.\r\n");
    _buildRequest("");
    return _connect();
}

//**************************************************************************************************************
bool    asyncHTTPrequest::send(String body){
    DEBUG_HTTP("Send with String body length %d\r\n", body.length());
    _buildRequest(body.c_str());
    return _connect();
}

//**************************************************************************************************************
bool	asyncHTTPrequest::send(const char* body){
    DEBUG_HTTP("Send with char* body length %d\r\n", strlen(body));
    _buildRequest(body);
    return _connect();
}

//**************************************************************************************************************
void	asyncHTTPrequest::setRxTimeout(int seconds){
    _RxTimeout = seconds;
}

//**************************************************************************************************************
void	asyncHTTPrequest::setAckTimeout(uint32_t ms){
    _AckTimeout = ms;
}

//**************************************************************************************************************
void	asyncHTTPrequest::onTimeout(timeoutCBhandler cb, void* arg = 0){
    _timeoutCBhandler = cb;
    _timeoutCBparm = arg;
}

//**************************************************************************************************************
void	asyncHTTPrequest::end(){
    DEBUG_HTTP("end()\r\n");
    _readyState = readyStateUnsent;
    delete _URL;  _URL = nullptr;
    delete[] _host;  _host = nullptr;
    delete _headers;  _headers = nullptr;
    delete request;  request = nullptr;
    delete response;  response = nullptr;
}

//**************************************************************************************************************
void	asyncHTTPrequest::setReqHeader(const char* key, const char* value){
    _addHeader(key, value);
}

//**************************************************************************************************************
void	asyncHTTPrequest::setReqHeader(const char* key, int32_t value){
    setReqHeader(key, String(value).c_str());
}

//**************************************************************************************************************
int		asyncHTTPrequest::respHeaderCount(){                                            
    int count = 0;
    header* hdr = _headers;
    while(hdr){
        count++;
        hdr = hdr->next;
    }
    return count;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderKey(int ndx){
    header* hdr = _getHeader(ndx);
    if ( ! hdr) return nullptr;
    return hdr->key;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderValue(const char* key){
    header* hdr = _getHeader(key);
    if( ! hdr) return nullptr;
    return hdr->value;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderValue(int ndx){
    header* hdr = _getHeader(ndx);
    if ( ! hdr) return nullptr;
    return hdr->value;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::respHeaderExists(const char* key){
    header* hdr = _getHeader(key);
    if ( ! hdr) return false;
    return true;
}

//**************************************************************************************************************
String  asyncHTTPrequest::headers(){
    String response = "";
    header* hdr = _headers;
    while(hdr){
        response += hdr->key;
        response += ':';
        response += hdr->value;
        response += "\r\n";
        hdr = hdr->next;
    }
    response += "\r\n";
    return response;
}

//**************************************************************************************************************
int	asyncHTTPrequest::responseHTTPcode(){
    return _HTTPcode;
}

//**************************************************************************************************************
String	asyncHTTPrequest::responseText(){
    if ( ! response){
        return String("");
    }
    String localString;
    if( ! localString.reserve(response->length())) {
        // *err Need error no memory for return string
        return String("");
    }
    localString = *response;
    delete response;
    response = nullptr;
    return localString;
}

//**************************************************************************************************************
String*	asyncHTTPrequest::responseStringPtr(){
    String* returnString = response;
    response = nullptr;
    return returnString;
}

//**************************************************************************************************************
int		asyncHTTPrequest::readyState(){
    return _readyState;
}

//**************************************************************************************************************
uint32_t asyncHTTPrequest::elapsedTime(){
    if(_readyState != readyStateDone){
        return millis() - _requestStartTime;
    }
    return _requestEndTime - _requestStartTime;
}

//**************************************************************************************************************
asyncHTTPrequest::header*  asyncHTTPrequest::_addHeader(const char* key, const char* value){
    header* hdr = (header*) &_headers;
    while(hdr->next) {
        if(_strcmp_ci(key, hdr->next->key) == 0){
            header* oldHdr = hdr->next;
            hdr->next = hdr->next->next;
            oldHdr->next = nullptr;
            delete oldHdr;
        }
        else {
            hdr = hdr->next;
        }
    }
    hdr->next = new header;
    hdr->next->key = new char[strlen(key)+1];
    strcpy(hdr->next->key, key);
    hdr->next->value = new char[strlen(value)+1];
    strcpy(hdr->next->value, value);
    return hdr->next;
}

//**************************************************************************************************************
asyncHTTPrequest::header* asyncHTTPrequest::_getHeader(const char* key){
    header* hdr = _headers;
    while (hdr) {
        if(_strcmp_ci(key, hdr->key) == 0) break;
        hdr = hdr->next;
    }
    return hdr;
}

//**************************************************************************************************************
asyncHTTPrequest::header* asyncHTTPrequest::_getHeader(int ndx){
    header* hdr = _headers;
    while (hdr) {
        if( ! ndx--) break;
        hdr = hdr->next; 
    }
    return hdr;
}

//**************************************************************************************************************
int asyncHTTPrequest::_strcmp_ci(const char* str1, const char* str2){
    const char* char1 = str1;
    const char* char2 = str2;
    while(*char1 || *char2){
        if(*(char1++) != *(char2++)){
            if(toupper(*(char1-1)) > toupper(*(char2-1))) return +1;
            if(toupper(*(char1-1)) < toupper(*(char2-1))) return -1;
        }
    }
    return 0;
}

//**************************************************************************************************************
void   asyncHTTPrequest::_buildRequest(const char *body){
    delete request;
    if(strlen(body)){
        _addHeader("Content-Length", String(strlen(body)).c_str());
    }

            //  Compute size of header

    int headerSize = _HTTPmethod == HTTPmethodGET ? 4 : 5;
    headerSize += strlen(_URL->scheme) + 
                  strlen(_URL->host) + 
                  strlen(_URL->path) +
                  strlen(_URL->query) + 12;
    header* hdr = _headers;
    while(hdr){
        headerSize += strlen(hdr->key) + 1 + strlen(hdr->value) + 2;
        hdr = hdr->next;
    }
    headerSize += 2;
    
            // Create a String buffer and reserve space.

    request = new String;
    if( ! request->reserve(headerSize + strlen(body) ? strlen(body) + 2 : 0)){
        // *err string allocation failed
        return;
    }

        // Build the header.

    *request += String((_HTTPmethod == HTTPmethodGET) ? "GET " : "POST ");
    *request += _URL->scheme;
    *request += _URL->host;
    *request += _URL->path;
    *request += _URL->query;
    *request += " HTTP/1.1\r\n";
    hdr = _headers;
    while(hdr){
        *request += hdr->key;
        *request += ':';
        *request += hdr->value;
        *request += "\r\n";
        hdr = hdr->next;
    }
    delete _headers;
    _headers = nullptr;
    *request += "\r\n";

    if(strlen(body)){
        *request += body;
       // *request += "\r\n";
    }
}

//**************************************************************************************************************
bool  asyncHTTPrequest::_parseURL(const char* url){
    return _parseURL(String(url));
}

//**************************************************************************************************************
bool  asyncHTTPrequest::_parseURL(String url){
    delete _URL;
    int hostBeg = 0;
    _URL = new URL;
    _URL->scheme = new char[8];
    strcpy(_URL->scheme, "HTTP://");
    if(url.startsWith("HTTP://")) {
       hostBeg += 7; 
    }
    int pathBeg = url.indexOf('/', hostBeg);
    if(pathBeg < 0) return false;
    int hostEnd = pathBeg;
    int portBeg = url.indexOf(':',hostBeg);
    if(portBeg > 0 && portBeg < pathBeg){
        _URL->port = url.substring(portBeg+1, pathBeg).toInt();
        hostEnd = portBeg;
    }
    _URL->host = new char[hostEnd - hostBeg + 1];
    strcpy(_URL->host, url.substring(hostBeg, hostEnd).c_str());
    int queryBeg = url.indexOf('?');
    if(queryBeg < 0) queryBeg = url.length();
    _URL->path = new char[queryBeg - pathBeg + 1];
    strcpy(_URL->path, url.substring(pathBeg, queryBeg).c_str());
    _URL->query = new char[url.length() - queryBeg + 1];
    strcpy(_URL->query, url.substring(queryBeg).c_str());
    DEBUG_HTTP("_parseURL: %s%s:%d%s%s\r\n", _URL->scheme, _URL->host, _URL->port, _URL->path, _URL->query);
    return true;
}

//**************************************************************************************************************
bool  asyncHTTPrequest::_connect(){
    DEBUG_HTTP("_connect\r\n");
    _client = new AsyncClient();
    _client->setRxTimeout(_RxTimeout);
    _client->setAckTimeout(_AckTimeout);
    _client->onConnect([](void *obj, AsyncClient *client){((asyncHTTPrequest*)(obj))->_onConnect(client);}, this);
    _client->onDisconnect([](void *obj, AsyncClient* client){((asyncHTTPrequest*)(obj))->_onDisconnect(client);}, this);
    _client->onTimeout([](void *obj, AsyncClient *client, uint32_t time){((asyncHTTPrequest*)(obj))->_onTimeout(client);}, this);
    _client->onError([](void *obj, AsyncClient *client, uint32_t error){((asyncHTTPrequest*)(obj))->_onError(client, error);}, this);
    if( ! _client->connect(_URL->host, _URL->port)) {
        DEBUG_HTTP("connect failed\r\n");
        _HTTPcode = HTTPCODE_NOT_CONNECTED;
        _setReadyState(readyStateDone);
        return false;
    }
    _connecting = true;
    return true;
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onConnect(AsyncClient* client){
    DEBUG_HTTP("connect handler\r\n");
    _client = client;
    _connecting = false;
    _setReadyState(readyStateOpened);
    _client->onAck([](void* obj, AsyncClient* client, size_t len, uint32_t time){((asyncHTTPrequest*)(obj))->_send();}, this);
    _client->onData([](void* obj, AsyncClient* client, void* data, size_t len){((asyncHTTPrequest*)(obj))->_onData(data, len);}, this);
    if(_client->canSend()){
        _send();
    }
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onError(AsyncClient* client, int8_t error){
    DEBUG_HTTP("error handler, error=%d\r\n", error);
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onDisconnect(AsyncClient* client){
    DEBUG_HTTP("disconnect handler\r\n");
    if(_connecting){
        _HTTPcode = HTTPCODE_NOT_CONNECTED;
    }
    else if(_readyState != readyStateLoading || 
             response->length() < _contentLength) {
            _HTTPcode = HTTPCODE_CONNECTION_LOST;
    }
    delete _client;
    _client = nullptr;
    _requestEndTime = millis();
    _setReadyState(readyStateDone);
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onTimeout(AsyncClient* client){
//    _client = client;
    DEBUG_HTTP("timeout handler\r\n");
    _client->abort();
    if(_timeoutCBhandler){
        _timeoutCBhandler(this, _timeoutCBparm);
    }
    _HTTPcode = HTTPCODE_READ_TIMEOUT;
}

//**************************************************************************************************************
size_t  asyncHTTPrequest::_send(){
    if( ! request) return 0;
    DEBUG_HTTP("send %d\r\n", request->length());
    size_t supply = request->length();
    if( ! _client->connected() || ! _client->canSend() || supply == 0){
        return 0;
    }
    size_t demand = _client->space();
    if(supply > demand) supply = demand;
    size_t sent = _client->write(request->c_str(), request->length());
    if(request->length() == sent){
        delete request;
        request = nullptr;
    }
    else {
        request->remove(0, sent);
    }
    DEBUG_HTTP("sent %d\r\n", sent); 
    return sent;
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onData(void* Vbuf, size_t len){
    DEBUG_HTTP("onDATA length %d\r\n", len);
    if( ! response){
        response = new String;
    }
    if( ! response->reserve(response->length() + len)){
        //  *err insufficient buffer failure
        return;
    }
    size_t remaining = len;
    size_t move = 256;
    char *temp = new char[move+1];
    while(remaining){
        if(remaining < move){
            move = remaining;
        }
        memcpy(temp,(char*) Vbuf+len-remaining, move);
        temp[move] = 0;
        response->concat(temp);
        remaining -= move;
    }
    delete[] temp;
    
    if(_readyState == readyStateOpened) {
        _collectHeaders();
    }

    if(_readyState == readyStateHdrsRecvd) {
        if(response->length()){
            _setReadyState(readyStateLoading);
        }
    }

    if(_readyState == readyStateLoading) {
        if(response->length() >= _contentLength) {
            _client->close();
        }
    }
}

//**************************************************************************************************************
void  asyncHTTPrequest::_collectHeaders(){
    DEBUG_HTTP("Collect Headers\r\n");
    size_t lineBeg = 0;
    size_t lineEnd = 0;
    while(_readyState == readyStateOpened){
        lineEnd = response->indexOf("\r\n", lineBeg);
        if(lineEnd  == -1) break;
        if(lineBeg == lineEnd) {
            _setReadyState(readyStateHdrsRecvd);
        }
        else if(response->substring(lineBeg,7) == "HTTP/1."){
            _HTTPcode = response->substring(lineBeg+9, response->indexOf(' ', lineBeg+9)).toInt();
        } 
        else {
            size_t colon = response->indexOf(':', lineBeg);
            if(colon > lineBeg && colon < lineEnd){
                _addHeader(response->substring(lineBeg, colon).c_str(),
                           response->substring(colon+1, lineEnd).c_str());
            }   
        }
        lineBeg = lineEnd +2;   
    }
    response->remove(0, lineBeg);
    if(_readyState != readyStateOpened){
        header *hdr = _getHeader("content-length");
        if(hdr){
            _contentLength = String(hdr->value).toInt();
        }
    }
}

//**************************************************************************************************************
void  asyncHTTPrequest::_setReadyState(readyStates newState){
    if(_readyState != newState){
        _readyState = newState;          // implement onReadyState callbacks here
        DEBUG_HTTP("readyState %d\r\n", _readyState);
    } 
}