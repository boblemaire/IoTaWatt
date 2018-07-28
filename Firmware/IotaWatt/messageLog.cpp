#include "iotaWatt.h"

            messageLog::messageLog()
                :bufPos(0)
                ,bufLen(60)
                ,newMsg(true)
                ,restart(true)
                {}

void        messageLog::endMsg(){
                this->println();
                Serial.write(buf, bufPos);
                msgFile = SD.open(IotaMsgLog,FILE_WRITE);
                if(msgFile) {
                    msgFile.write(buf, bufPos);
                    msgFile.close();
                }
                delete[] buf;
                newMsg= true;
                return;
            }

size_t      messageLog::write(const uint8_t byte){
                if(newMsg){
                    newMsg = false;
                    buf = new uint8_t[bufLen];
                    bufPos = 0;
                    if(restart){
                        restart = false;
                        this->printf_P("\r\n** Restart **\r\n\n");
                    }
                    if(RTCrunning){
                        DateTime now = DateTime(UNIXtime() + (localTimeDiff * 3600));
                        this->printf_P(PSTR("%d/%02d/%02d %02d:%02d:%02d "),
                        now.month(), now.day(), now.year()%100, now.hour(), now.minute(), now.second());
                        if(localTimeDiff == 0){
                            buf[bufPos-1] = 'z';
                            buf[bufPos++] = ' ';
                        }
                    }
                }
                if(bufPos >= bufLen) {
                    Serial.write(buf, bufPos);
                    msgFile = SD.open(IotaMsgLog,FILE_WRITE);
                    if(! msgFile){
                        String msgDir = IotaMsgLog;
                        msgDir.remove(msgDir.indexOf('/',1));
                        SD.mkdir(msgDir.c_str());
                        msgFile = SD.open(IotaMsgLog,FILE_WRITE);
                    }
                    if(msgFile) {
                        msgFile.write(buf, bufPos);
                        msgFile.close();
                    }
                    bufPos = 0;
                }
                buf[bufPos++] = byte;
                return 1;
            }

size_t      messageLog::write(const uint8_t* buf, const size_t len){
                for(int i=0; i<len; i++) write(buf[i]);
                return len;
            }


  
  
