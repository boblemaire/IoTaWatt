#ifndef webServer_h
#define webServer_h

 typedef std::function<void(void)> genericHandler;

void handleRequest();
bool authenticate(const char*);
bool serverOn(const __FlashStringHelper* uri, HTTPMethod method, genericHandler fn);
void returnOK();
void returnFail(String msg);
bool loadFromSdCard(String path);
void handleFileUpload();
void deleteRecursive(String path);
void handleDelete();
void handleCreate();
void printDirectory();
void handleNotFound();
void handleStatus();
void handleVcal();
void handleCommand();
void handleGetFeedList();
void handleGetFeedData();
void handleGraphCreate();
void handleGraphUpdate();
void handleGraphGetall();
void handleGraphDelete();
void sendMsgFile(File &dataFile, int32_t relPos);
void handleGetConfig();
void sendChunked(String response);

#endif