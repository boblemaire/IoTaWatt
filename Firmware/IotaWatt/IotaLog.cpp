/*
  IotaLog - Library for IoTaLog energy monitor
  Created by Bob Lemaire
*/
#include "IotaWatt.h"
struct {
  uint32_t UNIXtime;
  uint32_t serial; 
} record;
int IotaLog::begin (const char* path ){
  if(IotaFile) return 0;	
  String logPath = String(path) + ".log";
	_path = new char[logPath.length()+1];
	strcpy(_path, logPath.c_str());
  if(!SD.exists(_path)){
		if(logPath.lastIndexOf('/') > 0){
			String  dir = logPath.substring(0,logPath.lastIndexOf('/'));
			if( ! SD.mkdir(dir)){
				Serial.printf_P(PSTR("mkdir failed: %s\r\n"), dir.c_str());
				return 2;
			}
		}
		IotaFile = SD.open(_path, FILE_WRITE);
		if(!IotaFile){
			return 2;
		}
		IotaFile.close();
  }
  IotaFile = SD.open(_path, FILE_WRITE);
	if(!IotaFile){
		return 2;
  }
  
  _fileSize = IotaFile.size();

  if(_fileSize){
		IotaFile.seek(0);
		IotaFile.read(&record, sizeof(record));
		_firstKey = record.UNIXtime;
		_firstSerial = record.serial;
		IotaFile.seek(_fileSize - sizeof(IotaLogRecord));
		IotaFile.read(&record, sizeof(record));
		_lastKey = record.UNIXtime;
		_lastSerial = record.serial;
		_entries = _fileSize / sizeof(IotaLogRecord);
  }

					// If there are trailing zero records at the end,
					// try to adjust _filesize down to match logical end of file.

	while(_fileSize && _lastSerial == 0){
		IotaLogRecord* record = new IotaLogRecord;
		_fileSize -= sizeof(IotaLogRecord);
		IotaFile.seek(_fileSize - sizeof(IotaLogRecord));
		IotaFile.read((char*)record, sizeof(IotaLogRecord));
		_lastSerial = record->serial;
		_lastKey = record->UNIXtime;
		_entries--;	 
		delete record;
	}
	if(_fileSize != IotaFile.size()){
		Serial.printf("physical %d, logical %d\r\n", IotaFile.size(), _fileSize);
	}
	
  if(_firstKey > _lastKey){
		_wrap = findWrap(0,_firstKey, _fileSize - sizeof(IotaLogRecord), _lastKey);
		IotaFile.seek(_wrap);
		IotaFile.read(&record, sizeof(record));
		_firstKey = record.UNIXtime;
		_firstSerial = record.serial;
		IotaFile.seek(_wrap - sizeof(IotaLogRecord));
		IotaFile.read(&record, sizeof(record));
		_lastKey = record.UNIXtime;
		_lastSerial = record.serial;
  }

  _lastReadKey = _firstKey;
  _lastReadSerial = _firstSerial;
  _maxFileSize = max(_fileSize, _maxFileSize);
  
  if(((int32_t) _lastSerial - _firstSerial + 1) != _entries){
		log("IotaLog: file damaged %s\r\n", _path);
		log("IotaLog: Creating diagnostic file.");
		dumpFile();
		log("IotaLog: Deleting %s and restarting.\r\n", _path);	
		IotaFile.close();
		SD.remove(_path);
		ESP.restart();
	}
	
	for(int i=0; i<_cacheSize; i++){
		_cacheKey[i] = _firstKey;
		_cacheSerial[i] = _firstSerial;
	}

  return 0;
}

uint32_t IotaLog::findWrap(uint32_t highPos, uint32_t highKey, uint32_t lowPos, uint32_t lowKey){
  struct {
	uint32_t UNIXtime;
	uint32_t serial; 
  } record;
  
  if((lowPos - highPos) == sizeof(IotaLogRecord)) {
		return lowPos;
  }
  uint32_t midPos = (highPos + lowPos) / 2;
  midPos += midPos % sizeof(IotaLogRecord);
  IotaFile.seek(midPos);
  IotaFile.read(&record, sizeof(record));
  uint32_t midKey = record.UNIXtime;
  if(midKey > highKey){
		return findWrap(midPos, midKey, lowPos, lowKey);
  }
  return findWrap(highPos, highKey, midPos, midKey);
}

