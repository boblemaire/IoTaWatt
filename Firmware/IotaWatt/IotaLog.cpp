/*
  IotaLog - Library for IoTaLog energy monitor
  Created by Bob Lemaire
*/
#include "IotaLog.h"
	#define PRINT(txt,val) Serial.print(txt); Serial.print(val);      // Quick debug aids
#define PRINTL(txt,val) Serial.print(txt); Serial.println(val);
	int IotaLog::begin (char* path){
		logPath = String(path) + ".log";
		indexPath = String(path) + ".ndx";
		if(!SD.exists((char*)logPath.c_str())){
			IotaFile = SD.open((char*)logPath.c_str(), FILE_WRITE);
			if(!IotaFile){
				Serial.println("open failed");
				Serial.println(logPath);
				return 2;
			}
			IotaFile.close();
			SD.remove((char*)indexPath.c_str());
			IotaIndex = SD.open((char*)indexPath.c_str(),FILE_WRITE);
			IotaIndex.close();
		}
		IotaFile = SD.open((char*)logPath.c_str(), FILE_WRITE);
		if(!IotaFile){
			return 2;
		}
		
		_fileSize = IotaFile.size();
				
		if(_fileSize % sizeof(IotaLogRecord)){
			Serial.println(_fileSize);
			Serial.println(sizeof(IotaLogRecord));
			IotaFile.close();
			return 3;
		}
		record = new IotaLogRecord;
		if(!_fileSize){
			_firstKey = 0;
			_lastKey = 0;
			_entries = 0;
		}
		else {
			IotaFile.seek(0);
			IotaFile.read(record, sizeof(IotaLogRecord));
			_firstKey = record->UNIXtime;
			IotaFile.seek(_fileSize-sizeof(IotaLogRecord));
			IotaFile.read(record, sizeof(IotaLogRecord));
			_lastKey = record->UNIXtime;
			_entries = _fileSize / sizeof(IotaLogRecord);
		}
		
		_L1indexBuffer = new IotaL1indexEntry [64];
		_L1indexBufferPos = 0xffffffff;
		
		return buildIndex();
	}
	
	int IotaLog::buildIndex(void){
		IotaIndex = SD.open((char*)indexPath.c_str(), FILE_READ);
		if(!IotaIndex){
			return 4;
		}
		_L1indexSize = IotaIndex.size();
		_L1entries = _L1indexSize / 8;
		uint32_t _L2range = _L2maxEntries;
		while(_L1entries > _L2range){
			_L2range *= 2;
		}
		_L1clusterEntries = _L2range / _L2maxEntries;
		_L2entries = (_L1entries + _L1clusterEntries - 1) / _L1clusterEntries;
		_L2index = new uint32_t [_L2entries];
		uint32_t* ptr = _L2index;
		for(int block = 0; block < _L2entries; block++){
			IotaIndex.seek(block * _L1clusterEntries * 8);
			IotaIndex.read(ptr, 4);
			ptr++;
		}
		_seriesKey = 0xffffffff;
		_L1indexBufferPos = 0xffffffff;
		return 0;
	}
	
	int IotaLog::write (IotaLogRecord* newRecord){
		if(!IotaFile){
			return 2;
		}
		if(newRecord->UNIXtime <= _lastKey) {
			return 1;
		}
		newRecord->serial = _entries++;
		IotaFile.seek(_fileSize);
		IotaFile.write((char*)newRecord, sizeof(IotaLogRecord));
		if(_firstKey == 0){
			_firstKey = newRecord->UNIXtime;
		}
		_fileSize += sizeof(IotaLogRecord);
		IotaFile.flush();
		if(newRecord->UNIXtime - _lastKey > _interval){
			IotaIndex.close();
			IotaIndex = SD.open((char*)indexPath.c_str(),FILE_WRITE);
			IotaIndex.write((char*)newRecord,8);
			IotaIndex.close();
			_L1entries++;
			_L1indexBufferPos = 0xffffffff;
			if(_L1entries > _L2entries * _L1clusterEntries){
				delete[] _L2index;
				buildIndex();
			}
		}
		_lastKey = newRecord->UNIXtime;
		return 0;
	}
	
	int IotaLog::readKey (IotaLogRecord* callerRecord){
		if(!IotaFile){
			return 2;
		}
		if(_entries == 0){
			return 1;
		}
		_callerRecord = callerRecord;
		uint32_t key = _callerRecord->UNIXtime - (_callerRecord->UNIXtime % _interval);
		
		if(key < _firstKey || key > _lastKey){
			return 1;
		}
			
				// Search L2 index for L1 block containing key.

			
		if(key < _seriesKey || key >= _seriesNextKey) {
			int32_t _L1cluster = _L2entries - 1;
			while(key < _L2index[_L1cluster--]);
			uint32_t L1Position = ++_L1cluster * _L1clusterEntries * 8;
			readL1index(L1Position);
			do{
				_seriesKey = _L1indexEntry->UNIXtime;
				_seriesSerial = _L1indexEntry->serial;
				L1Position += 8;
				readL1index(L1Position);
			} while(key >= _L1indexEntry->UNIXtime);
			_seriesEntries = _L1indexEntry->serial - _seriesSerial;
			_seriesNextKey = _L1indexEntry->UNIXtime;
		}
		
		uint32_t _seriesOffset = (key - _seriesKey) / _interval;
		if(_seriesOffset >= _seriesEntries){
			_seriesOffset = _seriesEntries - 1;
		}
		IotaFile.seek((_seriesSerial + _seriesOffset) * sizeof(IotaLogRecord));
		IotaFile.read(_callerRecord, sizeof(IotaLogRecord));
		_callerRecord->UNIXtime = key;
		return 0;
	}

			// readL1index() - reads an L1 index entry.
			// This function maintains a one block (512) buffer
			// of the L1 Index and services requests from that buffer.
			// The SD routines don't buffer, so otherwise these requests
			// would each be generating 512 byte reads to service an 8 byte request.
			// The performance improvement is huge.
	
	void IotaLog::readL1index(uint32_t pos){
		if(pos >= _L1indexSize){
			_L1indexEntry->UNIXtime = 0xffffffff;
			_L1indexEntry->serial = 0xffffffff;
			return;
		}
		if(pos < _L1indexBufferPos || pos >= (_L1indexBufferPos + 512)){
			_L1indexBufferPos = pos - (pos % 512);
			IotaIndex.seek(_L1indexBufferPos);
			uint32_t length = _L1indexSize - _L1indexBufferPos;
			if(length > 512) length = 512;
			IotaIndex.read((char*)_L1indexBuffer, length);
		}
		uint32_t bufferIndex = (pos - _L1indexBufferPos) / 8;
		_L1indexEntry->UNIXtime = _L1indexBuffer[bufferIndex].UNIXtime;
		_L1indexEntry->serial = _L1indexBuffer[bufferIndex].serial;
	}
	
	int IotaLog::readNext (IotaLogRecord* callerRecord){
		if(!IotaFile){
			return 2;
		}
		if(callerRecord->serial >= (_entries - 1)){
			return 1;
		}
		IotaFile.seek(sizeof(IotaLogRecord) * (callerRecord->serial+1));
		IotaFile.read(callerRecord, sizeof(IotaLogRecord));
		return 0;
	}
			
	int IotaLog::end(){
		logPath = "";
		indexPath = "";
		delete[] _L2index;
		delete[] _L1indexBuffer;
		_firstKey = 0;
		_lastKey = 0;
		_fileSize = 0;
		IotaFile.close();
		IotaIndex.close();
		return 0;
	}
	
	boolean IotaLog::isOpen(){
		if(IotaFile) return true;
		return false;
	}
	
	uint32_t IotaLog::firstKey(){return _firstKey;}
	uint32_t IotaLog::lastKey(){return _lastKey;}
	uint32_t IotaLog::fileSize(){return _fileSize;}

 




