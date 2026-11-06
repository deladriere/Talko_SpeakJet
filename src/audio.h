#pragma once

#include <stdint.h>

enum class PlaybackMode : uint8_t { Speech, Gate, Queue };

void setupAudio();
void triggerAudio(uint8_t bank, uint8_t item, PlaybackMode mode);
void stopAudio();
bool audioPlaying();
void serviceConsole();
