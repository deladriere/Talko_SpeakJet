#include "../src/dsp.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <initializer_list>
int main(){
  for(unsigned fx=0;fx<unsigned(Fx::Count);++fx){
    Dsp::Effects e;e.begin();e.configure(Fx(fx),4095);
    for(int n=0;n<24000;++n)e.process(n<4000?int16_t(std::sin(n*.08)*14000):0);
    e.configure(Fx(fx),0);
    for(int x:{-32768,-1000,0,1000,32767})assert(e.process(x)==x);
    assert(!e.ringing());
    e.configure(Fx::Delay,4095);e.process(24000);
    for(int n=0;n<800;++n)e.process(0);
    e.configure(Fx(fx),0);assert(e.process(0)==0);assert(!e.ringing());
  }
  Dsp::Freeze freeze;for(int n=0;n<1024;++n)freeze.remember(int16_t(n*25-12000),128+(n/64)%12);
  assert(freeze.available());uint8_t pose=0;
  int16_t a=freeze.sample(4095,256,pose);assert(pose>=128&&pose<=139);
  for(int n=1;n<64;++n)freeze.sample(4095,256,pose);
  assert(freeze.sample(4095,256,pose)==a);
  freeze.release();int16_t slow=freeze.sample(4095,64,pose);assert(slow==a);
  int16_t fraction=freeze.sample(4095,64,pose);assert(fraction>a&&fraction<a+25);
  assert(Panel::scale(0)==64);assert(Panel::scale(2048)==256);assert(Panel::scale(4095)>=1020);
  assert(Panel::mode(false,true)==PlaybackMode::Speech);
  assert(Panel::mode(true,true)==PlaybackMode::Gate);
  assert(Panel::mode(true,false)==PlaybackMode::Queue);
  assert(Panel::selection(4095,12)==11);
  puts("All FX dry/switch-tail behavior, ERROR loop/rate, pot scales and switch decoding OK");
}
