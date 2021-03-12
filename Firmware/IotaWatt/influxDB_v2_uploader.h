#ifndef influxDB_v2_uploader_h
#define influxDB_v2_uploader_h

#include "IotaWatt.h"

extern uint32_t influxDB_v2_dispatch(struct serviceBlock *serviceBlock);

class influxDB_v2_uploader : public uploader 
{
    public:
        influxDB_v2_uploader(): 
            _tagSet(0),
            _lookbackHours(0),
            _orgID(0),
            _bucket(0),
            _token(0),
            _measurement(0),
            _fieldKey(0),
            _staticKeySet(false)
        {
            _id = charstar("influxDB_v2");
        };

        ~influxDB_v2_uploader(){
            delete[] _orgID;
            delete[] _bucket;
            delete[] _token;
            delete[] _measurement;
            delete[] _fieldKey;
            delete _tagSet;
            delete _outputs;
            influxDB_v2 = nullptr;
        };

        bool configCB(const char *JsonText);
        uint32_t dispatch(struct serviceBlock *serviceBlock);

    private:


        struct influxTag {
            influxTag* next;
            char*      key;
            char*      value;
            influxTag()
                :next(nullptr)
                ,key(nullptr)
                ,value(nullptr)
                {}
            ~influxTag(){
                delete[] key;
                delete[] value;
                delete   next;
            }
        } *_tagSet;
        
        uint32_t _lookbackHours;
        char *_orgID;
        char *_bucket;
        char *_token;
        char *_measurement;
        char *_fieldKey;
        bool _staticKeySet;

        uint32_t handle_query_s();
        uint32_t handle_checkQuery_s();
        uint32_t handle_write_s();
        uint32_t handle_checkWrite_s();
        bool configCB(JsonObject &);

        void setRequestHeaders();
        String varStr(const char *in, Script *script);
        int scriptCompare(Script *a, Script *b);
};

#endif