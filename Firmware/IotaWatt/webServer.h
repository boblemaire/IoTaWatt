#ifndef webServer_h
#define webServer_h
#include "IotaWatt.h"
#include "auth.h"
#include <libb64/cdecode.h>

extern const char txtPlain_P[];
extern const char appJson_P[];
extern const char txtJson_P[];

 typedef std::function<void(void)> genericHandler;

void handleRequest();
bool authenticate(authLevel);
bool serverOn(authLevel level, const __FlashStringHelper* uri, HTTPMethod method, genericHandler fn);
void returnOK();
void returnFail(String msg, int code=500);
bool loadFromSdCard(String path);
bool loadFromSpiffs(String path, String dataType);
void handleFileUpload();
void handleSpiffsUpload();
void deleteRecursive(String path);
void handleDelete();
void handleCreate();
void printDirectory();
void printSpiffsDirectory(String path);
void handleNotFound();
void handleStatus();
void handleVcal();
void handleCommand();
void handleGetFeedList();
void handleGetFeedData();
void handleGraphCreate();
void handleGraphUpdate();
void handleGraphGetall();
void handleGraphGetallplus();
void handleGraphDelete();
void sendMsgFile(File &dataFile, int32_t relPos);
void handlePasswords();
void handleQuery();
void handleUpdate();
void handleDSTtest();

#endif