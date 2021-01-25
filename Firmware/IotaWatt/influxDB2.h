#pragma once

#include "IotaWatt.h"
#include "xbuf.h"

// struct influxTag {
//   influxTag* next;
//   char*      key;
//   char*      value;
//   influxTag()
//     :next(nullptr)
//     ,key(nullptr)
//     ,value(nullptr)
//     {}
//   ~influxTag(){
//     delete[] key;
//     delete[] value;
//     delete   next;
//   }
// };

uint32_t influx2Service(struct serviceBlock* _serviceBlock);
bool influx2Config(const char*);
String influx2VarStr(const char*, Script*);

extern bool     influx2Started;                    // set true when Service started
extern bool     influx2Stop;                       // set true to stop the Service
extern bool     influx2Restart;
extern uint32_t influx2LastPost;                   // Timestamp of last cuccessful post

extern uint16_t influx2BulkSend;
extern uint16_t influx2Port;
extern int32_t  influx2Revision;                   // Revision control for dynamic config
extern uint32_t influx2BeginPosting;               // time to begin posting new dataset
extern char*    influx2orgID;
extern char*    influx2Token;
extern char*    influx2Retention;
extern char*    influx2Measurement;
extern char*    influx2FieldKey;
extern char*    influx2Bucket;
extern influxTag* influx2TagSet;  
extern ScriptSet* influx2Outputs;