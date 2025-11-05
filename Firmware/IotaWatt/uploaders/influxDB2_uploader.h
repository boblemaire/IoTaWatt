#ifndef influxDB2_uploader_h
#define influxDB2_uploader_h

#include "IotaWatt.h"
#include "Uploader.h"

extern uint32_t influxDB_v2_dispatch(struct serviceBlock *serviceBlock);
extern Uploader*influxDB_v2;

class influxDB2_uploader: public Uploader
{
    public:
        influxDB2_uploader(): 
            _tagSet(0),
            _lookbackHours(0),
            _orgID(0),
            _bucket(0),
            _token(0),
            _measurement(0),
            _fieldKey(0),
            _staticKeySet(false),
            _heap(false)
        {
            _id = charstar("influxDB2");
        };

        ~influxDB2_uploader(){
            delete[] _orgID;
            delete[] _bucket;
            delete[] _token;
            delete[] _measurement;
            delete[] _fieldKey;
            delete[] _id;
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
        bool _heap;

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