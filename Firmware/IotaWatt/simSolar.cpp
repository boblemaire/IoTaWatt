#include "IotaWatt.h"

/**************************************************************************************************
 * 
 *  simSolar - simulate a solar input
 * 
 *  Simulates a solar output using a sine function from 8am - 4pm.
 *  
 *  Functions are:
 * 
 *  double simSolarPower(uint32_t time)
 * 
 *  double simSolarEnergy(uint32_t begin, uint32_t end)
 * 
 *  input time is UTC but function will convert to local time.
 *  Multiply result by simulated system capacity in Watts.
 * 
 * ************************************************************************************************/

#define SIMSOLAR_SUNRISE 700
#define SIMSOLAR_SUNSET 1700

double localHour(uint32_t time){
    tm *_tm;
    time_t local = UTC2Local(time);
    _tm = gmtime(&local);
    return float(_tm->tm_hour) + float(_tm->tm_min * 60 + _tm->tm_sec) / 3600.0;
}

double HHMMtoHour(int HHMM){
    int HH = HHMM / 100;
    int MM = HHMM % 100;
    return float(HH) + float(MM) / 60.0;
}

double simSolarPower(uint32_t time){
    return sin(RANGE(PI * (localHour(time) - HHMMtoHour(SIMSOLAR_SUNRISE)) / (HHMMtoHour(SIMSOLAR_SUNSET) - HHMMtoHour(SIMSOLAR_SUNRISE)), 0, PI));
}

double simSolarEnergy(uint32_t beginTime, uint32_t endTime){
    if(beginTime >= endTime){
        return 0;
    }
    int days = (endTime - beginTime) / 86400;
    double beginRadians = RANGE(PI * (localHour(beginTime) - HHMMtoHour(SIMSOLAR_SUNRISE)) / (HHMMtoHour(SIMSOLAR_SUNSET) - HHMMtoHour(SIMSOLAR_SUNRISE)), 0, PI);
    double endRadians = RANGE(PI * (localHour(endTime) - HHMMtoHour(SIMSOLAR_SUNRISE)) / (HHMMtoHour(SIMSOLAR_SUNSET) - HHMMtoHour(SIMSOLAR_SUNRISE)), 0, PI);
    double energy = days * 2.0;
    if(beginRadians < endRadians){
        energy += cos(beginRadians) - cos(endRadians);
    }
    else if(beginRadians > endRadians){
        energy += 2 - (cos(endRadians) - cos(beginRadians));
    }
    return energy * (HHMMtoHour(SIMSOLAR_SUNSET) - HHMMtoHour(SIMSOLAR_SUNRISE)) / PI;
}