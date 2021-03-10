#include "splitstr.h"

// Creates a copy of input line delimited by endchar,
// parses on sepchar,
// trims spaces from both ends of the elements
// resulting instance can be indexed with [] operator.
//
// splitstr line("first element,  abc  , third element", ',', 0);
// Serial.println(line.length());
//
// splitstr CSVline("first element,  abc  , third element    ");

// for (int i = 0; i < CSVline.length(); i++){
//  Serial.printf("line %d, length %d, %s\n", i, strlen(CSVline[i]), CSVline[i]);
// }
//
// line 0, length 13, first element
// line 1, length 3, abc
// line 2, length 13, third element
//

splitstr::splitstr(const char *line, const char sepchar, const char endchar):
        _entries(0),
        _line(0),
        _entry(0)
{
    char *end = strchr(line, endchar);
    if (end)
    {
        _line = new char[end - line] + 1;
        memcpy(_line, line, end - line);
        _line[end - line] = 0;
        char *ptr = _line;
        _entries = 1;
        while (ptr = strchr(ptr, sepchar))
        {
            *(ptr++) = 0;
            _entries++;
        }
        _entry = new char *[_entries];
        ptr = _line - 1;
        for (int i = 0; i < _entries; i++)
        {
            while (*(++ptr) == ' ');
            _entry[i] = ptr;
            ptr += strlen(ptr);
            char *trim = ptr;
            while (*(--trim) == ' ')
                ;
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
