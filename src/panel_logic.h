#pragma once
#include <stdint.h>
#include <math.h>

enum class PlaybackMode : uint8_t { Speech, Gate, Queue };
enum class Fx : uint8_t { Off, Ring, Reverb, Delay, Crush, Drive, Lowpass, Highpass, Count };

namespace Panel {
constexpr uint16_t Noon = 2048, Maximum = 4095, Deadband = 55;
inline PlaybackMode mode(bool a, bool b) {
  if (a && b) return PlaybackMode::Gate;
  if (a && !b) return PlaybackMode::Queue;
  return PlaybackMode::Speech;
}
inline const char *modeName(PlaybackMode m) {
  return m == PlaybackMode::Queue ? "queue" : m == PlaybackMode::Gate ? "gate" : "speech";
}
inline uint16_t scale(uint16_t adc) {
  int offset = int(adc) - Noon;
  if (offset >= -int(Deadband) && offset <= int(Deadband)) return 256;
  offset += offset > 0 ? -Deadband : Deadband;
  return uint16_t(256.0f * exp2f(2.0f * offset / (Noon - Deadband)) + .5f);
}
inline uint16_t amount(uint16_t adc) {
  return adc <= 70 ? 0 : uint16_t((uint32_t(adc - 70) * 32767) / (Maximum - 70));
}
inline uint8_t bounded(int value, uint8_t maximum) {
  return value < 0 ? 0 : value > maximum ? maximum : uint8_t(value);
}
inline unsigned selection(uint16_t sound, unsigned count) {
  return (uint32_t(sound > 4000 ? 4000 : sound) * count) / 4001;
}
}