int IotaLog::readKey (IotaLogRecord* callerRecord){
	uint32_t key = callerRecord->UNIXtime - (callerRecord->UNIXtime % _interval);
  if(!IotaFile) return 2;
  if(_entries == 0) return 1;
	if(key < _firstKey){													// Before the beginning of time
		readSerial(callerRecord, _firstSerial);
		callerRecord->UNIXtime = key;
		return 1;
	}
	if(key >= _lastKey){														// Back to the future
		readSerial(callerRecord, _lastSerial);
		callerRecord->UNIXtime = key;
		if(key = _lastKey) return 0;
		return 1;
	}
	//Serial.printf("search %d, highKey %d\r\n", key, _firstKey);
	uint32_t lowKey = _firstKey;
	int32_t lowSerial = _firstSerial;
	uint32_t highKey = _lastKey;
	int32_t highSerial = _lastSerial;
	
	for(int i=0; i<_cacheSize; i++){
		uint32_t cacheKey = _cacheKey[i];
		if(cacheKey == key){												// Deja Vu
			_cacheWrap = (_cacheWrap + _cacheSize - 1) % _cacheSize;
			readSerial(callerRecord, _cacheSerial[i]);
			return 0;	
		}
		else if(cacheKey > lowKey && cacheKey < key) {
			lowKey = cacheKey;
			lowSerial = _cacheSerial[i];
		}
		else if(cacheKey < highKey && cacheKey > key){
			highKey = cacheKey;
			highSerial = _cacheSerial[i];
		}
	}
	if((highSerial - lowSerial) == 1) {						// Hole in file
		_cacheWrap = (_cacheWrap + _cacheSize - 1) % _cacheSize;
		readSerial(callerRecord, lowSerial);
		callerRecord->UNIXtime = key;
		return 0;	
	}
	searchKey(callerRecord, key, lowKey, lowSerial, highKey, highSerial);
	callerRecord->UNIXtime = key;
	return 0;
}

void IotaLog::searchKey(IotaLogRecord* callerRecord, const uint32_t key, const uint32_t lowKey, const int32_t lowSerial, const uint32_t highKey, const int32_t highSerial){

  int32_t floorSerial = max(lowSerial, highSerial - (int32_t)((highKey - key) / _interval));
	int32_t ceilingSerial = min(highSerial, lowSerial + (int32_t)((key - lowKey) / _interval));

	//Serial.printf("low %d(%d), high %d(%d), floor %d, Ceiling %d\r\n", lowKey, lowSerial, highKey, highSerial,floorSerial, ceilingSerial); 

  if(ceilingSerial < highSerial || floorSerial == ceilingSerial){
		readSerial(callerRecord, ceilingSerial);
		
		if(callerRecord->UNIXtime == key){
			return;
		}
		searchKey(callerRecord, key, lowKey, lowSerial, callerRecord->UNIXtime, callerRecord->serial);
		return;
  }
  if(floorSerial > lowSerial){
		readSerial(callerRecord, floorSerial);
		if(callerRecord->UNIXtime == key){
			return;
		}
		searchKey(callerRecord, key, callerRecord->UNIXtime, callerRecord->serial, highKey, highSerial);
		return;
  }
  if((highSerial - lowSerial) <= 1){		
		readSerial(callerRecord, lowSerial);
		return;
  }
  readSerial(callerRecord, (lowSerial + highSerial) / 2);
  _readKeyIO++;
  if(callerRecord->UNIXtime == key){
		return;
  }
  if(callerRecord->UNIXtime < key){
		searchKey(callerRecord, key, callerRecord->UNIXtime, callerRecord->serial, highKey, highSerial);
		return;
  }
  searchKey(callerRecord, key, lowKey, lowSerial, callerRecord->UNIXtime, callerRecord->serial);
  return;
}

int IotaLog::readNext(IotaLogRecord* callerRecord){
  if(!IotaFile) return 2;
  if(callerRecord->serial == _lastSerial) return 1;
  return readSerial(callerRecord, callerRecord->serial + 1);
}

int IotaLog::end(){
  IotaFile.close();
  return 0;
}

boolean IotaLog::isOpen(){
  if(IotaFile) return true;
  return false;
}

