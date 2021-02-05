#ifndef influxDB_v1_uploader_h
#define influxDB_v1_uploader_h

#include "IotaWatt.h"
#include "xbuf.h"
#include "xurl.h"

#ifndef influxDB_v1_BUFFER_LIMIT
#define influxDB_v1_BUFFER_LIMIT 4000
#endif

extern uint32_t influxDB_v1_tick(struct serviceBlock *serviceBlock);

class influxDB_v1_uploader
{
    public:
        influxDB_v1_uploader() : newRecord(0),
                                 oldRecord(0),
                                 _tagSet(0),
                                 _outputs(0),
                                 _state(initialize_s),
                                 _url(0),
                                 _request(0),
                                 _interval(0),
                                 _bulkSend(1),
                                 _revision(0),
                                 _lookbackHours(0),
                                 _uploadStartDate(0),
                                 _orgID(0),
                                 _bucket(0),
                                 _token(0),
                                 _measurement(0),
                                 _fieldKey(0),
                                 _lastSent(0),
                                 _lastPost(0),
                                 _bufferLimit(influxDB_v1_BUFFER_LIMIT),
                                 _statusMessage(0),
                                 _POSTrequest(0),
                                 _stop(false),
                                 _end(false),
                                 _staticKeySet(false),
                                 _useProxyServer(true)

        {};
        
        ~influxDB_v1_uploader(){
            delete[] _orgID;
            delete[] _bucket;
            delete[] _token;
            delete[] _measurement;
            delete[] _fieldKey;
            delete[] _statusMessage;
            delete _POSTrequest;
            delete _request;
            delete newRecord;
            delete oldRecord;
            delete _tagSet;
            delete _outputs;
            delete _url;
        };

        bool config(const char *JsonText);
        void getStatusJson(JsonObject&);
        uint32_t tick(struct serviceBlock *serviceBlock);
        uint32_t stop();
        void end();

    private:

        IotaLogRecord *newRecord;
        IotaLogRecord *oldRecord;

        influxTag *_tagSet;
        ScriptSet *_outputs;

        enum states {
            initialize_s,
            buildLastSent_s,
            checkLastSent_s,
            buildPost_s,
            checkPost_s,
            HTTPpost_s,
            HTTPwait_s,
            delay_s,
            stopped_s
        } _state;

        // Parameters supplied to HTTPost                

        struct      POSTrequest{
            char*   endpoint;
            char*   contentType;
            states  completionState;
            POSTrequest():endpoint(nullptr),contentType(nullptr){};
            ~POSTrequest(){delete[] endpoint; delete[] contentType;}
        };

        xurl* _url;
        xbuf reqData;
        asyncHTTPrequest *_request;

        int16_t _interval;
        int16_t _bulkSend;
        uint32_t _revision;
        uint32_t _delayResumeTime;
        states _delayResumeState;
        uint32_t _lookbackHours;
        uint32_t _uploadStartDate;
        char *_orgID;
        char *_bucket;
        char *_token;
        char *_measurement;
        char *_fieldKey;
        bool _stop;
        bool _end;
        bool _staticKeySet;
        bool _useProxyServer;

        uint32_t _lastSent;
        uint32_t _lastPost;
        uint32_t _bufferLimit;
        uint32_t _HTTPtoken;
        char *_statusMessage;

        uint32_t tickInitialize();
        uint32_t tickBuildLastSent();
        uint32_t tickCheckLastSent();
        uint32_t tickBuildPost();
        uint32_t tickCheckPost();
        uint32_t tickStopped();
        uint32_t tickHTTPPost();
        uint32_t tickHTTPWait();
        uint32_t tickDelay();

        POSTrequest *_POSTrequest;

        void HTTPPost(const char *endpoint, states completionState, const char *contentType);
        String varStr(const char *in, Script *script);
        int scriptCompare(Script *a, Script *b);
        void delay(uint32_t seconds, states resumeState);
};

#endif