#ifndef emoncms_uploader_h
#define emoncms_uploader_h

#include "IotaWatt.h"

#ifndef emoncms_BUFFER_LIMIT
#define emoncms_BUFFER_LIMIT 4000
#endif

extern uint32_t emoncms_dispatch(struct serviceBlock *serviceBlock);

class emoncms_uploader : public uploader 
{
    public:
        emoncms_uploader():
            _tagSet(0),
            _outputs(0),
            _script(nullptr),
            _user(0),
            _pwd(0),
            _retention(0),
            _fieldKey(0),
            _database(0),
            _measurement(0),
            _staticKeySet(false)
        {
            _id = charstar("emoncms");
        };

        ~emoncms_uploader(){
            delete[] _measurement;
            delete[] _fieldKey;
            delete[] _retention;
            delete[] _database;
            delete[] _user;
            delete[] _pwd;
            delete _tagSet;
            delete _outputs;
            Emoncms = nullptr;
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
        ScriptSet *_outputs;
        Script *_script;

        char *_user;
        char *_pwd;
        char *_retention;
        char *_fieldKey;
        char *_database;
        char *_measurement;
        bool _staticKeySet;

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