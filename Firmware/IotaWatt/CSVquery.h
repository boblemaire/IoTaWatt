#pragma once

#include "iotawatt.h"

class  CSVquery {

    public:
        CSVquery();
        ~CSVquery();
        bool    setup();
        size_t  readResult(uint8_t* buf, int len);
        bool    isJson();
        bool    isCSV();

    private:

        enum        tUnits {tUnitsSeconds,      // time units 
                            tUnitsMinutes,
                            tUnitsHours,
                            tUnitsDays,
                            tUnitsWeeks,
                            tUnitsMonths,
                            tUnitsYears};

        enum        format {formatJson,         // Output format
                            formatCSV}; 

        IotaLogRecord*  _oldRec;                // -> aged logRecord
        IotaLogRecord*  _newRec;                // -> new logRecord
        xbuf            _buffer;                // work buffer to build response lines

        uint32_t    _begin;                     // Beginning time - UTC
        uint32_t    _end;                       // Ending time - UTC
        tUnits      _groupUnits;                // Basic group time unit
        int         _groupMult;                 // Group unit muliplier as in 7d
        format      _format;                    // Output format (Json or CSV)
        tm*         _tm;                        // -> external tm struct
        bool        _setup;                     // True if successful setup
        bool        _header;                    // True if header == yes
        bool        _firstLine;                 // True when no lines have been generated
        bool        _lastLine;                  // True when last line has been generated
        bool        _missingSkip;               // Omit output line when no data
        bool        _missingNull;               // Produce null values when no data
        bool        _missingZero;               // Produce zero values when no data

        struct column {                         // Output column descriptor - built lifo then made fifo    
                    column* next;               // -> next in chain
                    char    source;             // Data source 'T'=time, 'I'=input, 'O'=output
                    char    unit;               // Unit 'V'=voltage, 'P'=power(Watts), 'E'=energy(kWh)
                    int8_t  decimals;           // Overide decimal positions    
                    bool    timeLocal;          // output local time if source=='T'
                    union{                      // Multi-purpose
                        Script*     script;     // -> Script if source=='O'
                        int32_t     input;      // input number if source=='I'
                        };
                    column()
                        :next(nullptr)
                        ,source(' ')
                        ,unit(' ')
                        ,timeLocal(true)
                        ,decimals(1)
                        ,input(0)
                        {}
                    ~column(){delete next;}
                    };

        column*     _columns;                   // List head

                // Private functions

        void        buildHeader();
        void        buildLine();
        time_t      nextGroup(time_t time, tUnits units, int32_t mult);
        time_t      parseTimeArg(String timeArg);
        int         parseInt(char** ptr);

};