#pragma once
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <algorithm>
#include <string>
using std::min;using std::max;
#define __isr
#define __not_in_flash_func(name) name
inline void tight_loop_contents(){}
template<class T> T constrain(T v,T lo,T hi){return min(hi,max(lo,v));}
constexpr int PWM_IRQ_WRAP=0;
inline void irq_set_exclusive_handler(int,void(*)()){}
inline void irq_set_enabled(int,bool){}
using uint=unsigned;
constexpr int INPUT_PULLUP=2,CHANGE=1;
extern uint32_t testNow;
extern bool testPins[32];
extern int testAdc[32],testPwm[32];
inline uint32_t millis(){return testNow;}
inline uint32_t micros(){return testNow*1000;}
inline int analogRead(uint p){return testAdc[p];}
inline void analogReadResolution(int){}
inline void pinMode(uint,int){}
inline void delay(unsigned t){testNow+=t;}
inline int digitalPinToInterrupt(uint p){return p;}
inline void attachInterrupt(uint,void(*)(),int){}
inline long random(long n){return n?17%n:0;}
inline long random(long a,long b){return a+random(b-a);}
struct FakeSerial {
  bool connected=true;unsigned writeLimit=4096;std::string input,output;
  void begin(int){} explicit operator bool()const{return connected;}
  int available()const{return input.size();}
  int read(){unsigned char c=input[0];input.erase(0,1);return c;}
  int availableForWrite()const{return writeLimit;}
  size_t write(const uint8_t *data,size_t n){output.append(reinterpret_cast<const char*>(data),n);return n;}
};
inline FakeSerial Serial;
