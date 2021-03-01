/*
  IotaLog - Library for IoTaLog energy monitor
  Created by Bob Lemaire
*/
#include "IotaWatt.h"

struct {
  uint32_t UNIXtime;
  uint32_t serial; 
} recordKey;

int IotaLog::begin (const char* path ){
	if(IotaFile) return 0;
	_path = charstar(path);
	if(!SD.exists(_path)){
		String logPath = _path;
		if(logPath.lastIndexOf('/') > 0){
			String  dir = logPath.substring(0,logPath.lastIndexOf('/'));
			if( ! SD.exists(dir)){
				if( ! SD.mkdir(dir)){
					Serial.printf_P(PSTR("mkdir failed: %s\r\n"), dir.c_str());
					return 2;
				}
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
		IotaFile.read((uint8_t*)&recordKey, sizeof(recordKey));
		_firstKey = recordKey.UNIXtime;
		_firstSerial = recordKey.serial;
		IotaFile.seek(_fileSize - _recordSize);
		IotaFile.read((uint8_t*)&recordKey, sizeof(recordKey));
		_lastKey = recordKey.UNIXtime;
		_lastSerial = recordKey.serial;
		_entries = _fileSize / _recordSize;
  }

					// If there are trailing zero recordKeys at the end,
					// try to adjust _filesize down to match logical end of file.

	while(_fileSize && _lastSerial == 0){
		IotaLogRecord* logRec = new IotaLogRecord;
		_fileSize -= _recordSize;
		IotaFile.seek(_fileSize - _recordSize);
		IotaFile.read((uint8_t*)logRec, _recordSize);
		_lastSerial = logRec->serial;
		_lastKey = logRec->UNIXtime;
		_entries--;	 
		delete logRec;
	}
	if(_fileSize != IotaFile.size()){
		Serial.printf("physical %d, logical %d\r\n", IotaFile.size(), _fileSize);
	}
	
  if(_firstKey > _lastKey){
		_wrap = findWrap(0,_firstKey, _fileSize - _recordSize, _lastKey);
		IotaFile.seek(_wrap);
		IotaFile.read((uint8_t*)&recordKey, sizeof(recordKey));
		_firstKey = recordKey.UNIXtime;
		_firstSerial = recordKey.serial;
		IotaFile.seek(_wrap - _recordSize);
		IotaFile.read((uint8_t*)&recordKey, sizeof(recordKey));
		_lastKey = recordKey.UNIXtime;
		_lastSerial = recordKey.serial;
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
  } recordKey;
  
  if((lowPos - highPos) == _recordSize) {
		return lowPos;
  }
  uint32_t midPos = (highPos + lowPos) / 2;
  midPos += midPos % _recordSize;
  IotaFile.seek(midPos);
  IotaFile.read((uint8_t*)&recordKey, sizeof(recordKey));
  uint32_t midKey = recordKey.UNIXtime;
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
	int pos = ((serial - _firstSerial) * _recordSize + _wrap) % _fileSize;
	if(_writeCache && pos >= _writeCachePos){
		memcpy(callerRecord, _writeCacheBuf + (pos - _writeCachePos), _recordSize);
		return 0;
	}
	if(pos != IotaFile.position()){
		IotaFile.seek(pos);
	}
	IotaFile.read((uint8_t*)callerRecord, _recordSize);
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
	if(_writeCache){
		memcpy(_writeCacheBuf + (_fileSize % IOTALOG_BLOCK_SIZE), callerRecord, _recordSize);
		_fileSize += _recordSize;
		if(_fileSize % IOTALOG_BLOCK_SIZE == 0){
			IotaFile.seek(_writeCachePos);
			IotaFile.write(_writeCacheBuf, IOTALOG_BLOCK_SIZE);
			IotaFile.flush();
			_writeCachePos += IOTALOG_BLOCK_SIZE;
		}
		if(_fileSize >= _maxFileSize){
			writeCache(false);
		}
	}
	else {
		if(_wrap || _fileSize >= _maxFileSize){
				IotaFile.seek(_wrap);
				_wrap = (_wrap + _recordSize) % _fileSize;
		}
		else {
				IotaFile.seek(_fileSize);
				_fileSize += _recordSize;
				_entries++;
		}
		IotaFile.write((char*)callerRecord, _recordSize);
		IotaFile.flush();
	}
	_lastKey = callerRecord->UNIXtime;
	_lastSerial = callerRecord->serial;
	if(_firstKey == 0){
			_firstKey = callerRecord->UNIXtime;
	}
	else if(_wrap || _fileSize == _maxFileSize){
			IotaFile.seek(_wrap);
			IotaFile.read((uint8_t*)callerRecord,8);
			_firstKey = callerRecord->UNIXtime;
			_firstSerial = callerRecord->serial;
			callerRecord->UNIXtime = _lastKey;
			callerRecord->serial = _lastSerial;
	}
	
	return 0;
}

void IotaLog::writeCache(bool on){
	if(on && !_writeCache && !_wrap){ // turn cache on
		_writeCacheBuf = new uint8_t[IOTALOG_BLOCK_SIZE];
		_writeCachePos = _fileSize - (_fileSize % IOTALOG_BLOCK_SIZE);
		if(_writeCachePos < _fileSize){
			IotaFile.seek(_writeCachePos);
			IotaFile.read(_writeCacheBuf, IOTALOG_BLOCK_SIZE);
		}
		_writeCache = true;
	}
	else if(!on and _writeCache){ // turn cache off
		if(_fileSize % IOTALOG_BLOCK_SIZE){
			IotaFile.seek(_writeCachePos);
			IotaFile.write(_writeCacheBuf, _fileSize % IOTALOG_BLOCK_SIZE);
			IotaFile.flush();
		}
		delete[] _writeCacheBuf;
		_writeCacheBuf = nullptr;
		_writeCache = false;
	}
}

void IotaLog::dumpFile(){
	setLedCycle(LED_DUMPING_LOG);
	char diagPath[] = "iotaWatt/logDiag.txt";
	SD.remove(diagPath);
	File logDiag = SD.open(diagPath, FILE_WRITE);
	if(logDiag){
		DateTime now = DateTime(localTime());
    logDiag.printf_P(PSTR("%d/%02d/%02d %02d:%02d:%02d\r\nfilesize %d, entries %d\r\n"),
    now.month(), now.day(), now.year()%100, now.hour(), now.minute(), now.second(),
		IotaFile.size(), _entries);
		logDiag.close();
	}
	IotaFile.seek(0);
	IotaFile.read((uint8_t*)&recordKey,sizeof(recordKey));
  uint32_t begKey = recordKey.UNIXtime;
  uint32_t begSerial = recordKey.serial;
	uint32_t endKey = recordKey.UNIXtime;
	uint32_t endSerial = recordKey.serial;
  uint32_t filePos = 0;
  do {
		filePos += _recordSize;
		IotaFile.seek(filePos);
		IotaFile.read((uint8_t*)&recordKey,sizeof(recordKey));
		if(recordKey.UNIXtime - endKey != _interval || recordKey.serial - endSerial != 1 || filePos >= _fileSize){
			Serial.printf_P(PSTR("%d,%d,%d,%d\r\n"), begKey, begSerial, endKey, endSerial);
			logDiag = SD.open(diagPath, FILE_WRITE);
			if(logDiag){
				logDiag.printf_P(PSTR("%d,%d,%d,%d\r\n"), begKey, begSerial, endKey, endSerial);
				if(filePos >= IotaFile.size()){
					logDiag.printf_P(PSTR("End of file\r\n"));
				}
				logDiag.close();
			}
			begKey = recordKey.UNIXtime;
			begSerial = recordKey.serial;
		}
		endKey = recordKey.UNIXtime;
		endSerial = recordKey.serial;
	} while(filePos < IotaFile.size());
	endLedCycle();
}
