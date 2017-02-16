/*****************************************************************************************
 * 
 * Basic program event loging.  Goes to Serial if connected. Will be putting the entries
 * on the SDcard and providing an API to read so the log can be viewed in the status
 * utility.
 * 
 * There's a better way to accomodate the various types, but this works for now.
 ****************************************************************************************/

void msgLog(String message){msgLog((char*)message.c_str(), "", "");}
void msgLog(char* segment1, String segment2){msgLog(segment1,(char*)segment2.c_str());}
void msgLog(char* segment1, uint32_t segment2){msgLog(segment1, String(segment2));}
void msgLog(char* segment1){msgLog(segment1, "", "");}
void msgLog(char* segment1, char* segment2){msgLog(segment1, segment2, "");}
void msgLog(char* segment1, char* segment2, char* segment3){
  static File msgFile;
  String msg = "";
  uint32_t _NTPtime = NTPtime();
  DateTime now = DateTime(UnixTime() + (localTimeDiff * 3600));
  
  if(_NTPtime != 0){
    msg = String(now.month()) + '/' + String(now.day()) + '/' + String(now.year()%100) + ' ' + 
          String(now.hour()) + ':' + String(now.minute()) + ':' + String(now.second()) + ' ';
  } else {
    msg += "No clock yet: ";
  }
  msg += String(segment1) + String(segment2) + String(segment3);
  Serial.println(msg);

  if(!msgFile){
    msgFile = SD.open(IotaMsgLog,FILE_WRITE);
  }
  if(msgFile){
    msg += "\r\n";
    msgFile.write((char*)msg.c_str(), msg.length());
    msgFile.flush();
  } 
}

String formatIP(uint32_t IP){
  String _ip = String(IP & 0xFF) + ".";
  _ip += String((IP >> 8) & 0xFF) + ".";
  _ip += String((IP >> 16) & 0xFF) + ".";
  _ip += String((IP >> 24) & 0xFF);
  return _ip;
}

