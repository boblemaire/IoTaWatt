#include "IotaWatt.h"
#include <simSolar.h>

double simSolar::power(time_t instant){
    tm* _tm = gmtime(&instant);
    float hour = float(_tm->tm_hour) + float(_tm->tm_min * 60 + _tm->tm_sec) / 3600.0;
    return _power * sin(RANGE(PI * (hour - _sunrise) / (_sunset - _sunrise), 0, PI));
}

double simSolar::energy(time_t begin, time_t end){
    if(begin >= end){
        return 0;
    }
    int days = (end - begin) / 86400;
    tm* _tm = gmtime(&begin);
    float beginTime = float(_tm->tm_hour) + float(_tm->tm_min * 60 + _tm->tm_sec) / 3600.0;
    _tm = gmtime(&end);
    float endTime = float(_tm->tm_hour) + float(_tm->tm_min * 60 + _tm->tm_sec) / 3600.0;
    double beginRadians = RANGE(PI * (beginTime - _sunrise) / (_sunset - _sunrise), 0, PI);
    double endRadians = RANGE(PI * (endTime - _sunrise) / (_sunset - _sunrise), 0, PI);
    double energy = days * 2.0;
    if(beginRadians < endRadians){
        energy += cos(beginRadians) - cos(endRadians);
    }
    else if(beginRadians > endRadians){
        energy += 2 - (cos(endRadians) - cos(beginRadians));
    }
    return _power * energy * (_sunset - _sunrise) / PI;
}

bool simSolar::config(int sunrise, int sunset, uint32_t power){
    int HH = sunrise / 100;
    int MM = sunrise % 100;
    _sunrise = float(HH) + float(MM) / 60.0;
    HH = sunset / 100;
    MM = sunset % 100;
    _sunset = float(HH) + float(MM) / 60.0;
    _power = power;
    return true;
}