uint32_t IotaLog::firstKey(){return _firstKey;}
int32_t IotaLog::firstSerial(){return _firstSerial;}
uint32_t IotaLog::lastKey(){return _lastKey;}
int32_t IotaLog::lastSerial(){return _lastSerial;}
uint32_t IotaLog::fileSize(){return _fileSize;}
uint32_t IotaLog::readKeyIO(){return _readKeyIO;}
uint32_t IotaLog::interval(){return _interval;}

uint32_t IotaLog::setDays(uint32_t days){
	_maxFileSize = max(_fileSize, (uint32_t)(days * _recordSize * (86400UL / _interval)));
	_maxFileSize = max(_maxFileSize, (uint32_t)(_recordSize * (3600UL / _interval)));
	return _maxFileSize / (_recordSize * (86400 / _interval));
}
  
int IotaLog::readSerial(IotaLogRecord* callerRecord, int32_t serial){
  if(serial < _firstSerial || serial > _lastSerial){
		return 1;
  }
  IotaFile.seek(((serial - _firstSerial) * sizeof(IotaLogRecord) + _wrap) % _fileSize);
	IotaFile.read(callerRecord, sizeof(IotaLogRecord));
	_cacheKey[_cacheWrap] = callerRecord->UNIXtime;
	_cacheSerial[_cacheWrap++] = callerRecord->serial;
	_cacheWrap %= _cacheSize;
	_readKeyIO++;
  return 0;
};
   
int IotaLog::write (IotaLogRecord* callerRecord){

  if(!IotaFile){
		return 2;
  }
  if(callerRecord->UNIXtime <= _lastKey) {
		return 1;
  }
  callerRecord->serial = ++_lastSerial;
  if(_wrap || _fileSize >= _maxFileSize){
		IotaFile.seek(_wrap);
		_wrap = (_wrap + sizeof(IotaLogRecord)) % _fileSize;
  }
  else {
		IotaFile.seek(_fileSize);
		_fileSize += sizeof(IotaLogRecord);
		_entries++;
  }
  IotaFile.write((char*)callerRecord, sizeof(IotaLogRecord));
  IotaFile.flush();
  _lastKey = callerRecord->UNIXtime;
  _lastSerial = callerRecord->serial;
  if(_firstKey == 0){
		_firstKey = callerRecord->UNIXtime;
  }
  else if(_wrap || _fileSize == _maxFileSize){
		IotaFile.seek(_wrap);
		IotaFile.read((char*)callerRecord,8);
		_firstKey = callerRecord->UNIXtime;
		_firstSerial = callerRecord->serial;
		callerRecord->UNIXtime = _lastKey;
		callerRecord->serial = _lastSerial;
  }
  
  return 0;
}

void IotaLog::dumpFile(){
	setLedCycle(LED_DUMPING_LOG);
	char diagPath[] = "iotaWatt/logDiag.txt";
	SD.remove(diagPath);
	File logDiag = SD.open(diagPath, FILE_WRITE);
	if(logDiag){
		DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
    logDiag.printf_P(PSTR("%d/%02d/%02d %02d:%02d:%02d\r\nfilesize %d, entries %d\r\n"),
    now.month(), now.day(), now.year()%100, now.hour(), now.minute(), now.second(),
		IotaFile.size(), _entries);
		logDiag.close();
	}
	IotaFile.seek(0);
	IotaFile.read(&record,sizeof(record));
  uint32_t begKey = record.UNIXtime;
  uint32_t begSerial = record.serial;
	uint32_t endKey = record.UNIXtime;
	uint32_t endSerial = record.serial;
  uint32_t filePos = 0;
  do {
		filePos += _recordSize;
		IotaFile.seek(filePos);
		IotaFile.read(&record,sizeof(record));
		if(record.UNIXtime - endKey != _interval || record.serial - endSerial != 1 || filePos >= _fileSize){
			Serial.printf_P(PSTR("%d,%d,%d,%d\r\n"), begKey, begSerial, endKey, endSerial);
			logDiag = SD.open(diagPath, FILE_WRITE);
			if(logDiag){
				logDiag.printf_P(PSTR("%d,%d,%d,%d\r\n"), begKey, begSerial, endKey, endSerial);
				if(filePos >= IotaFile.size()){
					logDiag.printf_P(PSTR("End of file\r\n"));
				}
				logDiag.close();
			}
			begKey = record.UNIXtime;
			begSerial = record.serial;
		}
		endKey = record.UNIXtime;
		endSerial = record.serial;
	} while(filePos < IotaFile.size());
	endLedCycle();
}
