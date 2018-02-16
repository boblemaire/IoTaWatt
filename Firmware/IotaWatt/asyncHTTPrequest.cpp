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
    _readyStateChangeCB(nullptr),
    _onDataCB(nullptr)
    {DEBUG_HTTP("New request.");}

//**************************************************************************************************************
asyncHTTPrequest::~asyncHTTPrequest(){
    if(_client){
        _client->abort();
    }
    delete _URL;  _URL = nullptr;
    delete[] _host;  _host = nullptr;
    delete _headers;  _headers = nullptr;
    delete request;  request = nullptr;
    delete response;  response = nullptr;
}

//**************************************************************************************************************
void	asyncHTTPrequest::setDebug(bool debug){
    _debug = debug;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::open(const char* method, const char* URL){
    DEBUG_HTTP("Open: %s, URL: %s\r\n", method, URL);
    if(_readyState != readyStateUnsent && _readyState != readyStateDone) {
        return false;
    }
    delete _URL;  _URL = nullptr;
    delete[] _host;  _host = nullptr;
    delete _headers;  _headers = nullptr;
    delete request;  request = nullptr;
    delete response;  response = nullptr;
    _contentRead = 0;
    _readyState = readyStateUnsent;   
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
	return _connect();
}

void    asyncHTTPrequest::onReadyStateChange(readyStateChangeCB cb, void* arg){
    _readyStateChangeCB = cb;
    _readyStateChangeCBarg = arg;
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
bool	asyncHTTPrequest::send(){
    DEBUG_HTTP("Send no body.\r\n");
    if( ! _buildRequest("")) return false;
    _send();
    return true;
}

//**************************************************************************************************************
bool    asyncHTTPrequest::send(String body){
    DEBUG_HTTP("Send with String body length %d\r\n", body.length());
    if( ! _buildRequest(body.c_str())) return false;
    _send();
    return true;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::send(const char* body){
    DEBUG_HTTP("Send with char* body length %d\r\n", strlen(body));
    if( ! _buildRequest(body)) return false;
    _send();
    return true;
}

//**************************************************************************************************************
void    asyncHTTPrequest::abort(){
    DEBUG_HTTP("abort\r\n");
    if(! _client) return;
    _client->abort();
}
//**************************************************************************************************************
int		asyncHTTPrequest::readyState(){
    return _readyState;
}

//**************************************************************************************************************
int	asyncHTTPrequest::responseHTTPcode(){
    return _HTTPcode;
}

//**************************************************************************************************************
String	asyncHTTPrequest::responseText(){
    if( ! response || _readyState < readyStateLoading || ! available()) return String();      
    String localString;
    size_t avail = available();
    if( ! localString.reserve(avail)) {
        _HTTPcode = HTTPCODE_TOO_LESS_RAM;
        return String("");
        _client->abort();
    }
    localString = response->substring(0, avail);
    String* newString = new String(response->substring(avail));
    delete response;
    response = newString;
    _contentRead += avail;
    if(_client){
        _client->ack(_notAcked);    
    }
    _notAcked = 0;
    return localString;
}

//**************************************************************************************************************
String*	asyncHTTPrequest::responseStringPtr(){
    if( ! response || _readyState < readyStateLoading || ! available()) return nullptr;
    size_t avail = available();
    String* returnString = response;
    response = new String(response->substring(avail));
    returnString->remove(avail);
    _contentRead += avail;
    if(_client){
        _client->ack(_notAcked);   
    }
    _notAcked = 0;
    return returnString;
}

//**************************************************************************************************************
size_t  asyncHTTPrequest::read(uint8_t* buf, size_t len){
    if( ! response || _readyState < readyStateLoading || ! available()) return 0;
    size_t avail = available();
    if(avail > len){
        avail = len;
    }
    response->getBytes((byte*) buf, avail);
    response->remove(0, avail);
    _contentRead += avail;
    if(_client){
        _client->ack(_notAcked);   
    }
    _notAcked = 0;
    return avail;
}

//**************************************************************************************************************
size_t	asyncHTTPrequest::available(){
    if(_readyState < readyStateLoading) return 0;
    size_t avail = response->length();
    if(_chunked && (_contentLength - _contentRead) < response->length()){
        return _contentLength - _contentRead;
    }
    return response->length();
}

//**************************************************************************************************************
void	asyncHTTPrequest::onData(onDataCB cb, void* arg){
    _onDataCB = cb;
    _onDataCBarg = arg;
}

//**************************************************************************************************************
uint32_t asyncHTTPrequest::elapsedTime(){
    if(_readyState <= readyStateOpened) return 0;
    if(_readyState != readyStateDone){
        return millis() - _requestStartTime;
    }
    return _requestEndTime - _requestStartTime;
}

/*______________________________________________________________________________________________________________

               PPPP    RRRR     OOO    TTTTT   EEEEE    CCC    TTTTT   EEEEE   DDDD
               P   P   R   R   O   O     T     E       C   C     T     E       D   D
               PPPP    RRRR    O   O     T     EEE     C         T     EEE     D   D
               P       R  R    O   O     T     E       C   C     T     E       D   D
               P       R   R    OOO      T     EEEEE    CCC      T     EEEEE   DDDD
_______________________________________________________________________________________________________________*/

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
    DEBUG_HTTP("_client->connect. host=%s, port=%d\r\n", _URL->host, _URL->port);
    if( ! _client->connect(_URL->host, _URL->port)) {
        DEBUG_HTTP("connect failed\r\n");
        _HTTPcode = HTTPCODE_NOT_CONNECTED;
        _setReadyState(readyStateDone);
        return false;
    }
    DEBUG_HTTP("_client->connect: true\r\n");
    _connecting = true;
    return true;
}

//**************************************************************************************************************
bool   asyncHTTPrequest::_buildRequest(const char *body){
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
        headerSize += strlen(hdr->name) + 1 + strlen(hdr->value) + 2;
        hdr = hdr->next;
    }
    headerSize += 2;
    
            // Create a String buffer and reserve space.

    request = new String;
    if( ! request->reserve(headerSize + strlen(body) ? strlen(body) + 2 : 0)){
        _HTTPcode = HTTPCODE_TOO_LESS_RAM;
        return false;
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
        *request += hdr->name;
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
    }
    return true;
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
void  asyncHTTPrequest::_setReadyState(readyStates newState){
    if(_readyState != newState){
        _readyState = newState;          
        DEBUG_HTTP("readyState %d\r\n", _readyState);
        if(_readyStateChangeCB){
            _readyStateChangeCB(_readyStateChangeCBarg, this, _readyState);
        }
    } 
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

/*______________________________________________________________________________________________________________

EEEEE   V   V   EEEEE   N   N   TTTTT         H   H    AAA    N   N   DDDD    L       EEEEE   RRRR     SSS
E       V   V   E       NN  N     T           H   H   A   A   NN  N   D   D   L       E       R   R   S 
EEE     V   V   EEE     N N N     T           HHHHH   AAAAA   N N N   D   D   L       EEE     RRRR     SSS
E        V V    E       N  NN     T           H   H   A   A   N  NN   D   D   L       E       R  R        S
EEEEE     V     EEEEE   N   N     T           H   H   A   A   N   N   DDDD    LLLLL   EEEEE   R   R    SSS 
_______________________________________________________________________________________________________________*/


//**************************************************************************************************************
void  asyncHTTPrequest::_onConnect(AsyncClient* client){
    DEBUG_HTTP("connect handler\r\n");
    _client = client;
    delete _URL;  _URL = nullptr;
    delete[] _host;  _host = nullptr;
    _connecting = false;
    _setReadyState(readyStateOpened);
    response = new String;
    _notAcked = 0;
    _contentLength = 0;
    _contentRead = 0;
    _chunked = false;
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
    else if(_HTTPcode > 0 &&
            (_readyState != readyStateLoading || 
            (_contentRead + response->length()) < _contentLength)) {
            _HTTPcode = HTTPCODE_CONNECTION_LOST;
    }
    delete _client;
    _client = nullptr;
    _requestEndTime = millis();
    _setReadyState(readyStateDone);
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onTimeout(AsyncClient* client){
    DEBUG_HTTP("timeout handler\r\n");
    _client->abort();
    _HTTPcode = HTTPCODE_READ_TIMEOUT;
}

//**************************************************************************************************************
void  asyncHTTPrequest::_onData(void* Vbuf, size_t len){
    DEBUG_HTTP("onDATA length %d\r\n", len);
    
                // Insure space in buffer String, throw error if not.

    if( ! response->reserve(response->length() + len)){
        _HTTPcode = HTTPCODE_TOO_LESS_RAM;
        DEBUG_HTTP("cannot extent buffer from %d to %d\r\n", response->length(), response->length()+len);
        _client->close();
        return;
    }

                // Copy raw buffer to buffer String.
                // For flow control, don't ack until caller takes data.

    _client->ackLater();        
    _notAcked += len;
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
    
                // If headers not complete, collect them.
                // If still not complete, we're done for now.
                // If done, set Content-Length or Chunked as the case may be.

    if(_readyState == readyStateOpened) {
        if( ! _collectHeaders()) return;
        header *hdr = _getHeader("Content-Length");
        if(hdr){
            _contentLength = strtol(hdr->value,nullptr,10);
        }
        hdr = _getHeader("Transfer-Encoding"); 
        if(hdr && _strcmp_ci(hdr->value, "chunked") == 0){
            DEBUG_HTTP("TE chunked\r\n");
            _chunked = true;
            _contentLength = 0;
        }
        else {
            response->remove(0,2);
        } 
    }

                // If chunked response, check for another chunk header.
                // If found, add data length to _contentLength and remove the header.
                // If it was the zero length delimiter, close.

    if(_chunked && (response->length() > (_contentLength - _contentRead))){
        size_t chunkEnd = _contentLength - _contentRead;
        int32_t hdrEnd = response->indexOf("\r\n", chunkEnd+2);
        if(hdrEnd > chunkEnd){
            size_t chunkSize = strtol(response->substring(chunkEnd+2, hdrEnd).c_str(),nullptr,16);
            DEBUG_HTTP("he %d, ce %d\r\n", hdrEnd, chunkEnd);
            DEBUG_HTTP("chksz %d, _cL %d, _cR %d, rl %d\r\n", chunkSize, _contentLength, _contentRead, response->length());
            _contentLength += chunkSize;
            response->remove(chunkEnd, hdrEnd-chunkEnd+2);
            if(chunkSize == 0){
                _client->close();
            }
        }
    }

                // If there's data in the buffer and not Done,
                // advance readyState to Loading.

    if(response->length() && _readyState != readyStateDone){
        _setReadyState(readyStateLoading);
    }

                // If not chunked and all data read, close it up.

    if( ! _chunked && (response->length() + _contentRead) >= _contentLength){
        _client->close();
    }

                // If onData callback requested and data above minimum
                // make the call.

    if(_onDataCB && available()){
        _onDataCB(_onDataCBarg, this, available());
    }            
          
}

//**************************************************************************************************************
bool  asyncHTTPrequest::_collectHeaders(){
    DEBUG_HTTP("Collect Headers\r\n");
    size_t lineBeg = 0;
    size_t lineEnd = 0;
    while((lineEnd = response->indexOf("\r\n", lineBeg)) != -1) {
        if(lineBeg == lineEnd) {
            _setReadyState(readyStateHdrsRecvd);
            response->remove(0, lineBeg);
            return true;
        }
        if(response->substring(lineBeg,7) == "HTTP/1."){
            _HTTPcode = response->substring(lineBeg+9, response->indexOf(' ', lineBeg+9)).toInt();
        } 
        else {
            size_t colon = response->indexOf(':', lineBeg);
            if(colon > lineBeg && colon < lineEnd){
                String name = response->substring(lineBeg, colon);
                name.trim();
                String value = response->substring(colon+1, lineEnd);
                value.trim();
                _addHeader(name.c_str(), value.c_str());
            }   
        }
        lineBeg = lineEnd +2;   
    }
    response->remove(0, lineBeg);
    return false;
}

/*_____________________________________________________________________________________________________________

                        H   H  EEEEE   AAA   DDDD   EEEEE  RRRR    SSS
                        H   H  E      A   A  D   D  E      R   R  S   
                        HHHHH  EEE    AAAAA  D   D  EEE    RRRR    SSS
                        H   H  E      A   A  D   D  E      R  R       S
                        H   H  EEEEE  A   A  DDDD   EEEEE  R   R   SSS
______________________________________________________________________________________________________________*/

//**************************************************************************************************************
void	asyncHTTPrequest::setReqHeader(const char* name, const char* value){
    if(_readyState != readyStateUnsent) return;
    _addHeader(name, value);
}

//**************************************************************************************************************
void	asyncHTTPrequest::setReqHeader(const char* name, int32_t value){
    if(_readyState != readyStateUnsent) return;
    setReqHeader(name, String(value).c_str());
}

//**************************************************************************************************************
int		asyncHTTPrequest::respHeaderCount(){
    if(_readyState < readyStateHdrsRecvd) return 0;                                            
    int count = 0;
    header* hdr = _headers;
    while(hdr){
        count++;
        hdr = hdr->next;
    }
    return count;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderName(int ndx){
    if(_readyState < readyStateHdrsRecvd) return nullptr;      
    header* hdr = _getHeader(ndx);
    if ( ! hdr) return nullptr;
    return hdr->name;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderValue(const char* name){
    if(_readyState < readyStateHdrsRecvd) return nullptr;      
    header* hdr = _getHeader(name);
    if( ! hdr) return nullptr;
    return hdr->value;
}

//**************************************************************************************************************
char*   asyncHTTPrequest::respHeaderValue(int ndx){
    if(_readyState < readyStateHdrsRecvd) return nullptr;      
    header* hdr = _getHeader(ndx);
    if ( ! hdr) return nullptr;
    return hdr->value;
}

//**************************************************************************************************************
bool	asyncHTTPrequest::respHeaderExists(const char* name){
    if(_readyState < readyStateHdrsRecvd) return false;      
    header* hdr = _getHeader(name);
    if ( ! hdr) return false;
    return true;
}

//**************************************************************************************************************
String  asyncHTTPrequest::headers(){
    String response = "";
    header* hdr = _headers;
    while(hdr){
        response += hdr->name;
        response += ':';
        response += hdr->value;
        response += "\r\n";
        hdr = hdr->next;
    }
    response += "\r\n";
    return response;
}

//**************************************************************************************************************
asyncHTTPrequest::header*  asyncHTTPrequest::_addHeader(const char* name, const char* value){
    header* hdr = (header*) &_headers;
    while(hdr->next) {
        if(_strcmp_ci(name, hdr->next->name) == 0){
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
    hdr->next->name = new char[strlen(name)+1];
    strcpy(hdr->next->name, name);
    hdr->next->value = new char[strlen(value)+1];
    strcpy(hdr->next->value, value);
    return hdr->next;
}

//**************************************************************************************************************
asyncHTTPrequest::header* asyncHTTPrequest::_getHeader(const char* name){
    header* hdr = _headers;
    while (hdr) {
        if(_strcmp_ci(name, hdr->name) == 0) break;
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
