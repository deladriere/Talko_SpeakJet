#pragma once

#include <stddef.h>
#include <stdint.h>

struct Entry {
  const char *label;
  const uint8_t *bytes;
  uint8_t count;
  uint8_t singleCode;
};

template <size_t N>
constexpr Entry phrase(const char *label, const uint8_t (&bytes)[N]) {
  static_assert(N <= 64, "A test phrase must fit the SpeakJet FIFO");
  return {label, bytes, N, 0};
}

constexpr Entry sound(const char *label, uint8_t code) {
  return {label, nullptr, 0, code};
}

struct Bank {
  const Entry *entries;
  size_t count;
  uint8_t firstCode;
};

template <size_t N>
constexpr Bank bank(const Entry (&entries)[N]) {
  return {entries, N, 0};
}

constexpr Bank nativeBank(uint8_t first, uint8_t last) {
  return {nullptr, size_t(last - first + 1), first};
}

// Native effects in the same byte order as Emy.
constexpr uint8_t FirstEffectCode = 200;
constexpr const char *emyEffectLabels[] = {
    "ROBOT_0", "ROBOT_1", "ROBOT_2", "ROBOT_3", "ROBOT_4",
    "ROBOT_5", "ROBOT_6", "ROBOT_7", "ROBOT_8", "ROBOT_9",
    "ALARM_0", "ALARM_1", "ALARM_2", "ALARM_3", "ALARM_4",
    "ALARM_5", "ALARM_6", "ALARM_7", "ALARM_8", "ALARM_9",
    "BEEP_0", "BEEP_1", "BEEP_2", "BEEP_3", "BEEP_4",
    "BEEP_5", "BEEP_6", "BEEP_7", "BEEP_8", "BEEP_9",
    "BIO_0", "BIO_1", "BIO_2", "BIO_3", "BIO_4",
    "BIO_5", "BIO_6", "BIO_7", "BIO_8", "BIO_9",
    "DTMF_0", "DTMF_1", "DTMF_2", "DTMF_3", "DTMF_4",
    "DTMF_5", "DTMF_6", "DTMF_7", "DTMF_8", "DTMF_9",
    "DTMF_S", "DTMF_R", "SONAR_PING", "PISTOLSHOT", "WOW",
};
static_assert(sizeof(emyEffectLabels) / sizeof(emyEffectLabels[0]) == 255 - FirstEffectCode);

// Generated from banks/catalogue.json; edit the catalogue and regenerate.
#include "banks_generated.h"
