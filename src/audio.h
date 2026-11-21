#pragma once
#include "panel_logic.h"
#include <stdint.h>
constexpr uint8_t VersionMajor=1, VersionMinor=0;
constexpr const char *FirmwareVersion="1.0";
struct PanelControls {
  uint16_t pitch=2048,speed=2048,fx=0;
  Fx effect=Fx::Delay;
  PlaybackMode mode=PlaybackMode::Speech;
  bool error=false;
  bool modeA=true,modeB=true;
  uint16_t sound=0;
};
void setupAudio();
void triggerAudio(uint8_t bank,uint8_t item,PlaybackMode mode);
void stopAudio();
void sayStartupPart(uint8_t part);
bool audioBusy();
bool audioFrozen();
uint8_t nativeBuffered();
void setPanelControls(const PanelControls &controls,bool announce=false);
void serviceConsole();
