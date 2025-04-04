#pragma once

#define PCF8523_CTL_1           0       // Control Register 1 address
#define PCF8523_CTL_1_RESET     0x58    // Control Register 1 reset
#define PCF8523_CTL_1_CAP_SEL   0x80    // Capacitor select 0=7pF, 1=12.5pF

#define PCF8523_CTL_2           1       // Control Register 2 address
#define PCF8523_CTL_2_WTAF      0x80    // WDT A interrupt generated
#define PCF8523_CTL_2_WTAIE     0x04    // WDT A interrupt enable

#define PCF8523_CTL_3           2       // Control Register 3 address
#define PCF8523_CTL_3_PM        0xe0    // Battery Switch over and low detection ctl
#define PCF8523_CTL_3_BSF       0x08    // Battery switchover occurred (power fail)
#define PCF8523_CTL_3_BLF       0x04    // Battery Status (1 = low)
#define PCF8523_CTL_3_BSIE      0x02    // Interrupt on battery switchover = 1
#define PCF8523_CTL_3_BLIE      0x01    // Interrupt on battery low = 1

#define PCF8523_Tmr_CLKOUT_ctrl         0x0f    // Control Timers
#define PCF8523_Tmr_CLKOUT_ctrl_TAM     0x80    // Timer A (pulsed=1)
#define PCF8523_Tmr_CLKOUT_ctrl_TBM     0x40    // Timer B (pulsed=1)
#define PCF8523_Tmr_CLKOUT_ctrl_NOCLK   0x38    // No CLKOUT
#define PCF8523_Tmr_CLKOUT_ctrl_TAC     0x06    // Timer A control
#define PCF8523_Tmr_CLKOUT_ctrl_TAC_NO  0x00    // Timer A disabled
#define PCF8523_Tmr_CLKOUT_ctrl_TAC_CD  0x02    // Timer A countdown
#define PCF8523_Tmr_CLKOUT_ctrl_TAC_WD  0x04    // Timer A watchdog

#define PCF8523_Tmr_A_freq_ctrl         0x10    // Timer A frequency control
#define PCF8523_Tmr_A_freq_ctrl_1Hz     0x02    // Timer A 1 Hz

#define PCF8523_Tmr_A_reg               0x11    // Timer A register


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
uint32_t  UTCtime(uint32_t _localtime);
uint32_t  localTime();
uint32_t  localTime(uint32_t _utctime);
uint32_t  millisAtUTCTime(uint32_t);
void      dateTime(uint16_t* date, uint16_t* time);
uint32_t  littleEndian(uint32_t);
uint32_t  UTC2Local(uint32_t UTCtime);
uint32_t  local2UTC(uint32_t localTime);
bool      testRule(uint32_t standardTime, dateTimeRule);

