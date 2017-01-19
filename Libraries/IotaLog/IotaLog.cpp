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
		if(_entries == 0)return 1;
		_callerRecord = callerRecord;
		uint32_t key = _callerRecord->UNIXtime;
		uint32_t lowKey = _firstKey;
		uint32_t lowRec = 0;
		uint32_t highKey = _lastKey;
		uint32_t highRec = _entries-1;
		if(key < _firstKey || key > _lastKey)return 2;
		if(_fileState == fileClosed){
			IotaFile = SD.open(fileName, FILE_READ);
			_fileState = openRead;
		}
		
		search(key, lowKey, lowRec, highKey, highRec);
		// IotaFile.close();
		return 0;
	}
	
	int IotaLog::readNext (IotaLogRecord* callerRecord){
		if(callerRecord->serial >= (_entries - 1))return 1;
		IotaFile.seek(header.headerLength + header.entryLength * (callerRecord->serial+1));
		IotaFile.read(_callerRecord, header.entryLength);
		return 0;
	}
		
	void IotaLog::search(uint32_t key, uint32_t lowKey, uint32_t lowRec,
						uint32_t highKey, uint32_t highRec){

		if(highRec - lowRec == 1){
			if(key == _callerRecord->UNIXtime)return;
			if(key == highKey){
				IotaFile.seek(header.headerLength + header.entryLength * highRec);
			}
			else {
				IotaFile.seek(header.headerLength + header.entryLength * lowRec);
			}
			IotaFile.read(_callerRecord, header.entryLength);
			return;
		}					
		uint32_t thisRec = lowRec + 0.5 + ((float(key - lowKey) / float(highKey - lowKey)) * float(highRec - lowRec));
		if(thisRec == lowRec)thisRec++;
		if(thisRec == highRec)thisRec--;
		IotaFile.seek(header.headerLength + header.entryLength * thisRec);
		IotaFile.read(_callerRecord, header.entryLength);
		uint32_t thisKey = _callerRecord->UNIXtime;
		if(key == thisKey) return;
		if(key > thisKey){
			search(key, thisKey, thisRec, highKey, highRec);
		} 
		else {
			search(key, lowKey, lowRec, thisKey, thisRec);
		}
		return;
	}
						
	
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

 




