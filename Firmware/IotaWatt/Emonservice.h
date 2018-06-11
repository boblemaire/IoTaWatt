#pragma once

#include "IotaWatt.h"
#include "xbuf.h"

String bin2hex(const uint8_t* in, size_t len);
String base64encode(const uint8_t* in, size_t len);
void   base64encode(xbuf*);
uint32_t EmonService(struct serviceBlock* _serviceBlock);
bool EmonConfig(const char*);

extern bool     EmonStarted;                      // set true when Service started
extern bool     EmonStop;                         // set true to stop the Service
extern bool     EmonRestart;                      // Initialize or reinitialize EmonService
extern uint32_t EmonLastPost;
extern char*    EmonURL;                          // These are set from the config file
extern char*    EmonURI;
extern char*    apiKey;
extern char*    emonNode;
extern char*    EmonUsername;
extern uint16_t EmonPort;
extern int16_t  EmonBulkSend;
extern int32_t  EmonRevision;
extern uint32_t EmonBeginPosting;
extern uint8_t  cryptoKey[16];
enum    EmonSendMode {
        EmonSendGET = 1,
        EmonSendPOSTsecure = 2
};
extern EmonSendMode EmonSend;
extern ScriptSet* emonOutputs;