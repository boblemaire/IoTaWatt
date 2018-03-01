#include "IotaWatt.h"

String timeString(int value);
String formatIP(uint32_t IP);

/*****************************************************************************************
 * 
 * Basic program event loging.  Goes to Serial if connected. Will be putting the entries
 * on the SDcard and providing an API to read so the log can be viewed in the status
 * utility.
 * 
 * There's a better way to accomodate the various types, but this works for now.
 ****************************************************************************************/

void msgLog(String message){msgLog((const char*)message.c_str(), "", "");}
void msgLog(const char* segment1, String segment2){msgLog(segment1,(const char*)segment2.c_str());}
void msgLog(const char* segment1, int32_t segment2){msgLog(segment1, String(segment2));}
void msgLog(const char* segment1){msgLog(segment1, "", "");}
void msgLog(const char* segment1,const  char* segment2){msgLog(segment1, segment2, "");}
void msgLog(const char* segment1,const  char* segment2,const  char* segment3){
  static File msgFile;
  static boolean restart = true;
  String msg = "";
  uint32_t _NTPtime = NTPtime();
  DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
  
  if(RTCrunning){
    msg = String(now.month()) + '/' + String(now.day()) + '/' + String(now.year()%100) + ' ' + 
          timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second()) + ' ';
  } else {
    msg += "No clock yet: ";
  }
  msg += String(segment1) + String(segment2) + String(segment3);
  Serial.println(msg);

  if(!msgFile){
    msgFile = SD.open(IotaMsgLog,FILE_WRITE);
    if(restart && msgFile.size() > 1000000L){
      msgFile.close();
      SD.remove(IotaMsgLog);
      msgFile = SD.open(IotaMsgLog,FILE_WRITE);
    }
  }
  if(msgFile){
    if(restart){
      restart = false;
      msgFile.write("\r\n** Restart **\r\n\n");
    }
    msg += "\r\n";
    msgFile.write((char*)msg.c_str(), msg.length());
    msgFile.close(); 
  }
}

String timeString(int value){
  if(value < 10) return String("0") + String(value);
  return String(value);
}

String formatIP(uint32_t IP){
  String _ip = String(IP & 0xFF) + ".";
  _ip += String((IP >> 8) & 0xFF) + ".";
  _ip += String((IP >> 16) & 0xFF) + ".";
  _ip += String((IP >> 24) & 0xFF);
  return _ip;
}

