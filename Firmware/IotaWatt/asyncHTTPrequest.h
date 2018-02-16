#ifndef asyncHTTPrequest_h
#define asyncHTTPrequest_h


   /***********************************************************************************
    IotaWatt Electric Power Monitor System
    Copyright (C) <2017>  <Bob Lemaire, IoTaWatt, Inc.>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.  
   
***********************************************************************************/

#ifndef DEBUG_IOTA_PORT
#define DEBUG_IOTA_PORT Serial
#endif

#ifdef DEBUG_IOTA_HTTP
#define DEBUG_IOTA_HTTP_SET true
#else
#define DEBUG_IOTA_HTTP_SET false
#endif

#include <Arduino.h>
#include <ESPasyncTCP.h>
#include <pgmspace.h>

#define DEBUG_HTTP(format,...)  if(_debug){\
                                    DEBUG_IOTA_PORT.print("Debug: ");\
                                    DEBUG_IOTA_PORT.printf_P(PSTR(format),##__VA_ARGS__);}

#define DEFAULT_RX_TIMEOUT 3                    // Seconds for connect timeout
#define DEFAULT_ACK_TIMEOUT 2000                // Ms for ack timeout

#define HTTPCODE_CONNECTION_REFUSED  (-1)
#define HTTPCODE_SEND_HEADER_FAILED  (-2)
#define HTTPCODE_SEND_PAYLOAD_FAILED (-3)
#define HTTPCODE_NOT_CONNECTED       (-4)
#define HTTPCODE_CONNECTION_LOST     (-5)
#define HTTPCODE_NO_STREAM           (-6)
#define HTTPCODE_NO_HTTP_SERVER      (-7)
#define HTTPCODE_TOO_LESS_RAM        (-8)
#define HTTPCODE_ENCODING            (-9)
#define HTTPCODE_STREAM_WRITE        (-10)
#define HTTPCODE_READ_TIMEOUT        (-11)

class asyncHTTPrequest {

  struct header {
	  header*	 	next;
	  char*			name;
	  char*			value;
	  header():
        next(nullptr), 
        name(nullptr), 
        value(nullptr)
        {};
	  ~header(){
        delete[] name; 
        delete[] value; 
        delete next;
        }
  };

  struct  URL {
        char*   scheme;
        char*   user;
        char*   pwd;
        char*   host;
        int     port;
        char*   path;
        char*   query;
        char*   fragment; 
        URL():
            scheme(nullptr),
            user(nullptr),
            pwd(nullptr),
            host(nullptr),
            port(80),
            path(nullptr),
            query(nullptr),
            fragment(nullptr)
            {};
        ~URL(){
            delete[] scheme;
            delete[] user;
            delete[] pwd;
            delete[] host;
            delete[] path;
            delete[] query;
            delete[] fragment;
        }
    };

    typedef std::function<void(void*, asyncHTTPrequest*, int readyState)> readyStateChangeCB;
    typedef std::function<void(void*, asyncHTTPrequest*, size_t available)> onDataCB;
	
  public:
    asyncHTTPrequest();
	~asyncHTTPrequest();

     
    //External functions in typical order of use:
    //__________________________________________________________________________________________________________*/
    void    setDebug(bool);                                         // Turn debug message on/off

	bool	open(const char* /*GET/POST*/, const char* URL);        // Initiate a request
    void    onReadyStateChange(readyStateChangeCB, void* arg = 0);  // Optional event handler for ready state change
                                                                    // or you can simply poll readyState()    
    void	setRxTimeout(int);                                      // overide default connect timeout (seconds)
    void    setAckTimeout(uint32_t);                                // overide default data ack timeout (ms)

    void	setReqHeader(const char* name, const char* value);      // add a request header     
	void	setReqHeader(const char* name, int32_t value);          // overload to use integer value     

	bool	send();                                                 // Send the request (GET)
	bool	send(String body);                                      // Send the request (POST)
	bool	send(const char* body);                                 // Send the request (POST)
    void    abort();                                                // Abort the current operation
    
    int		readyState();                                           // Return the ready state
	
	int		respHeaderCount();                                      // Retrieve count of response headers
	char*	respHeaderName(int index);                              // Return header name by index
	char*	respHeaderValue(int index);                             // Return header value by index
	char*	respHeaderValue(const char* name);                      // Return header value by name
	bool	respHeaderExists(const char* name);                     // Does header exist by name?
    String  headers();                                              // Return all headers as String

    void    onData(onDataCB, void* arg = 0);                        // Notify when min data is available
    size_t  available();                                            // response available
    int     responseHTTPcode();                                     // HTTP response code or (negative) error code
    String  responseText();                                         // response (whole* or partial* as string)
    String* responseStringPtr();                                    // response (whole* or partial* as String*)
    size_t  responseRead(uint8_t* buffer, size_t len);              // Read response into buffer
    uint32_t elapsedTime();                                         // Elapsed time of in progress transaction or last completed (ms)                                                                // Note, caller takes posession, responsible for delete
//___________________________________________________________________________________________________________________________________

  private:
  
	enum	{HTTPmethodGET,	HTTPmethodPOST} _HTTPmethod;
			
	enum	readyStates {
                readyStateUnsent = 0,
                readyStateOpened =  1,
                readyStateHdrsRecvd = 2,
                readyStateLoading = 3,
                readyStateDone = 4} _readyState;

    int16_t     _HTTPcode;
    bool        _connecting;
    bool        _chunked;
    bool        _debug;
    uint32_t    _RxTimeout;
    uint32_t    _AckTimeout;
    uint32_t    _requestStartTime;
    uint32_t    _requestEndTime;

    readyStateChangeCB  _readyStateChangeCB;
    void*               _readyStateChangeCBarg;
    onDataCB            _onDataCB;
    void*               _onDataCBarg; 
    size_t              _onDataCBmin;
		
	URL*        _URL;
    uint16_t    port;
	AsyncClient* _client;
    size_t      _contentLength;
    size_t      _contentRead;
    size_t      _notAcked;

	String*     _request;
	String*	    _response;

	header*	    _headers;

    header*     _addHeader(const char*, const char*);
    header*     _getHeader(const char*);
    header*     _getHeader(int);
    bool        _buildRequest(const char*);
    int         _strcmp_ci(const char*, const char*);
    bool        _parseURL(const char*);
    bool        _parseURL(String);
    bool        _connect();
    size_t      _send();
    void        _setReadyState(readyStates);

    void        _onConnect(AsyncClient*);
    void        _onDisconnect(AsyncClient*);
    void        _onData(void*, size_t);
    void        _onError(AsyncClient*, int8_t);
    void        _onTimeout(AsyncClient*);
    bool        _collectHeaders();
    
	
};
#endif 