#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>
#include "audio.h"
#include "banks.h"
#ifndef TALKO_PANEL_HOST
#include "talko_startup.h"
#endif

namespace {
constexpr uint ErrorPin=0,GatePin=2,EncoderA=3,EncoderB=4,SegmentA=5,DecimalPoint=12;
constexpr uint SelectPin=13,ModeB=14,ModeA=15,SoundPin=26,FxPin=27,SpeedPin=28,PitchPin=29;
constexpr uint Brightness=100;
constexpr uint8_t BankGlyphs[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71};
static_assert(BankCount<=sizeof(BankGlyphs));
constexpr uint8_t FxGlyphs[]={0x40,0x5c,0x50,0x5e,0x39,0x6d,0x38,0x76};
static_assert(sizeof(FxGlyphs)==unsigned(Fx::Count));
constexpr uint8_t ModeGlyphs[]={0x6d,0x3d,0x67};
constexpr uint8_t Snake[]={0,1,6,4,3,2,6,5};
volatile uint8_t bankCursor=0,fxCursor=unsigned(Fx::Delay);
volatile bool fxPage=false;
uint8_t lastEncoder=0;
int8_t encoderSteps=0;
volatile uint8_t gateEvents=0;
volatile uint16_t gateOpens=0;
uint32_t lastGateEdge=0,lastPotAt=0,lastActivity=0,modeChangedAt=0,modeShownUntil=0;
PlaybackMode mode=PlaybackMode::Speech,rawMode=PlaybackMode::Speech;
bool gateWasClosed=false;
PanelControls controls;

struct Button {
  uint pin;bool raw=false,held=false;uint32_t changedAt=0;
  bool update(uint32_t now) {
    bool next=!gpio_get(pin);
    if(next!=raw){raw=next;changedAt=now;}
    if(raw==held || now-changedAt<8)return false;
    held=raw;return true;
  }
};
Button selectButton{SelectPin},errorButton{ErrorPin};
struct Pot {
  uint pin;int filtered=0,last=0;
  void begin(){last=analogRead(pin);filtered=last*8;}
  int value()const{return filtered/8;}
  bool update(){filtered+=analogRead(pin)-value();if(abs(value()-last)<24)return false;last=value();return true;}
};
Pot soundPot{SoundPin},pitch{PitchPin},speed{SpeedPin},fx{FxPin};

uint8_t encoderState(){uint32_t p=gpio_get_all();return (((p>>EncoderA)&1)<<1)|((p>>EncoderB)&1);}
void encoderInterrupt(){
  constexpr int8_t steps[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  uint8_t next=encoderState();encoderSteps+=steps[(lastEncoder<<2)|next];lastEncoder=next;
  if(encoderSteps==4 || encoderSteps==-4){
    int direction=encoderSteps>0?1:-1;encoderSteps=0;
    if(fxPage)fxCursor=Panel::bounded(int(fxCursor)+direction,unsigned(Fx::Count)-1);
    else bankCursor=Panel::bounded(int(bankCursor)+direction,BankCount-1);
  }
}
void gateInterrupt(){
  if(gpio_get(GatePin)){gateEvents|=2;return;}
  uint32_t now=micros();
  if(now-lastGateEdge>=1500){lastGateEdge=now;gateEvents|=1;if(gateOpens<65535)++gateOpens;}
}
void draw(uint8_t glyph,bool dot,uint brightness){
  static uint32_t previous=~0u;
  uint32_t signature=glyph|(uint32_t(dot)<<8)|(brightness<<16);
  if(signature==previous)return;previous=signature;
  for(uint i=0;i<7;++i)pwm_set_gpio_level(SegmentA+i,(glyph&(1u<<i))?brightness:0);
  pwm_set_gpio_level(DecimalPoint,dot?Brightness:0);
}
void startupDraw(uint8_t glyph){draw(glyph&127,glyph&128,Brightness);}
void readControls(){
  controls.sound=soundPot.value();controls.pitch=pitch.value();controls.speed=speed.value();controls.fx=fx.value();
  controls.effect=Fx(fxCursor);controls.mode=mode;controls.error=errorButton.held;
  uint32_t pins=gpio_get_all();controls.modeA=pins&(1u<<ModeA);controls.modeB=pins&(1u<<ModeB);
}
}

void setup(){
  TalkoStartup::prepareLed();Serial.begin(115200);
  for(uint p:{ErrorPin,GatePin,EncoderA,EncoderB,SelectPin,ModeA,ModeB})pinMode(p,INPUT_PULLUP);
  for(uint pin=SegmentA;pin<=DecimalPoint;++pin){
    uint slice=pwm_gpio_to_slice_num(pin);pwm_set_wrap(slice,255);pwm_set_gpio_level(pin,0);
    gpio_set_function(pin,GPIO_FUNC_PWM);pwm_set_enabled(slice,true);
  }
  analogReadResolution(12);soundPot.begin();pitch.begin();speed.begin();fx.begin();
  setupAudio();
  TalkoStartup::showVersion(VersionMajor,VersionMinor,startupDraw,sayStartupPart,audioBusy);
  delay(25);lastEncoder=encoderState();
  attachInterrupt(digitalPinToInterrupt(EncoderA),encoderInterrupt,CHANGE);
  attachInterrupt(digitalPinToInterrupt(EncoderB),encoderInterrupt,CHANGE);
  lastGateEdge=micros()-1500;attachInterrupt(digitalPinToInterrupt(GatePin),gateInterrupt,CHANGE);
  mode=rawMode=Panel::mode(gpio_get(ModeA),gpio_get(ModeB));
  readControls();setPanelControls(controls,true);lastActivity=millis();
}
void loop(){
  uint32_t now=millis();
  bool moved=false;
  if(now-lastPotAt>=2){lastPotAt=now;moved=soundPot.update()|pitch.update()|speed.update()|fx.update();}
  bool selected=selectButton.update(now),errorChanged=errorButton.update(now);
  if(selected && !selectButton.held)fxPage=!fxPage;
  uint32_t lock=save_and_disable_interrupts();
  uint8_t events=gateEvents;gateEvents=0;uint16_t opens=gateOpens;gateOpens=0;
  uint8_t bank=bankCursor,effect=fxCursor;
  restore_interrupts(lock);
  static uint8_t oldBank=255,oldEffect=255;
  bool encoderMoved=bank!=oldBank || effect!=oldEffect;oldBank=bank;oldEffect=effect;
  PlaybackMode next=Panel::mode(gpio_get(ModeA),gpio_get(ModeB));
  if(next!=rawMode){rawMode=next;modeChangedAt=now;}
  bool modeChanged=next!=mode && now-modeChangedAt>=10;
  if(modeChanged){mode=next;stopAudio();modeShownUntil=now+550;}
  bool gate=!gpio_get(GatePin),closed=mode==PlaybackMode::Gate&&!gate;
  if(mode==PlaybackMode::Gate && ((events&2)||(closed&&!gateWasClosed)))stopAudio();
  gateWasClosed=closed;
  readControls();
  setPanelControls(controls,modeChanged||errorChanged||selected||encoderMoved);
  if(!closed){
    if(modeChanged && mode==PlaybackMode::Gate && gate)++opens;
    for(unsigned n=0;n<opens;++n)triggerAudio(bank,Panel::selection(soundPot.value(),banks[bank].count),mode);
  }
  if(moved||selected||errorChanged||encoderMoved||modeChanged||events||audioBusy()||audioFrozen()||errorButton.held)lastActivity=now;
  uint8_t glyph=fxPage?FxGlyphs[effect]:BankGlyphs[bank];
  bool dot=fxPage;
  uint brightness=Brightness;
  if(int32_t(modeShownUntil-now)>0)glyph=ModeGlyphs[unsigned(mode)];
  if(errorButton.held){
    static uint32_t lastFlicker=0;static uint8_t level=Brightness;
    if(now-lastFlicker>=50){lastFlicker=now;level=random(100)<30?random(20,70):80+random(-10,20);if(random(100)<5)level=0;}
    brightness=level;
  }else if(now-lastActivity>=5*60*1000){glyph=1u<<Snake[(now/120)%sizeof(Snake)];dot=false;brightness=40;}
  draw(glyph,dot,brightness);
  TalkoStartup::setLed(mode==PlaybackMode::Queue && nativeBuffered()>32 ? 70u<<16 : 0);
  serviceConsole();
  // Panel/USB updates need at most 1 kHz; leave SRAM bus time for core 1.
  // Encoder and gate edges are captured by interrupts between updates.
  delay(1);
}
