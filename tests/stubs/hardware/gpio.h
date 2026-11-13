#pragma once
#include <Arduino.h>
constexpr int GPIO_FUNC_PWM=1;
inline bool gpio_get(uint p){return testPins[p];}
inline uint32_t gpio_get_all(){uint32_t v=0;for(uint i=0;i<32;++i)if(testPins[i])v|=1u<<i;return v;}
inline void gpio_set_function(uint,int){}
