#include <cassert>
#include <cstdio>
#include "stubs/Arduino.h"
uint32_t testNow=0;bool testPins[32]{};int testAdc[32]{},testPwm[32]{};
namespace TalkoStartup{constexpr uint8_t Point=10;}
#define TALKO_CONSOLE_HOST
#include "../src/audio.cpp"
static void pump(unsigned n=200){while(n--){++testNow;serviceConsole();}}
static void command(const char *s){Serial.input+=s;Serial.input+='\n';pump();}
static void label(uint8_t bank,uint8_t item){PlaybackEvent e{uint16_t((bank<<8)|(item+1))};assert(queue_try_add(&playbackEvents,&e));}
static void stop(){PlaybackEvent e{0};assert(queue_try_add(&playbackEvents,&e));}
int main(){
 setupAudio();pump();assert(Serial.output.empty());
 PanelControls controls;setPanelControls(controls,true);pump();assert(Serial.output.empty());
 label(0,0);stop();label(0,0);stop();pump();assert(Serial.output=="HELLO\r\nHELLO\r\n");Serial.output.clear();
 // STOP as a vocabulary label is retained; automatic end markers are omitted.
 label(0,9);stop();label(13,0);stop();pump();assert(Serial.output=="STOP\r\nDTMF_0\r\n");Serial.output.clear();
 Report report{{0,0,0,PlaybackMode::Speech},Event::Done};assert(queue_try_add(&reports,&report));pump();assert(Serial.output.empty());
 command("STATUS");assert(Serial.output.find("Talko_SpeakJet 0.9.3 build=normal")!=std::string::npos);assert(Serial.output.find("PANEL ")!=std::string::npos);Serial.output.clear();
 pump();setPanelControls(controls,true);pump();assert(Serial.output.empty());
 command("STATS ON");assert(queue_try_add(&reports,&report));stop();pump();assert(Serial.output.find("RESULT ")!=std::string::npos);assert(Serial.output.find("STOP\r\n")!=std::string::npos);Serial.output.clear();
 // Disconnect restores the labels-only default even after diagnostic use.
 Serial.connected=false;pump();Serial.connected=true;pump();assert(Serial.output.empty());
 assert(queue_try_add(&reports,&report));label(0,5);stop();Serial.writeLimit=1;pump();assert(Serial.output=="YES\r\n");Serial.output.clear();Serial.writeLimit=4096;
 command("\x1eSAM\x1f");assert(Serial.output.find("\x1eSAM:-\x1f")!=std::string::npos);Serial.output.clear();
 Serial.connected=false;pump();Serial.connected=true;label(0,6);stop();pump();assert(Serial.output=="NO\r\n");
 puts("Actual serial service: labels only by default, no banner/PANEL/STOP, explicit diagnostics/animation, reconnect reset and bounded writes OK");
}
