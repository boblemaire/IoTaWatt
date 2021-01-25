#ifndef influxDB_v2_uploader_h
#define influxDB_v2_uploader_h

#include "IotaWatt.h"
#include "xbuf.h"
#include "xurl.h"

#define INFLUXDB_V2_BUFFER_LIMIT 4000

extern uint32_t influxDB_v2_tick(struct serviceBlock *serviceBlock);

class influxDB_v2_uploader
{
    public:
        influxDB_v2_uploader() : newRecord(0),
                                 oldRecord(0),
                                 _tagSet(0),
                                 _outputs(0),
                                 _state(initialize),
                                 _request(0),
                                 _interval(0),
                                 _bulkSend(1),
                                 _revision(0),
                                 _orgID(0),
                                 _bucket(0),
                                 _token(0),
                                 _measurement(0),
                                 _fieldKey(0),
                                 _lastSent(0),
                                 _lastPost(0),
                                 _bufferLimit(INFLUXDB_V2_BUFFER_LIMIT),
                                 _statusMessage(0),
                                 _stop(false),
                                 _end(false),
                                 _staticKeySet(false),
                                 _useProxyServer(true)

        {};
        
        ~influxDB_v2_uploader(){

        };

        bool config(const char *JsonText);
        void getStatusJson(JsonObject&);
        uint32_t tick(struct serviceBlock *serviceBlock);
        void end();

    private:
        IotaLogRecord *newRecord;
        IotaLogRecord *oldRecord;

        influxTag *_tagSet;
        ScriptSet *_outputs;

        enum {
            initialize,
            getLastPost,
            post,
            sendPost,
            waitPost,
            stopped
        } _state;

        xurl _url;
        xbuf reqData;
        asyncHTTPrequest *_request;

        int16_t _interval;
        int16_t _bulkSend;
        uint32_t _revision;
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
        uint32_t tickGetLastSent();
        uint32_t tickPost();
        uint32_t tickSendPost();
        uint32_t tickWaitPost();
        uint32_t tickStopped();

        String varStr(const char *in, Script *script);
        int scriptCompare(Script *a, Script *b);

};

extern influxDB_v2_uploader *influxDB_v2;
#endif