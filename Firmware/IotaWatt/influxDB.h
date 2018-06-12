#pragma once

#include "IotaWatt.h"
#include "xbuf.h"

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
};

uint32_t influxService(struct serviceBlock* _serviceBlock);
bool influxConfig(const char*);
String influxVarStr(const char*, Script*);

extern bool     influxStarted;                    // set true when Service started
extern bool     influxStop;                       // set true to stop the Service
extern bool     influxRestart;
extern uint32_t influxLastPost;                   // Timestamp of last cuccessful post

extern uint16_t influxBulkSend;
extern uint16_t influxPort;
extern int32_t  influxRevision;                   // Revision control for dynamic config
extern uint32_t influxBeginPosting;               // time to begin posting new dataset
extern char*    influxUser;
extern char*    influxPwd;
extern char*    influxRetention;
extern char*    influxMeasurement;
extern char*    influxFieldKey;
extern char*    influxURL;
extern char*    influxDataBase;
extern influxTag* influxTagSet;  
extern ScriptSet* influxOutputs;