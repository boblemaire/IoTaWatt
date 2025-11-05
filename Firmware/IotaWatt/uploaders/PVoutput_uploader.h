#ifndef PVoutput_h
#define PVoutput_h
#include "IotaWatt.h"
#include "Uploader_Registry.h"

//********************************************************************************************************
// PVoutput class
//
// This class provides everything needed to post data to the PVoutput.org service
//
//  It supports:
//      freeload and donator modes
//      Extended data values in donator mode
//      Loading and reloading of historical data within the constraints of PVoutput
//      PVoutput flow control limits
//      
//  This implementation of PVoutput is based on priliminary work by Brendon Costa 
//  who produced a working prototype with this basic class orientation, which is a huge
//  improvement over the approach taken in the initial export services for influxDB and
//  emoncms  This second cut adds support for donator mode and uses the IoTaWatt script 
//  system to produce the various data items that are exported in the output and status 
//  records. The logic was also simplified by new local time zone support that was not
//  available at the time of the first cut.
//
//********************************************************************************************************

        // Some useful definitions of universal constants and current limits of pvoutput

#define UNIX_DAY 86400UL
#define PV_REQDATA_LIMIT 3000                       // Maximum size of status batch
#define PV_DEFAULT_RATE_LIMIT 60                    // writes per hour default
#define PV_DONATOR_RATE_LIMIT 300                   // writes per hour donator
#define PV_DEFAULT_STATUS_LIMIT 30                  // max status added per batch default
#define PV_DONATOR_STATUS_LIMIT 100                 // max status added per batch donator
#define PV_DEFAULT_STATUS_DAYS 14                   // max status update lookback days default
#define PV_DONATOR_STATUS_DAYS 90                   // max status update lookback days donator

        // PVresponse class is used to capture the response from any PVoutut API request and make them
        // intelligible as sets of items delimited by commas within sets of sections delimited by semicolons.

class PVresponse {
    private:
        char*       _response;                                  // The raw response string

    public:
        PVresponse(asyncHTTPrequest* request);
        ~PVresponse();
        size_t      sections();                                 // number of sections in response
        size_t      items(int section=0);                       // number of items in given section
        int32_t     parsel(int section, int item);              // parse a given section/item as a long
        uint32_t    parseul(int section, int item);             // parse a given section/item as a unsigned long
        String      parseString(int section, int item);         // parse a given section/item as a String    
        char*       parsePointer(int section, int item);        // parse a given section/item as a char*
        uint32_t    parseDate(int section, int item);           // parse a given section/item as a YYYYMMDD date
        uint32_t    parseTime(int section, int item);           // parse a given section/item as a hh:mm time
        void        print();                                    // Print the whole response
        String      peek(int length, int offset=0);             // Return the whole response as a String
        size_t      length();                                   // Return strlen(_response)
        bool        contains(const char *str);                  // Check if _response contains a given string
        bool        contains(const __FlashStringHelper *str);   // Same with a _FlashStringHelper
    
 };

class PVoutput_uploader {

public:

    PVoutput_uploader()
        :_interval(0)
        ,_apiKey(nullptr)
        ,_systemID(nullptr)
        ,_revision(-1)
        ,_beginPosting(0)
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
        ,oldRecord(nullptr)
        ,newRecord(nullptr)
        ,_POSTrequest(nullptr)
        ,_statusMessage(nullptr)
        ,_rateLimitReset(0)
        ,_errorCode(0)
        ,_errorCount(0)
        ,_baseTime(0)
        {
            _id = charstar("PVoutput");
        };

    ~PVoutput_uploader(){
        delete[] _apiKey;
        delete[] _systemID;
        delete[] _statusMessage;
        delete[] _id;
        delete oldRecord;
        delete newRecord;
        delete _outputs;
        delete request;
        delete response;
        delete _POSTrequest;
    };

        // Methods available

    bool config(const char* jsonText);                  // Process configuration as a string of Json
    void stop();                                        // stop the state machine ASAP
    void end();                                         // Destroy this instance of the class ASAP
    void restart();                                     // Force a restart of the state machine ASAP
    void getStatusJson(JsonObject&);                    // Add status objects to the supplied Json object
    char *id() { return _id; };
    uint32_t tick(struct serviceBlock* serviceBlock);   // Invoke state machine execution

private:

        // Services operate as state machines to maintain context and synchronize.

    enum    states     {initialize = 0, 
                        getSystemService = 1,
                        checkSystemService = 2,
                        getStatus = 3,
                        gotStatus = 4,
                        uploadStatus = 5,
                        checkUploadStatus = 6, 
                        HTTPpost = 7, 
                        HTTPwait = 8,
                        limitWait =9,
                        stopped = 10,
                        invalid = 11
                    } _state, _resumeState;

    // State machine handlers corresponding to like named states.

    uint32_t    handle_initialize_s();
    uint32_t    tickGetSystemService();
    uint32_t    tickCheckSystemService();
    uint32_t    tickGetStatus();
    uint32_t    tickGotStatus();
    uint32_t    tickUploadStatus();
    uint32_t    tickCheckUploadStatus();
    uint32_t    handle_HTTPpost_s();
    uint32_t    handle_HTTPwait_s();
    uint32_t    tickLimitWait();
    uint32_t    handle_stopped_s();

        // Possible response from any HTTP request

    enum HTTPresponses {OK,
                        LOAD_IN_PROGRESS,
                        DATE_TOO_OLD,
                        DATE_IN_FUTURE,
                        RATE_LIMIT,
                        NO_STATUS,
                        MOON_POWERED,
                        HTTP_FAILURE,
                        UNRECOGNIZED
                    } _HTTPresponse;

        // Parameters supplied to HTTPost                

    struct      POSTrequest{
        char*   URI;
        char*   contentType;
        states  completionState;
        POSTrequest():URI(nullptr),contentType(nullptr){};
        ~POSTrequest(){delete[] URI; delete[] contentType;}
    };

        // function used by state handlers to transition to HTTPpost state

    void        HTTPPost(const __FlashStringHelper *URI, states completionState, const char *contentType = nullptr);

        // Class variables
    
    uint16_t    _interval;                  // Status interval from getSystemService
    char*       _apiKey;                    // From configuration
    char*       _systemID;                  // From configuration
    int32_t     _revision;                  // Configuration change identifier
    uint32_t    _beginPosting;              // Earliest date to begin posting as appropriate
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
    char        *_statusMessage;            // message for status query
    char        *_id;                       // UploaderID

    int32_t     _rateLimitLimit;            // Flow control from PVoutput response headers
    int32_t     _rateLimitRemaining;
    uint32_t    _rateLimitReset;            // UTC time when rate limit will be reset

    int         _errorCode;                 // last HTTP error code
    int         _errorCount;                // Count of repeated HTTP errors
    int         _reqEntries;                // Output or status entries in reqData
    double      _baseConsumption;           // Energy consumption at start of current reporting day
    double      _baseGeneration;            // Energy generation at start of current reporting day
    uint32_t    _baseTime;                  // Local date (UNIXtime 00:00:00) of above base values

};

extern PVoutput_uploader *PVoutput;

#endif  