#pragma once

struct dateTimeRule {           // Defines a starting date and time in any year.   
    int8_t    month;            // Month period begins(1-12)
    int8_t    weekday;          // Weekday period begins (sun-sat = 1-7)
    int8_t    instance;         // Instance in month (1=1st, 2=2nd, etc : -1=last, -2=2nd last, etc)
    int16_t   time;             // Time (in minutes) period begins
    dateTimeRule()              // Ex: second Sunday in March at 02:00:
        :month(1)               // month = 3, weekday = 1, instance = 2, time = 120
        ,weekday(1)
        ,instance(1)
        ,time(0)
        {};
};

struct tzRule {                 // Defines a daylight saving period and adjustment
    dateTimeRule begPeriod;     // Beginning of period
    dateTimeRule endPeriod;     // End of period
    bool         useUTC;        // Use UTC to evaluate rule  
    int16_t      adjMinutes;    // Minutes to adjust.
    tzRule():useUTC(false), adjMinutes(0){};
};  

uint32_t  NTPtime();
uint32_t  UTCtime();
uint32_t  localTime();
uint32_t  millisAtUTCTime(uint32_t);
void      dateTime(uint16_t* date, uint16_t* time);
uint32_t  littleEndian(uint32_t);
uint32_t  localTime();
uint32_t  UTC2Local(uint32_t UTCtime);
uint32_t  local2UTC(uint32_t localTime);
bool      testRule(uint32_t standardTime, dateTimeRule);

