#pragma once
#include <Arduino.h>
inline uint pwm_gpio_to_slice_num(uint p){return p/2;}
inline void pwm_set_wrap(uint,uint){}
inline void pwm_set_gpio_level(uint p,uint v){testPwm[p]=v;}
inline void pwm_set_enabled(uint,bool){}
