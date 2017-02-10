/*
  IotaLog - Library for IoTaLog energy monitor
  Created by Bob Lemaire 
*/
#include "Iotalog.h"

	int IotaLog::create(char* logFileName, uint32_t entryLength){
		IotaFile = SD.open(logFileName, FILE_WRITE);
		if(!IotaFile){
			return 2;
		}
		fileName = String(logFileName);
		IotaFile.write((char*)&header,sizeof(IotaHeaderRecord));
		_fileSize = sizeof(IotaHeaderRecord);
		IotaFile.close();
		return 0;
		
	}
	
	int IotaLog::begin (char* logFileName){
		if(!SD.exists(logFileName)){
			create(logFileName, sizeof(IotaLogRecord));
		}
		IotaFile = SD.open(logFileName, FILE_READ);
		if(!IotaFile){
			return 2;
		}
		fileName = String(logFileName);
		_fileSize = IotaFile.size();
		IotaFile.read((char*)&header,sizeof(IotaHeaderRecord));
		if((_fileSize - header.headerLength) % header.entryLength){
			IotaFile.close();
			return 3;
		}
		record = new IotaLogRecord;
		if(_fileSize == sizeof(IotaHeaderRecord)){
			_firstKey = 0;
			_lastKey = 0;
			_entries = 0;
		}
		else {
			IotaFile.read(record, header.entryLength);
			_firstKey = record->UNIXtime;
			_lastReadKey = record->UNIXtime;
			_lastReadSerial = record->serial;
			IotaFile.seek(_fileSize-header.entryLength);
			IotaFile.read(record, header.entryLength);
			_lastKey = record->UNIXtime;
			_entries = (_fileSize - header.headerLength) / header.entryLength;
		}
		IotaFile.close();
		return 0;
	}
	
	int IotaLog::write (IotaLogRecord* newRecord){
		if(newRecord->UNIXtime <= _lastKey)return 1;
		if(_fileState == openRead){
			IotaFile.close();
			_fileState = fileClosed;
		}
		if(_fileState != openWrite){
			IotaFile = SD.open(fileName, FILE_WRITE);
			_fileState = openWrite;
		}
		if(!IotaFile){
			_fileState = fileClosed;
			return 2;
		}
		newRecord->serial = _entries++;
		IotaFile.seek(_fileSize);
		IotaFile.write((char*)newRecord, header.entryLength);
		_fileSize += header.entryLength;
		_lastKey = newRecord->UNIXtime;
		IotaFile.flush();
		return 0;
	}
	
	int IotaLog::readKey (IotaLogRecord* callerRecord){
		_searchReads = 0;
		if(_entries == 0)return 1;
		_callerRecord = callerRecord;
		uint32_t key = _callerRecord->UNIXtime;
		uint32_t lowKey = _firstKey;
		uint32_t lowRec = 0;
		uint32_t highKey = _lastKey;
		uint32_t highRec = _entries-1;
		uint32_t thisRec = 0;
				
		if(key < _firstKey || key > _lastKey)return 2;
		if(_fileState == fileClosed){
			IotaFile = SD.open(fileName, FILE_READ);
			_fileState = openRead;
		}
		
		if(key == _lastReadKey){
			thisRec = _lastReadSerial;
		} 
		else if(key < _lastReadKey){
			thisRec = search(key, lowKey, lowRec, _lastReadKey, _lastReadSerial);
		}
		else {
			IotaFile.seek(header.headerLength + header.entryLength * (_lastReadSerial + 1));
			IotaFile.read(_callerRecord, 4);
			_searchReads++;
			if(key <_callerRecord->UNIXtime) {
				thisRec = _lastReadSerial;
			}
			else {
				_lastReadSerial++;
				_lastReadKey = callerRecord->UNIXtime;
				thisRec = search(key, _lastReadKey, _lastReadSerial, highKey, highRec);
			}
		}
		IotaFile.seek(header.headerLength + header.entryLength * thisRec);
		IotaFile.read(_callerRecord, header.entryLength);
		_lastReadKey = _callerRecord->UNIXtime;
		_lastReadSerial = _callerRecord->serial;
		return 0;
	}
	
	int IotaLog::readNext (IotaLogRecord* callerRecord){
		if(callerRecord->serial >= (_entries - 1))return 1;
		IotaFile.seek(header.headerLength + header.entryLength * (callerRecord->serial+1));
		IotaFile.read(callerRecord, header.entryLength);
		return 0;
	}
		
	uint32_t IotaLog::search(uint32_t key, uint32_t lowKey, uint32_t lowRec,
						uint32_t highKey, uint32_t highRec){
		
		if(key == highKey) return highRec;
		if(key == lowKey)return lowRec;		
		if(highRec - lowRec == 1) return lowRec;
			
		uint32_t thisRec = (lowRec + highRec) / 2;
		uint32_t realityRec = lowRec + (key - lowKey) / _interval;
		
		if(thisRec > realityRec) thisRec = realityRec;
		if(thisRec <= lowRec)thisRec = lowRec + 1;
		if(thisRec >= highRec)thisRec = highRec - 1;
		IotaFile.seek(header.headerLength + header.entryLength * thisRec);
		IotaFile.read(_callerRecord, 4);
		_searchReads++;
		uint32_t thisKey = _callerRecord->UNIXtime;
		if(key == thisKey) return thisRec;
		if(key > thisKey){
			return search(key, thisKey, thisRec, highKey, highRec);
		} 
		else {
			return search(key, lowKey, lowRec, thisKey, thisRec);
		}	
	}
	
	int IotaLog::searchReads(){return _searchReads;}
						
	
	int IotaLog::end(){
		fileName = "";
		IotaHeaderRecord(header);
		_firstKey = 0;
		_lastKey = 0;
		_fileSize = 0;
		IotaFile.close();
		return 0;
	}
	
	boolean IotaLog::isOpen(){
		if(IotaFile) return true;
		return false;
	}
	
	uint32_t IotaLog::firstKey(){return _firstKey;}
	uint32_t IotaLog::lastKey(){return _lastKey;}
	uint32_t IotaLog::fileSize(){return _fileSize;}

 




