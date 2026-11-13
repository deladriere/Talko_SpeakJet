#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>

#include "audio.h"
#include "banks.h"
#include "talko_startup.h"

namespace {

constexpr uint GatePin = 2;
constexpr uint EncoderA = 3;
constexpr uint EncoderB = 4;
constexpr uint SegmentA = 5;
constexpr uint DecimalPoint = 12;
constexpr uint ModeB = 14;
constexpr uint ModeA = 15;
constexpr uint SoundPot = 26;
constexpr uint SoundMax = 4000;
constexpr uint DisplayBrightness = 100;
constexpr uint GateDebounceUs = 4000;

static_assert(BankCount <= sizeof(TalkoStartup::Digits));

uint8_t selectedBank = 0;
uint8_t lastEncoderState = 0;
int8_t encoderSteps = 0;
volatile uint8_t encoderBank = 0;
int filteredSound = 0;
uint32_t lastPotRead = 0;
bool gateWasClosed = false;

enum GateEvent : uint8_t { GateOpened = 1, GateClosed = 2 };
volatile uint8_t gateEvents = 0;
uint32_t lastGateEdge = 0;

void gateInterrupt() {
  // The input transistor inverts the external gate.
  if (gpio_get(GatePin)) {
    gateEvents |= GateClosed;
    return;
  }
  uint32_t now = micros();
  if (now - lastGateEdge >= GateDebounceUs) {
    lastGateEdge = now;
    gateEvents |= GateOpened;
  }
}

uint8_t encoderState() {
  uint32_t pins = gpio_get_all();
  return (((pins >> EncoderA) & 1u) << 1) | ((pins >> EncoderB) & 1u);
}

void encoderInterrupt() {
  // Full quadrature decoding, as in the Talko 2 and S.A.M. firmwares.
  constexpr int8_t transitions[16] = {
       0, -1,  1,  0,
       1,  0,  0, -1,
      -1,  0,  0,  1,
       0,  1, -1,  0,
  };
  uint8_t state = encoderState();
  encoderSteps += transitions[(lastEncoderState << 2) | state];
  lastEncoderState = state;
  if (encoderSteps == 4 || encoderSteps == -4) {
    encoderBank = constrain(int(encoderBank) + (encoderSteps > 0 ? 1 : -1), 0, BankCount - 1);
    encoderSteps = 0;
  }
}

void drawDisplay(uint8_t mask) {
  static uint8_t previous = 0xff;
  if (mask == previous) {
    return;
  }
  previous = mask;
  for (uint bit = 0; bit < 8; ++bit) {
    pwm_set_gpio_level(SegmentA + bit, (mask & (1u << bit)) ? DisplayBrightness : 0);
  }
}

} // namespace

void setup() {
  TalkoStartup::prepareLed();
  Serial.begin(115200);
  pinMode(GatePin, INPUT_PULLUP);
  pinMode(EncoderA, INPUT_PULLUP);
  pinMode(EncoderB, INPUT_PULLUP);
  pinMode(ModeA, INPUT_PULLUP);
  pinMode(ModeB, INPUT_PULLUP);
  for (uint pin = SegmentA; pin <= DecimalPoint; ++pin) {
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(pin, 0);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
  }
  analogReadResolution(12);
  filteredSound = analogRead(SoundPot) * 8;
  setupAudio();
  TalkoStartup::showVersion(VersionMajor, VersionMinor, drawDisplay, sayStartupPart, audioBusy);

  lastGateEdge = micros() - GateDebounceUs;
  attachInterrupt(digitalPinToInterrupt(GatePin), gateInterrupt, CHANGE);
  delay(25);
  lastEncoderState = encoderState();
  attachInterrupt(digitalPinToInterrupt(EncoderA), encoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(EncoderB), encoderInterrupt, CHANGE);
  drawDisplay(TalkoStartup::Digits[selectedBank]);
}

void loop() {
  uint32_t now = millis();
  if (now - lastPotRead >= 2) {
    lastPotRead = now;
    filteredSound += analogRead(SoundPot) - filteredSound / 8;
  }

  uint32_t interruptState = save_and_disable_interrupts();
  selectedBank = encoderBank;
  uint8_t events = gateEvents;
  gateEvents = 0;
  uint32_t pins = gpio_get_all();
  restore_interrupts(interruptState);

  constexpr uint32_t modePins = (1u << ModeA) | (1u << ModeB);
  const uint32_t switchPins = pins & modePins;
  PlaybackMode mode = PlaybackMode::Speech;
  if (switchPins == modePins) {
    mode = PlaybackMode::Gate;
  } else if (switchPins == (1u << ModeA)) {
    mode = PlaybackMode::Queue;
  }
  bool gateActive = (pins & (1u << GatePin)) == 0;
  bool gateClosed = mode == PlaybackMode::Gate && !gateActive;
  if (mode == PlaybackMode::Gate && ((events & GateClosed) || (gateClosed && !gateWasClosed))) {
    stopAudio();
  }
  gateWasClosed = gateClosed;

  if ((events & GateOpened) && !gateClosed) {
    const Bank &bank = banks[selectedBank];
    uint value = constrain(filteredSound / 8, 0, int(SoundMax));
    uint8_t item = (value * bank.count) / (SoundMax + 1);
    triggerAudio(selectedBank, item, mode);
  }
  drawDisplay(TalkoStartup::Digits[selectedBank]);
  serviceConsole();
}
