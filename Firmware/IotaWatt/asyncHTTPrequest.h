#ifndef asyncHTTPrequest_h
#define asyncHTTPrequest_h

#ifndef DEBUG_IOTA_PORT
#define DEBUG_IOTA_PORT Serial
#endif

#ifdef DEBUG_IOTA_HTTP
#define DEBUG_IOTA_HTTP_SET true
#else
#define DEBUG_IOTA_HTTP_SET false
#endif

#define DEBUG_HTTP(...)  if(_debug){DEBUG_IOTA_PORT.print("Debug: "); DEBUG_IOTA_PORT.printf(__VA_ARGS__ );}

#define DEFAULT_RX_TIMEOUT 3                    // Seconds for connect timeout
#define DEFAULT_ACK_TIMEOUT 2000                // Ms for ack timeout

#include <Arduino.h>
#include <ESPasyncTCP.h>
#include <cbuf.h>

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
	  char*			key;
	  char*			value;
	  header():
        next(nullptr), 
        key(nullptr), 
        value(nullptr)
        {};
	  ~header(){
        delete[] key; 
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

    typedef std::function<void(asyncHTTPrequest*, void*)> timeoutCBhandler;
	
  public:
    asyncHTTPrequest();
	~asyncHTTPrequest();

    void    setDebug(bool);
	
	bool	open(const char*, const char*);
	bool	send();
	bool	send(String);
	bool	send(const char*);
    void    end();
    uint32_t elapsedTime();
	
	void	setRxTimeout(int);
    void    setAckTimeout(uint32_t);
	void	onTimeout(timeoutCBhandler, void*);
	
	void	setReqHeader(const char*, const char*);
	void	setReqHeader(const char*, int32_t);
	
	int		respHeaderCount();
	char*	respHeaderKey(int);
	char*	respHeaderValue(const char*);
	char*	respHeaderValue(int);
	bool	respHeaderExists(const char*);

    int     responseHTTPcode();
    String  responseText();
    String* responseStringPtr();

    String  headers();  // temp

    
	int		readyState();
		
  private:
  
	enum	{HTTPmethodGET,
			HTTPmethodPOST} _HTTPmethod;
			
	enum	readyStates {readyStateUnsent = 0,
			readyStateOpened =  1,
			readyStateHdrsRecvd = 2,
			readyStateLoading = 3,
			readyStateDone = 4} _readyState;

    int16_t _HTTPcode;
    bool    _debug;
    bool    _connecting;
    uint32_t _RxTimeout;
    uint32_t _AckTimeout;
    uint32_t _requestStartTime;
    uint32_t _requestEndTime;
    timeoutCBhandler _timeoutCBhandler;
    void*   _timeoutCBparm;

			
	URL*    _URL;
    char*   _host;
    uint16_t port;
	AsyncClient* _client;
    int     _responseCode;
    size_t  _contentLength;

	String* request;
	String*	response;
	
	header*	_headers;

    header* _addHeader(const char*, const char*);
    header* _getHeader(const char*);
    header* _getHeader(int);
    void    _buildRequest(const char*);
    int     _strcmp_ci(const char*, const char*);
    bool    _parseURL(const char*);
    bool    _parseURL(String);
    bool    _connect();
    size_t  _send();
    void    _setReadyState(readyStates);

    void    _onConnect(AsyncClient*);
    void    _onDisconnect(AsyncClient*);
    void    _onData(void*, size_t);
    void    _onError(AsyncClient*, int8_t);
    void    _onTimeout(AsyncClient*);
    void    _collectHeaders();
    
	
};
#endif 