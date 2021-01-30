#include "splitstr.h"


        splitstr::splitstr(const char* line, const char sepchar, const char endchar):
            _entries(0),
            _line(0),
            _entry(0)
        {
            char *end = strchr(line, endchar);
            if(end){
                _line = new char[end - line] + 1;
                memcpy(_line, line, end - line);
                _line[end - line] = 0;
                char *ptr = _line;
                _entries = 1;
                while(ptr = strchr(ptr, sepchar)){
                    *(ptr++) = 0;
                    _entries++;
                }
                _entry = new char *[_entries];
                ptr = _line - 1;
                for (int i = 0; i < _entries; i++){
                    while(*(++ptr) == ' ');
                    _entry[i] = ptr;
                    ptr += strlen(ptr);
                    char *trim = ptr;
                    while(*(--trim) == ' ');
                    *(trim + 1) = 0;
                }
            }
        }

        splitstr::~splitstr()
        {
            delete[] _line;
            delete[] _entry;
        }

        char* splitstr::operator[] (int ndx){
            return _entry[ndx];
        }

        int splitstr::length(){
            return _entries;
        }
