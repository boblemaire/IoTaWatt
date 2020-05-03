#pragma once
#include <Arduino.h>

class messageLog: public Print {

    public:

        messageLog();

        size_t      write(const uint8_t);
        size_t      write(const uint8_t*, const size_t);
        void        endMsg();

    protected:

        File        msgFile;
        bool        newMsg;
        bool        restart;
        uint8_t*    buf;
        uint8_t     bufLen;
        uint8_t     bufPos;
};

#define log(format,...)  Message_log.printf_P(PSTR(format),##__VA_ARGS__); Message_log.endMsg()
