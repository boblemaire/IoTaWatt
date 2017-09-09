#ifndef webServer_h
#define webServer_h

#include <Arduino.h>

void returnOK();  
void returnFail(String msg);

void handleFileUpload();
void handleDelete();
void handleCreate();
void handleNotFound();
void handleStatus();
void handleVcal();
void handleCommand();
void handleGetFeedList();
void handleGraphGetall();
void handleGetConfig();
void handleDisconnect();
  
void printDirectory();
  
#endif // !webServer_h