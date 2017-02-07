/*
  IotaLog.h - Library for IotaLog SD log files
  Created by Bob Lemaire, May 2016
*/

#ifndef IotaLog_h
#define IotaLog_h
#include "SPI.h"
#include "SD.h"

/*******************************************************************************************************
********************************************************************************************************
Class IotaLog

Maintains a log of fixed length, consecutive entries.
All entries must be written with increasing keys.
Entries are read by key value.
When reading by key, the entry with the requested or next lower key is returned with the requested key.

********************************************************************************************************
********************************************************************************************************/
struct IotaLogRecord {
			uint32_t UNIXtime;				// Time period represented by this record
			uint32_t serial;				// record number in file
			double logHours;				// Total hours of monitoring logged to date in this log	
			struct channels {
				double accum1;
				double accum2;
				channels(){accum1 = 0; accum2 = 0;}
			} channel[15];
			IotaLogRecord(){UNIXtime=0; serial=0; logHours=0;};
		};

class IotaLog
{
  public:
  		
		int begin (char* /* filepath */);
		int write (IotaLogRecord* /* pointer to record to be written*/);
		int readKey (IotaLogRecord* /* pointer to caller's buffer */);
		int readNext(IotaLogRecord* /* pointer to caller's buffer */);
		int end();
		boolean isOpen();
		uint32_t firstKey();
		uint32_t lastKey();
		uint32_t fileSize();
		int searchReads();
			
  private:
  
    struct IotaHeaderRecord {
		uint16_t headerLength;
		uint16_t entryLength;
		uint16_t logInterval;
		uint16_t version;
		IotaHeaderRecord(){headerLength=sizeof(IotaHeaderRecord);
					entryLength=sizeof(IotaLogRecord);
					logInterval=0;
					version=0;}	
		} header;
		
	File IotaFile;
	enum fileState {fileClosed, openWrite, openRead};
	fileState _fileState = fileClosed;
	
	IotaLogRecord* record;
	IotaLogRecord* _callerRecord;
	
	String fileName;
	uint32_t _firstKey = 0;
	uint32_t _lastKey=0;
	uint32_t _fileSize = 0;
	uint32_t _entries = 0;
	uint32_t _searchReads = 0;
	
	uint32_t _lastReadKey = 0;
	uint32_t _lastReadSerial = 0;
	uint32_t _interval = 5;
	uint32_t _sigKey = 10000000;
	
	int create(char* /*filepath */, uint32_t /* entry length */);
	uint32_t search(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
	
};



#endif
