#ifndef influxDB_v2_uploader_h
#define influxDB_v2_uploader_h

#include "IotaWatt.h"

#ifndef INFLUXDB_V2_BUFFER_LIMIT
#define INFLUXDB_V2_BUFFER_LIMIT 4000
#endif

extern uint32_t influxDB_v2_tick(struct serviceBlock *serviceBlock);

class influxDB_v2_uploader : public uploader 
{
    public:
        influxDB_v2_uploader(): 
            _tagSet(0),
            _outputs(0),
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

        bool config(const char *JsonText);
        uint32_t tick(struct serviceBlock *serviceBlock);

    private:


        influxTag *_tagSet;
        ScriptSet *_outputs;

        uint32_t _lookbackHours;
        char *_orgID;
        char *_bucket;
        char *_token;
        char *_measurement;
        char *_fieldKey;
        bool _staticKeySet;
        bool _useProxyServer;

        uint32_t tickBuildLastSent();
        uint32_t tickCheckLastSent();
        uint32_t tickBuildPost();
        uint32_t tickCheckPost();

        void setRequestHeaders();
        String varStr(const char *in, Script *script);
        int scriptCompare(Script *a, Script *b);
};

#endif