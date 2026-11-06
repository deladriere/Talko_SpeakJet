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

// Generated with upstream text2speakjet, --no-phrasealator --terminator none.
constexpr uint8_t hello[] = {184, 7, 133, 146, 14, 164};
constexpr uint8_t ready[] = {148, 14, 131, 174, 7, 128};
constexpr uint8_t robot[] = {148, 14, 164, 171, 135, 191};
constexpr uint8_t danger[] = {174, 14, 154, 141, 165, 7, 151};
constexpr uint8_t talko[] = {192, 14, 132, 195, 7, 164};
constexpr uint8_t pangram[] = {
    169, 7, 133, 4, 195, 147, 14, 129, 196, 4, 170, 148, 14, 163, 141, 4, 186,
    14, 135, 197, 187, 4, 165, 14, 134, 140, 199, 187, 4, 14, 164, 166, 7, 151,
    4, 169, 7, 133, 4, 145, 14, 154, 167, 7, 128, 4, 175, 14, 136, 181,
};

constexpr Entry words[] = {
    phrase("HELLO", hello), phrase("READY", ready), phrase("ROBOT", robot),
    phrase("DANGER", danger), phrase("TALKO", talko),
    phrase("THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG", pangram),
};

// Emy's SpeakJet number dictionary. The EOP byte and C string terminator are omitted.
constexpr uint8_t digitZero[] = {167, 7, 128, 7, 149, 164};
constexpr uint8_t digitOne[] = {147, 14, 135, 8, 141};
constexpr uint8_t digitTwo[] = {8, 191, 162};
constexpr uint8_t digitThree[] = {8, 190, 148, 8, 128};
constexpr uint8_t digitFour[] = {186, 7, 137, 153};
constexpr uint8_t digitFive[] = {186, 157, 166};
constexpr uint8_t digitSix[] = {8, 187, 129, 14, 194, 7, 187};
constexpr uint8_t digitSeven[] = {8, 187, 7, 131, 166, 131, 141};
constexpr uint8_t digitEight[] = {154, 4, 191};
constexpr uint8_t digitNine[] = {141, 14, 157, 141};

constexpr Entry spokenDigits[] = {
    phrase("ZERO", digitZero), phrase("ONE", digitOne), phrase("TWO", digitTwo),
    phrase("THREE", digitThree), phrase("FOUR", digitFour), phrase("FIVE", digitFive),
    phrase("SIX", digitSix), phrase("SEVEN", digitSeven), phrase("EIGHT", digitEight),
    phrase("NINE", digitNine),
};

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

constexpr Bank banks[] = {
    bank(words),
    nativeBank(128, 139), // Vowels
    nativeBank(128, 181), // Voiced
    nativeBank(170, 181), // Voiced stops
    nativeBank(182, 199), // Voiceless
    nativeBank(128, 199), // All allophones
    nativeBank(200, 254), // All effects
    nativeBank(240, 251), // DTMF
    bank(spokenDigits),
};
constexpr int BankCount = sizeof(banks) / sizeof(banks[0]);
