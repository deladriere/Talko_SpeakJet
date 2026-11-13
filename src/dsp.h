#pragma once
#include "panel_logic.h"
#include <stdint.h>

namespace Dsp {
constexpr unsigned Rate = 8026;
inline int16_t clip(int32_t x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : int16_t(x); }

class Effects {
  static constexpr unsigned Size=4096, Mask=Size-1;
  int16_t delay[Size]{};
  int16_t sine[256]{};
  unsigned head=0, valid=0, phase=0, tail=0;
  int32_t lp=0, hp=0, previous=0;
  Fx selected=Fx::Off;
  uint16_t wet=0, step=0, coefficient=0, delaySamples=0, configuredAdc=65535;
  int16_t tap(unsigned n) const { return n<=valid ? delay[(head-n)&Mask] : 0; }
  void push(int32_t x) { delay[head]=clip(x);head=(head+1)&Mask;if(valid<Size)++valid; }
public:
  void begin();
  void configure(Fx effect,uint16_t adc);
  int16_t process(int16_t x);
  bool ringing() const { return tail>0 && wet && (selected==Fx::Delay || selected==Fx::Reverb); }
};

class Freeze {
  static constexpr unsigned Size=1024,Mask=Size-1;
  int16_t history[Size]{};
  uint8_t poses[Size]{};
  unsigned head=0,count=0,base=0,length=0,phase=0;
  bool active=false;
public:
  void clear() { head=count=phase=0;active=false; }
  bool available() const { return count>=16; }
  void remember(int16_t x,uint8_t pose) { history[head]=x;poses[head]=pose;head=(head+1)&Mask;if(count<Size)++count; }
  void release() { active=false; }
  int16_t sample(uint16_t speed,uint16_t pitch,uint8_t &pose) {
    unsigned wanted=64+(uint32_t(4095-speed)*960)/4095;
    if(wanted>count)wanted=count;
    if(!active || wanted!=length) { length=wanted;base=(head-length)&Mask;phase=0;active=true; }
    if(!length) { pose=0;return 0; }
    const unsigned at=phase>>8,next=(at+1)%length,frac=phase&255;
    const int32_t a=history[(base+at)&Mask],b=history[(base+next)&Mask];
    pose=poses[(base+at)&Mask];
    phase+=pitch;
    while(phase>=length*256)phase-=length*256;
    return int16_t(a+((b-a)*int32_t(frac))/256);
  }
};
}
