#ifndef UPLOADER_H
#define UPLOADER_H
#include "IotaWatt.h"
#include "xurl.h"

#define DEFAULT_BUFFER_LIMIT 4000

extern void Declare_Uploader(const char *);

extern uint32_t uploader_dispatch(struct serviceBlock *serviceBlock);

class Uploader
{

    enum methods
        {
            method_GET,
            method_POST
        };

        
    public:

        
        Uploader() : 
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
        
                    ~Uploader(){
            delete[] _statusMessage;
            delete _POSTrequest;
            delete _request;
            delete newRecord;
            delete oldRecord;
            delete _url;
        };

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

        uint32_t dispatch(struct serviceBlock *serviceBlock);
        virtual bool config(const char *JsonText);
        virtual bool configCB(JsonObject&);
        virtual void getStatusJson(JsonObject&);
        virtual void stop();
        virtual void end();
        virtual void setRequestHeaders();
        virtual char *id() { return _id; };

        virtual uint32_t handle_initialize_s();
        virtual uint32_t handle_query_s() = 0;
        virtual uint32_t handle_checkQuery_s() = 0;
        virtual uint32_t handle_write_s() = 0;
        virtual uint32_t handle_checkWrite_s() = 0;
        virtual uint32_t handle_stopped_s();
        virtual uint32_t handle_HTTPpost_s();
        virtual uint32_t handle_HTTPwait_s();
        virtual uint32_t handle_delay_s();
        virtual void delay(uint32_t seconds, states resumeState);

        virtual void HTTPPost(const char *endpoint, states completionState, const char *contentType);
        virtual void HTTPGet(const char *endpoint, states completionState);
        virtual void HTTP(const char* method, const char *endpoint, states completionState, const char *contentType = nullptr);

    protected:

        IotaLogRecord *newRecord;
        IotaLogRecord *oldRecord;

             // Parameters supplied to HTTPost                

        struct POSTrequest{
            char*   endpoint;
            char*   contentType;
            methods method;
            states  completionState;
            POSTrequest():endpoint(nullptr),contentType(nullptr),method(method_POST){};
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

        
};

#endif