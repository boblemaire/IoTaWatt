#ifndef postgrest_uploader_h
#define postgrest_uploader_h

#include "IotaWatt.h"

extern uint32_t postgrest_dispatch(struct serviceBlock *serviceBlock);

class postgrest_uploader : public uploader 
{
    public:
        postgrest_uploader() :
            _table(0),
            _deviceName(0),
            _jwtToken(0),
            _schema(0),
            _GETrequest(0)
        {
            _id = charstar("postgrest");
        };

        ~postgrest_uploader(){
            delete[] _table;
            delete[] _deviceName;
            delete[] _jwtToken;
            delete[] _schema;
            if (_GETrequest) {
                delete _GETrequest;
            }
            postgrest = nullptr;
        };

        bool configCB(const char *JsonText);
        uint32_t dispatch(struct serviceBlock *serviceBlock);

    protected:
        char *_table;           // Database table name
        char *_deviceName;      // Device identifier (supports $device substitution)
        char *_jwtToken;        // JWT token for authentication
        char *_schema;          // Database schema name
        
        struct GETrequest{
            char*   endpoint;
            states  completionState;
            GETrequest():endpoint(nullptr){};
            ~GETrequest(){delete[] endpoint;}
        };
        GETrequest* _GETrequest;

        uint32_t handle_query_s();
        uint32_t handle_checkQuery_s();
        uint32_t handle_write_s();
        uint32_t handle_checkWrite_s();
        uint32_t handle_HTTPpost_s();
        bool configCB(JsonObject &);
        uint32_t parseTimestamp(const char* timestampStr);

        void setRequestHeaders();
        void HTTPGet(const char* endpoint, states completionState);
        int scriptCompare(Script *a, Script *b);
        String resolveDeviceName();
};

#endif
