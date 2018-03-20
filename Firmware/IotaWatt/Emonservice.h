#pragma once

#include "IotaWatt.h"
#include "xbuf.h"

String bin2hex(const uint8_t* in, size_t len);
String encryptData(String in, const uint8_t* key);
String base64encode(const uint8_t* in, size_t len);
uint32_t EmonService(struct serviceBlock* _serviceBlock);
bool EmonConfig(JsonObject& config);

extern bool     EmonStarted;                      // set true when Service started
extern bool     EmonStop;                         // set true to stop the Service
extern bool     EmonRestart;                      // Initialize or reinitialize EmonService
extern String   EmonURL;                          // These are set from the config file
extern uint16_t  EmonPort;
extern String   EmonURI;
extern String   apiKey;
extern uint8_t  cryptoKey[16];
extern String   node;
extern boolean  EmonSecure;
extern String   EmonUsername;
extern int16_t  EmonBulkSend;
extern int32_t  EmonRevision;
enum    EmonSendMode {
        EmonSendGET = 1,
        EmonSendPOSTsecure = 2
};
extern EmonSendMode EmonSend;
extern ScriptSet* emonOutputs;