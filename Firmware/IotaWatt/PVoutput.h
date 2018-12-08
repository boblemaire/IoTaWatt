#pragma once
#include "IotaWatt.h"

#define UNIX_DAY 86400UL
#define PV_MISSING_LIMIT 50                         // maximum missing outputs returned in one request
#define PV_DEFAULT_RATE_LIMIT 60                    // writes per hour default
#define PV_DONATOR_RATE_LIMIT 300                   // writes per hour donator
#define PV_DEFAULT_OUTPUT_LIMIT 30                  // max outputs added per batch default
#define PV_DONATOR_OUTPUT_LIMIT 100                 // max outputs added per batch donator
#define PV_DEFAULT_STATUS_LIMIT 30                  // max status added per batch default
#define PV_DONATOR_STATUS_LIMIT 100                 // max status added per batch donator
#define PV_DEFAULT_STATUS_DAYS 14                   // max status update lookback days default
#define PV_DONATOR_STATUS_DAYS 90                   // max status update lookback days donator

class PVresponse {
    private:
        char*       _response;

    public:
        PVresponse(asyncHTTPrequest* request);
        ~PVresponse();
        size_t      sections();
        size_t      items(int section=0);
        int32_t     parsel(int section, int item);
        uint32_t    parseul(int section, int item);
        String      parseString(int section, int item);
        char*       parsePointer(int section, int item);
        uint32_t    parseDate(int section, int item);
        uint32_t    parseTime(int section, int item);
        void        print();
        String      peek(int length, int offset=0);
        size_t      length();
        bool        contains(const char *str);
        bool        contains(const __FlashStringHelper *str);
    
 };

class PVoutput {

public:

    PVoutput()
        :_interval(0)
        ,_apiKey(nullptr)
        ,_systemID(nullptr)
        ,_revision(-1)
        ,_beginPosting(0)
        ,_lastMissing(0)
        ,_missingQ(nullptr)
        ,_state(initialize)
        ,_started(false)
        ,_stop(false)
        ,_restart(false)
        ,_end(false)
        ,_donator(false)
        ,_reload(false)
        ,_HTTPtoken(0)
        ,_outputs(nullptr)
        ,request(nullptr)
        ,response(nullptr)
        ,_lastPostTime(0)
        ,_lastReqTime(0)
        ,oldRecord(new IotaLogRecord)
        ,newRecord(nullptr)
        ,_POSTrequest(nullptr)
        ,_rateLimitReset(0)
        ,_baseTime(0)
        {};

    ~PVoutput(){
        delete _missingQ;
        delete[] _apiKey;
        delete oldRecord;
        delete newRecord;
        delete _outputs;
        delete request;
        delete response;
        delete _POSTrequest;
    };

    bool config(const char* jsonText);
    void stop();
    void end();
    void restart();
    void getStatusJson(JsonObject&);
    uint32_t tick(struct serviceBlock* serviceBlock);

private:

    enum    states     {initialize, 
                        getSystemService,
                        checkSystemService,
                        gotSystemService,
                        getMissingList,
                        checkMissingList,
                        gotMissingList,
                        uploadMissing,
                        checkUploadMissing,
                        getStatus,
                        gotStatus,
                        uploadStatus,
                        checkUploadStatus, 
                        HTTPpost, 
                        HTTPwait,
                        limitWait,
                        stopped,
                        noRetry
                    } _state;

    enum HTTPresponses {OK,
                        LOAD_IN_PROGRESS,
                        DATE_TOO_OLD,
                        DATE_IN_FUTURE,
                        RATE_LIMIT,
                        NO_STATUS,
                        MOON_POWERED,
                        HTTP_FAILURE
                    } _HTTPresponse;

    struct      POSTrequest{
        char*   URI;
        char*   contentType;
        states  completionState;
        POSTrequest():URI(nullptr),contentType(nullptr){};
        ~POSTrequest(){delete[] URI; delete[] contentType;}
    };

    struct      missingQ {
        missingQ*   next;
        uint32_t    first;
        uint32_t    last;
        missingQ():next(nullptr), first(0), last(0){};
        ~missingQ(){delete next;};
    };
    
    uint16_t    _interval;                  // Status interval from getSystemService
    char*       _apiKey;                    // From configuration
    char*       _systemID;                  // From configuration
    int32_t     _revision;                  // Configuration change identifier
    uint32_t    _beginPosting;              // Earliest date to begin posting as appropriate
    uint32_t    _lastMissing;               // Used for context while building missing outputs list
    missingQ*   _missingQ;                  // Head of queue of missing outputs
    bool        _started;                   // Initialization has been completed
    bool        _stop;                      // Set to stop and idle (_state = stopped) ASAP    
    bool        _restart;                   // Restart by setting (_state = getSystemService) ASAP
    bool        _end;                       // End the service and delete the instance of PVoutput ASAP
    bool        _donator;                   // PVoutput indicates donation made in getSystemService
    bool        _reload;                    // Reload all PVoutput data from _beginPosting as appropriate             
    uint32_t    _HTTPtoken;                 // Token used as identifier for HTTP subsystem resource          
    ScriptSet*  _outputs;                   // Output scripts
    asyncHTTPrequest* request;              // Instance of asyncHTTPrequest used for HTTP GET/PUT
    PVresponse* response;                   // Instance of response class used to parse response data
    xbuf        reqData;                    // Instance of xbuf used to build output and status batches
    uint32_t    _lastPostTime;              // Local time of last output or status posted to PVoutput
    uint32_t    _lastReqTime;               // Local time of last output or status in reqData or posted not confirmed
    IotaLogRecord* oldRecord;               // Older of two log records bracketing a reporting interval
    IotaLogRecord* newRecord;               // Newer of two log records bracketing a reporting interval    
    POSTrequest* _POSTrequest;              // Details of active POST request

    int32_t     _rateLimitLimit;            // Flow control from PVoutput response headers
    int32_t     _rateLimitRemaining;
    uint32_t    _rateLimitReset;            // UTC time when rate limit will be reset

    int         _reqEntries;                // Output or status entries in reqData
    double      _baseConsumption;           // Energy consumption at start of current reporting day
    double      _baseGeneration;            // Energy generation at start of current reporting day
    uint32_t    _baseTime;                  // Local date (UNIXtime 00:00:00) of above base values

    uint32_t    tickInitialize();
    uint32_t    tickGetSystemService();
    uint32_t    tickCheckSystemService();
    uint32_t    tickGotSystemService();
    uint32_t    tickGetMissingList();
    uint32_t    tickCheckMissingList();
    uint32_t    tickGotMissingList();
    uint32_t    tickUploadMissing();
    uint32_t    tickCheckUploadMissing();
    uint32_t    tickGetStatus();
    uint32_t    tickGotStatus();
    uint32_t    tickUploadStatus();
    uint32_t    tickCheckUploadStatus();
    uint32_t    tickHTTPPost();
    uint32_t    tickHTTPWait();
    uint32_t    tickLimitWait();
    uint32_t    tickStopped();

    void        HTTPPost(const char* URI, const char* contentType, states completionState);
    
enum        HTTP_status {
                    HTTP_SUCCESS,
                    HTTP_LIMIT,
                    HTTP_DONATOR,
                    HTTP_INVALID
                    } _HTTP_status;

};

extern PVoutput* pvoutput;

uint32_t PVOutputTick(struct serviceBlock* serviceBlock);
   