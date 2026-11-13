#pragma once
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
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
struct FakeSerial{void begin(int){}};
inline FakeSerial Serial;
