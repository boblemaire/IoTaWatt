#pragma once

#include "IotaWatt.h"

class  CSVquery {

    public:
        CSVquery();
        ~CSVquery();
        bool    setup();
        size_t  readResult(uint8_t* buf, int len);
        bool    isJson();
        bool    isCSV();
        String  failReason();

    private:

        enum        query  {none,               // No valid query setup
                            show,               // Query is show series
                            select};            // Query is select series

        // enum        units   {Volts = 1,         // Measurement units
        //                      Watts = 0,         // Values must match values in IotaScript.h (Will combine someday);
        //                      Wh = 8,
        //                      kWh = 6,
        //                      Amps = 2,
        //                      VA = 3,
        //                      Hz = 4,
        //                      PF = 7};

        enum        tUnits {tUnitsSeconds,      // time units 
                            tUnitsMinutes,
                            tUnitsHours,
                            tUnitsDays,
                            tUnitsWeeks,
                            tUnitsMonths,
                            tUnitsYears};

        enum        format {formatJson,         // Output format
                            formatCSV}; 

        enum        tformat {iso,
                             unix};

        IotaLogRecord*  _oldRec;                // -> aged logRecord
        IotaLogRecord*  _newRec;                // -> new logRecord
        xbuf            _buffer;                // work buffer to build response lines
        String          _failReason;            // Error message from constructor

        uint32_t    _begin;                     // Beginning time - UTC
        uint32_t    _end;                       // Ending time - UTC
        int32_t     _limit;                     // Output limit in lines, -1 is nolimit  
        uint32_t    _groupMult;                 // Group unit muliplier as in 7d
        tUnits      _groupUnits;                // Basic group time unit
        tformat     _timeFormat;                // Time output format
        format      _format;                    // Output format (Json or CSV)
        tm*         _tm;                        // -> external tm struct
        query       _query;                     // Type of query 
        bool        _header;                    // True if header = yes
        bool        _highRes;
        bool        _integrations;              // Period and Group should work with integrations
        bool        _firstLine;                 // True when no lines have been generated
        bool        _lastLine;                  // True when last line has been generated
        bool        _missingSkip;               // Omit output line when no data
        bool        _missingNull;               // Produce null values when no data
        bool        _missingZero;               // Produce zero values when no data
        bool        _timeOnly;                  // Query is for time only, no data needed    

        struct column {                         // Output column descriptor - built lifo then made fifo    
                    column* next;               // -> next in chain
                    double  lastValue;          // Used for delta function.
                    char    source;             // Data source 'T'=time, 'I'=input, 'O'=output
                    union   {
                        units   unit;           // Units to produce
                        tformat timeFormat;     // Format of Time column
                    };
                    int8_t  decimals;           // Overide decimal positions    
                    bool    timeLocal;          // output local time if source=='T'
                    bool    delta;              // Output change in value;
                    Script* script;             // -> Script
                    int32_t input;              // input number if source=='I'
                    column()
                        :next(nullptr)
                        ,lastValue(0)
                        ,source(' ')
                        ,timeLocal(true)
                        ,delta(false)
                        ,script(nullptr)
                        ,decimals(1)
                        ,input(0)
                        {}
                    ~column(){
                        if(source == 'I'){
                            delete script;
                        }
                        delete next;}
                    };

        column*     _columns;                   // List head

                // Private functions

        void        buildHeader();
        void        buildLine();
        void        printValue(const double value, const int8_t decimals);
        time_t      nextGroup(time_t time, tUnits units, int32_t mult);
        time_t      parseTimeArg(String timeArg);
        int         parseInt(char** ptr);
        const char* unitstr(units);

};
