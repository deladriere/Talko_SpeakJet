#pragma once

#include <stdint.h>

constexpr uint8_t VersionMajor = 0;
constexpr uint8_t VersionMinor = 9;

enum class PlaybackMode : uint8_t { Speech, Gate, Queue };

void setupAudio();
void triggerAudio(uint8_t bank, uint8_t item, PlaybackMode mode);
void stopAudio();
void sayStartupPart(uint8_t part);
bool audioBusy();
void serviceConsole();
