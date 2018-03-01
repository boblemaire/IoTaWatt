#pragma once
/***********************************************************************************
    Copyright (C) <2018>  <Bob Lemaire, IoTaWatt, Inc.>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.  
   
***********************************************************************************/
#include <Arduino.h>
#define SEGMENT_SIZE 64

struct xseg {
    xseg    *next;
    uint8_t data[SEGMENT_SIZE];
    xseg():next(nullptr){}
    ~xseg(){}
};

class xbuf {
    public:

        xbuf();
        ~xbuf();

        size_t      write(const uint8_t);
        size_t      write(const char*);
        size_t      write(const uint8_t*, size_t);
        size_t      write(String);
        uint8_t     read();
        size_t      read(uint8_t*, size_t);
        size_t      available();
        int         indexOf(const char, const size_t begin=0);
        int         indexOf(const char*, const size_t begin=0);
        String      readStringUntil(const char);
        String      readStringUntil(const char*);
        String      readString(int);
        void        flush();

    protected:

        xseg        *_head;
        xseg        *_tail;
        size_t      _offset;
        size_t      _used;
        size_t      _free;

        void        addSeg();
        void        remSeg();

};