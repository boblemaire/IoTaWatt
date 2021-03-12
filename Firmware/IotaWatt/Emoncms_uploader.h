#ifndef emoncms_uploader_h
#define emoncms_uploader_h

#include "IotaWatt.h"

extern uint32_t emoncms_dispatch(struct serviceBlock *serviceBlock);

class emoncms_uploader : public uploader 
{
    public:
        emoncms_uploader() :_node(0),
                            _userID(0),
                            _sha256(0),
                            _base64Sha(0),
                            _revision(0),
                            _encrypt(false),
                            _encrypted(false)
        {
            _id = charstar("emoncms");
        };

        ~emoncms_uploader(){
            
            Emoncms = nullptr;
        };

        bool configCB(const char *JsonText);
        uint32_t dispatch(struct serviceBlock *serviceBlock);

    protected:
        char *_node;
        char *_userID;
        uint8_t *_sha256;
        char *_base64Sha;
        uint8_t _cryptoKey[16];
        int _revision;
        bool _encrypt;
        bool _encrypted;

        void     queryLast();
        uint32_t handle_query_s();
        uint32_t handle_checkQuery_s();
        uint32_t handle_write_s();
        uint32_t handle_checkWrite_s();
        bool     configCB(JsonObject &);

        void     setRequestHeaders();
        int      scriptCompare(Script *a, Script *b);
};

#endif