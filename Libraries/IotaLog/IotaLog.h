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
				channels(){accum1 = 0;}
			} channel[30];
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
  			
struct IotaL1indexEntry {
			uint32_t UNIXtime;
			uint32_t serial;
		} L1indexEntry;
		
	IotaL1indexEntry* _L1indexEntry = &L1indexEntry;
	IotaL1indexEntry* _L1indexBuffer;
	uint32_t _L1indexBufferPos;
		
	File IotaFile;
	File IotaIndex;
	
	IotaLogRecord* record;
	IotaLogRecord* _callerRecord;
	
	String logPath;
	String indexPath;
	
	uint32_t _firstKey = 0;
	uint32_t _lastKey=0;
	uint32_t _fileSize = 0;
	uint32_t _entries = 0;
	
	// Posting interval to log. Currently tested only using 5.
	
	uint32_t _interval = 5;
	
	// Defines the L1 (SDfile), and L2 (array) indices.
	// L1 entries are an ordered list of the first UNIXtime/serial of each contigeous series in the log, 
	// contained in the NDX file.
	// L2 entries are an ordered list of the first UNIXtime of a cluster (group) of L1 entries.  The size
	// of the clusters are dynamically determined.
	
	uint32_t _L1indexSize = 0;				// Size of L1 index
	uint32_t _L1entries = 0;				// Entries in 1st level index
	uint32_t _L2entries = 0;				// Number of entries in 2nd level index
	uint32_t _L2maxEntries = 128; 			// Maximum 2nd level index entries
	uint32_t _L1clusterEntries = 1;			// 1st level entries indexed by one 2nd level entry
	uint32_t* _L2index;						// 2nd level index array pointer
	
	

	// Defines the last indexed series
	
	uint32_t _seriesKey = 0;
	uint32_t _seriesSerial = 0;
	uint32_t _seriesEntries = 0;
	uint32_t _seriesNextKey = 0;	
		
	uint32_t search(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
	int buildIndex(void);
	void readL1index(uint32_t);
	
};



#endif
