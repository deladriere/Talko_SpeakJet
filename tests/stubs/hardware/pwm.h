#pragma once
#include <Arduino.h>
#include <hardware/gpio.h>
inline uint pwm_gpio_to_slice_num(uint p){return p/2;}
inline void pwm_set_wrap(uint,uint){}
inline void pwm_set_gpio_level(uint p,uint v){testPwm[p]=v;}
inline void pwm_set_enabled(uint,bool){}

struct pwm_config{};
inline uint pwm_gpio_to_channel(uint p){return p&1;}
inline pwm_config pwm_get_default_config(){return {};}
inline void pwm_config_set_wrap(pwm_config*,uint){}
inline void pwm_init(uint,const pwm_config*,bool){}
inline void pwm_set_chan_level(uint,uint,uint){}
inline void pwm_clear_irq(uint){}
inline void pwm_set_irq_enabled(uint,bool){}
