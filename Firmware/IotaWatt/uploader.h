#ifndef UPLOADER_H
#define UPLOADER_H
#include "IotaWatt.h"
#include "xurl.h"

#define DEFAULT_BUFFER_LIMIT 4000

extern uint32_t uploader_dispatch(struct serviceBlock *serviceBlock);

class uploader
{
    public:
        uploader() : 
                    newRecord(0),
                    oldRecord(0),
                    _state(initialize_s),
                    _url(0),
                    _request(0),
                    _interval(0),
                    _bulkSend(1),
                    _revision(-1),
                    _uploadStartDate(0),
                    _lastSent(0),
                    _lastPost(0),
                    _id(0),
                    _statusMessage(0),
                    _POSTrequest(0),
                    _outputs(0),
                    _script(0),
                    _stop(false),
                    _end(false),
                    _useProxyServer(true)

        {};
        
        ~uploader(){
            delete[] _statusMessage;
            delete _POSTrequest;
            delete _request;
            delete newRecord;
            delete oldRecord;
            delete _url;
        };

        uint32_t dispatch(struct serviceBlock *serviceBlock);
        virtual bool config(const char *JsonText);
        virtual bool configCB(JsonObject&);
        virtual void getStatusJson(JsonObject&);
        virtual void stop();
        virtual void end();
        virtual void setRequestHeaders();

    protected:

        IotaLogRecord *newRecord;
        IotaLogRecord *oldRecord;

        enum states {
            initialize_s,
            query_s,
            checkQuery_s,
            write_s,
            checkWrite_s,
            HTTPpost_s,
            HTTPwait_s,
            delay_s,
            stopped_s
        } _state;

        // Parameters supplied to HTTPost                

        struct POSTrequest{
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
        int32_t _revision;
        uint32_t _delayResumeTime;
        states _delayResumeState;
        uint32_t _uploadStartDate;
        bool _stop;
        bool _end;
        bool _useProxyServer;

        uint32_t _lastSent;
        uint32_t _lastPost;
        uint32_t _HTTPtoken;
        char *_id;
        char *_statusMessage;
        POSTrequest *_POSTrequest;
        ScriptSet *_outputs;
        Script *_script;

        virtual uint32_t handle_initialize_s();
        virtual uint32_t handle_query_s() = 0;
        virtual uint32_t handle_checkQuery_s() = 0;
        virtual uint32_t handle_write_s() = 0;
        virtual uint32_t handle_checkWrite_s() = 0;
        virtual uint32_t handle_stopped_s();
        virtual uint32_t handle_HTTPpost_s();
        virtual uint32_t handle_HTTPwait_s();
        virtual uint32_t handle_delay_s();

        virtual void HTTPPost(const char *endpoint, states completionState, const char *contentType);
        virtual void delay(uint32_t seconds, states resumeState);
};

#endif