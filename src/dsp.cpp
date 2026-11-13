#include "dsp.h"
#include <math.h>

namespace Dsp {
void Effects::begin() {
  for(unsigned i=0;i<256;++i)sine[i]=int16_t(sinf(6.28318530718f*i/256.0f)*32767.0f);
}
void Effects::configure(Fx effect,uint16_t adc) {
  if(effect==selected && adc==configuredAdc)return;
  configuredAdc=adc;
  const uint16_t amount=Panel::amount(adc);
  if(effect!=selected || !amount) { valid=0;tail=0;lp=hp=previous=0; }
  selected=effect;wet=amount;
  step=uint16_t((uint32_t(30+(uint32_t(amount)*1370)/32767)*65536)/Rate);
  delaySamples=unsigned(Rate)*(40+(uint32_t(amount)*400)/32767)/1000;
  const float cutoff=effect==Fx::Highpass ? 80.0f+1920.0f*amount/32767.0f : 3200.0f-3020.0f*amount/32767.0f;
  const float pole=6.28318530718f*cutoff;
  coefficient=uint16_t(32767.0f*(effect==Fx::Highpass ? Rate/(Rate+pole) : pole/(Rate+pole)));
}
int16_t Effects::process(int16_t x) {
  if(!wet || selected==Fx::Off) { tail=0;return x; }
  int32_t y=x;
  switch(selected) {
  case Fx::Ring:
    y=(int32_t(x)*sine[phase>>8])/32768;phase=(phase+step)&65535;break;
  case Fx::Delay: {
    y=tap(delaySamples);push(int32_t(x)+(y*17000)/32768);
    if(x>16 || x<-16)tail=Rate*2;else if(tail)--tail;
    break;
  }
  case Fx::Reverb: {
    y=(int32_t(tap(Rate*80/1000))*16000+int32_t(tap(Rate*160/1000))*10000+int32_t(tap(Rate*320/1000))*6000)/32768;
    push(int32_t(x)+(y*19000)/32768);
    if(x>16 || x<-16)tail=Rate*2;else if(tail)--tail;
    break;
  }
  case Fx::Crush: {
    unsigned bits=1+(uint32_t(wet)*10)/32767;
    y=(int32_t(x)/(1<<bits))*(1<<bits);break;
  }
  case Fx::Drive:
    y=clip((int32_t(x)*(256+(uint32_t(wet)*1792)/32767))/256);break;
  case Fx::Lowpass:
    lp+=((int32_t(x)-lp)*coefficient)/32768;y=lp;break;
  case Fx::Highpass:
    hp=int32_t(((int64_t(hp)+x-previous)*coefficient)/32768);previous=x;y=hp;break;
  default:break;
  }
  return clip((int32_t(x)*(32767-wet)+y*wet)/32767);
}
}
