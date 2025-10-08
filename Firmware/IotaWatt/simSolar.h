#pragma once

/**************************************************************************************************
 * 
 *  simSolar - simulate a solar input
 * 
 *  Simulates a solar output using a sine function from parameters sunrise, sunset and power
 *  
 *  Functions are:
 * 
 *  double power(uint32_t time)
 * 
 *  double energy(uint32_t begin, uint32_t end)
 *  
 * ************************************************************************************************/

#include <Arduino.h>

class simSolar {

    public:
        simSolar(): _sunrise(0700), _sunset(1700), _power(1000){};
        double power(time_t time);
        double energy(time_t begin, time_t end);
        bool config(int sunrise, int sunset, uint32_t power);

    protected:
        float _sunrise;
        float _sunset;
        float _power;
};