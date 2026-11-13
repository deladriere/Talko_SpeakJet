#include <vector>
#include <cassert>
#include <cstdio>
#include "stubs/Arduino.h"
uint32_t testNow=0;bool testPins[32];int testAdc[32]{},testPwm[32]{};
namespace TalkoStartup {
constexpr uint8_t Digits[]={0x3f,6,0x5b,0x4f,0x66,0x6d,0x7d,7,0x7f,0x6f};
inline void prepareLed(){}
inline void setLed(uint32_t){}
inline void showVersion(uint8_t,uint8_t,void(*)(uint8_t),void(*)(uint8_t),bool(*)(),int){}
}
#define TALKO_PANEL_HOST
#include "../src/main.cpp"
struct Trigger{uint8_t bank,item;PlaybackMode mode;};
std::vector<Trigger> triggered;unsigned cuts=0;PanelControls observed;
void setupAudio(){}
void triggerAudio(uint8_t b,uint8_t i,PlaybackMode m){triggered.push_back({b,i,m});}
void stopAudio(){++cuts;}
void sayStartupPart(uint8_t){}
bool audioBusy(){return false;}
bool audioFrozen(){return false;}
uint8_t nativeBuffered(){return 0;}
void setPanelControls(const PanelControls &p,bool){observed=p;}
void serviceConsole(){}
static void advance(unsigned ms){for(unsigned i=0;i<ms;++i){++testNow;loop();}}
static void gate(bool on){testPins[2]=!on;gateInterrupt();advance(5);}
static void switchMode(bool a,bool b){testPins[15]=a;testPins[14]=b;advance(20);}
int main(){
  for(auto &p:testPins)p=true;
  testAdc[29]=testAdc[28]=2048;setup();advance(20);
  assert(observed.mode==PlaybackMode::Gate);
  gate(true);assert(triggered.back().mode==PlaybackMode::Gate);unsigned before=cuts;gate(false);assert(cuts>before);
  switchMode(false,true);assert(observed.mode==PlaybackMode::Speech);
  gate(true);before=cuts;gate(false);assert(cuts==before);assert(triggered.back().mode==PlaybackMode::Speech);
  switchMode(true,false);assert(observed.mode==PlaybackMode::Queue);
  bankCursor=1;testAdc[26]=0;advance(100);gate(true);gate(false);assert(triggered.back().item==0);
  testAdc[26]=4095;advance(150);before=cuts;gate(true);gate(false);
  assert(cuts==before && triggered.back().mode==PlaybackMode::Queue && triggered.back().item==11);
  testAdc[29]=4095;testAdc[28]=0;testAdc[27]=3300;advance(200);
  assert(observed.pitch>4000 && observed.speed<10 && observed.fx>3200);
  testPins[13]=false;advance(12);testPins[13]=true;advance(12);assert(fxPage && testPwm[12]==100);
  unsigned oldBank=bankCursor,oldEffect=fxCursor;
  for(unsigned state:{1u,0u,2u,3u}){testPins[3]=state&2;testPins[4]=state&1;encoderInterrupt();}
  advance(3);assert(bankCursor==oldBank && fxCursor==oldEffect+1);
  testPins[0]=false;advance(12);assert(observed.error);assert(testPwm[12]==100);
  testPins[0]=true;advance(12);assert(!observed.error);
  testPins[13]=false;advance(12);testPins[13]=true;advance(12);assert(!fxPage && testPwm[12]==0);
  bankCursor=8;for(unsigned state:{1u,0u,2u,3u}){testPins[3]=state&2;testPins[4]=state&1;encoderInterrupt();}
  assert(bankCursor==8);
  for(unsigned state:{2u,0u,1u,3u}){testPins[3]=state&2;testPins[4]=state&1;encoderInterrupt();}
  assert(bankCursor==7);
  puts("Actual panel loop: three switch positions, gate cuts, queued selections, all ADCs, FX page, ERROR and bank reversal OK");
}
