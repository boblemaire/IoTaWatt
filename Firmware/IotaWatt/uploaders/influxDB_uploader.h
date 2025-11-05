#ifndef influxDB_uploader_h
#define influxDB_uploader_h

#include "IotaWatt.h"
#include "Uploader.h"

extern uint32_t influxDB_v1_dispatch(struct serviceBlock *serviceBlock);
extern Uploader* influxDB_v1;

class influxDB_uploader : public Uploader
{
    public:
        influxDB_uploader():
            _tagSet(0),
            _user(0),
            _pwd(0),
            _retention(0),
            _fieldKey(0),
            _database(0),
            _measurement(0),
            _staticKeySet(false),
            _heap(false)
        {
            _id = charstar("influxDB");
        };

        ~influxDB_uploader(){
            delete[] _measurement;
            delete[] _fieldKey;
            delete[] _retention;
            delete[] _database;
            delete[] _user;
            delete[] _pwd;
            delete[] _id;
            delete _tagSet;
            delete _outputs;
            influxDB_v1 = nullptr;
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

        char *_user;
        char *_pwd;
        char *_retention;
        char *_fieldKey;
        char *_database;
        char *_measurement;
        bool _staticKeySet;
        bool _heap;

        void queryLast();
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