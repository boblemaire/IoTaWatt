/*
  IotaLog - Library for IoTaLog energy monitor
  Created by Bob Lemaire
*/
#include "IotaLog.h"
  struct {
    uint32_t UNIXtime;
    uint32_t serial; 
  } record;
  int IotaLog::begin (const char* path, uint32_t maxDays){
    String logPath = String(path) + ".log";
    if(!SD.exists((char*)logPath.c_str())){
      if(logPath.lastIndexOf('/') > 0){
        String  dir = logPath.substring(0,logPath.lastIndexOf('/'));
        if( ! SD.mkdir((char*)dir.c_str())){
          Serial.print("mkdir failed: ");
          Serial.println(dir);
          return 2;
        }
      }
      IotaFile = SD.open((char*)logPath.c_str(), FILE_WRITE);
      if(!IotaFile){
        return 2;
      }
      IotaFile.close();
    }
    IotaFile = SD.open((char*)logPath.c_str(), FILE_WRITE);
    if(!IotaFile){
      return 2;
    }
    
    _fileSize = IotaFile.size();

    if(!_fileSize){
      _firstKey = 0;
      _firstSerial = 0;
      _lastKey = 0;
      _lastSerial = -1;
      _entries = 0;
    }
    else {
      IotaFile.seek(0);
      IotaFile.read(&record, sizeof(record));
      _firstKey = record.UNIXtime;
      _firstSerial = record.serial;
      uint32_t lastRec = _fileSize - sizeof(IotaLogRecord);
      IotaFile.seek(lastRec);
      IotaFile.read(&record, sizeof(record));
      _lastKey = record.UNIXtime;
      _lastSerial = record.serial;
      _entries = _fileSize / sizeof(IotaLogRecord);
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
    _maxFileSize = max(_fileSize, maxDays * (_recordSize * 24 * 3600 / _interval));
    
    if(((int32_t) _lastSerial - _firstSerial + 1) * _recordSize != _fileSize){
      Serial.println("Datalog inconsistent: size:");
      Serial.print("filesize:");
      Serial.println(_fileSize);
      dumpFile();
      IotaFile.close();
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
    if(_entries == 0 || key < _firstKey || key > _lastKey){
      return 1;
    }
    if(key < _lastReadKey){
      searchKey(callerRecord, key, _firstKey, _firstSerial, _lastReadKey, _lastReadSerial);
    }
    else {
      searchKey(callerRecord, key, _lastReadKey, _lastReadSerial, _lastKey, _lastSerial);
    }
    _lastReadKey = callerRecord->UNIXtime;
    _lastReadSerial = callerRecord->serial;
    return 0;
  }

  void IotaLog::searchKey(IotaLogRecord* callerRecord, const uint32_t key, const uint32_t lowKey, const uint32_t lowSerial, const uint32_t highKey, const uint32_t highSerial){

    uint32_t floorSerial = max(lowSerial, highSerial - (highKey - key) / _interval);
    uint32_t ceilingSerial = min(highSerial, lowSerial + (key - lowKey) / _interval);

    if(ceilingSerial < highSerial || floorSerial == ceilingSerial){
      readSerial(callerRecord, ceilingSerial);
      _readKeyIO++;
      if(callerRecord->UNIXtime == key){
        return;
      }
      searchKey(callerRecord, key, lowKey, lowSerial, callerRecord->UNIXtime, callerRecord->serial);
      return;
    }
    if(floorSerial > lowSerial){
      readSerial(callerRecord, floorSerial);
      _readKeyIO++;
      if(callerRecord->UNIXtime == key){
        return;
      }
      searchKey(callerRecord, key, callerRecord->UNIXtime, callerRecord->serial, highKey, highSerial);
      return;
    }
    if((highSerial - lowSerial) <= 1){
      readSerial(callerRecord, lowSerial);
      _readKeyIO++;
      callerRecord->UNIXtime = key;
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
  uint32_t IotaLog::firstSerial(){return _firstSerial;}
  uint32_t IotaLog::lastKey(){return _lastKey;}
  uint32_t IotaLog::lastSerial(){return _lastSerial;}
  uint32_t IotaLog::fileSize(){return _fileSize;}
  uint32_t IotaLog::readKeyIO(){return _readKeyIO;}
    
  int IotaLog::readSerial(IotaLogRecord* callerRecord, uint32_t serial){
    if(serial < _firstSerial || serial > _lastSerial){
      return 1;
    }
    IotaFile.seek(((serial - _firstSerial) * sizeof(IotaLogRecord) + _wrap) % _fileSize);
    IotaFile.read(callerRecord, sizeof(IotaLogRecord));
    return 0;
  };
     
  int IotaLog::write (IotaLogRecord* callerRecord){
    uint32_t oldWrap = _wrap;
    if(!IotaFile){
      return 2;
    }
    if(callerRecord->UNIXtime <= _lastKey) {
      return 1;
    }
    callerRecord->serial = ++_lastSerial;
    if(_wrap || _fileSize >= _maxFileSize){
      //Serial.print("seeking: "); Serial.println(_wrap);
      IotaFile.seek(_wrap);
      _wrap = (_wrap + sizeof(IotaLogRecord)) % _fileSize;
    }
    else {
      IotaFile.seek(_fileSize);
      _fileSize += sizeof(IotaLogRecord);
      _entries++;
    }
    //Serial.println("Writing");
    IotaFile.write((char*)callerRecord, sizeof(IotaLogRecord));
    IotaFile.flush();
    _lastKey = callerRecord->UNIXtime;
    _lastSerial = callerRecord->serial;
    if(_firstKey == 0){
      _firstKey = callerRecord->UNIXtime;
    }
    else if(_wrap || _fileSize == _maxFileSize){
      //Serial.print("seeking: "); Serial.println(_wrap);
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
    uint32_t filePos = 0;
    uint32_t key = 0;
    uint32_t serial = 0;
    while(filePos < _fileSize){
      IotaFile.seek(filePos);
      IotaFile.read(&record,sizeof(record));
      if(record.UNIXtime - key != _interval){
        if(key){
          Serial.print(key);
          Serial.print("(");
          Serial.print(serial);
          Serial.println(")");
        }
        
        Serial.print(record.UNIXtime);
        Serial.print("(");
        Serial.print(record.serial);
        Serial.print(")-");
      }
      key = record.UNIXtime;
      serial = record.serial;
      filePos += _recordSize;
    }
    Serial.print(key);
    Serial.print("(");
    Serial.print(serial);
    Serial.println(")");
  }
    

