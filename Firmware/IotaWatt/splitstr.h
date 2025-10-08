#ifndef SPLITSTR_H
#define SPLITSTR_H

#include "Arduino.h"

class splitstr {
    public:
        splitstr(const char *line, const char sepchar = ',' , const char endchar = 0);
        ~splitstr();
        int length();
        char *operator[] (int ndx);

    private:
        int _entries;
        char *_line;
        char **_entry;
};

#endif