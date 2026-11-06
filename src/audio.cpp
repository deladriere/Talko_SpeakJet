#include "audio.h"
#include "banks.h"

#include <Arduino.h>
#include <atomic>
#include <cstdio>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <pico/util/queue.h>
#include <speakjet.h>

namespace {

constexpr uint AudioPin = 1;
constexpr uint CarrierCycles = 12;
constexpr uint BlockSamples = 64;
constexpr uint BlockBudgetUs = (BlockSamples * 1000000u + SJ_SAMPLE_RATE / 2) / SJ_SAMPLE_RATE;
constexpr uint RingSize = 1024;
constexpr uint RingMask = RingSize - 1;
constexpr uint PrefillSamples = BlockSamples * 2;
// Leave room for a new gate to affect a phoneme that is still playing.
constexpr uint QueueAheadSamples = BlockSamples * 4;
constexpr uint PendingCapacity = 16;
static_assert((RingSize & RingMask) == 0 &&
              RingSize >= QueueAheadSamples && QueueAheadSamples >= PrefillSamples);

struct Request {
  uint32_t epoch;
  uint8_t bank;
  uint8_t item;
  PlaybackMode mode;
};

enum class Event : uint8_t { Play, Done, Cut, Error, Queued, Dropped };

struct Report {
  Request request;
  Event event;
  uint32_t blocks;
  uint32_t totalUs;
  uint32_t peakUs;
  uint32_t lateBlocks;
  uint32_t underrunEvents;
  uint32_t underrunSamples;
  uint8_t pending;
  uint8_t bufferBytes;
};

queue_t requests;
queue_t reports;
std::atomic<bool> audioReady{false};
std::atomic<bool> playing{false};
std::atomic<bool> requestActive{false};
std::atomic<bool> renderDone{false};
std::atomic<uint32_t> gateEpoch{0};
std::atomic<uint32_t> readIndex{0};
std::atomic<uint32_t> writeIndex{0};
std::atomic<uint32_t> underrunEvents{0};
std::atomic<uint32_t> underrunSamples{0};

// Core 1 prepares this state before publishing playing=true. The PWM ISR
// stops accessing it before publishing playing=false.
uint32_t streamEpoch;
bool inUnderrun = false;
uint16_t duties[RingSize];

sj_chip *chip;
uint audioSlice;
uint audioChannel;
uint pwmPeriod;
uint carrierPhase = 0;

uint nextDuty() {
  if (!playing.load(std::memory_order_acquire)) {
    return pwmPeriod / 2;
  }
  if (streamEpoch != gateEpoch.load(std::memory_order_relaxed)) {
    playing.store(false, std::memory_order_release);
    return pwmPeriod / 2;
  }

  const uint32_t read = readIndex.load(std::memory_order_relaxed);
  // Read done first so a completed stream includes its final published block.
  const bool done = renderDone.load(std::memory_order_acquire);
  if (read != writeIndex.load(std::memory_order_acquire)) {
    uint duty = duties[read & RingMask];
    readIndex.store(read + 1, std::memory_order_release);
    inUnderrun = false;
    return duty;
  }

  if (done) {
    playing.store(false, std::memory_order_release);
  } else {
    underrunSamples.store(underrunSamples.load(std::memory_order_relaxed) + 1,
                          std::memory_order_relaxed);
    if (!inUnderrun) {
      underrunEvents.store(underrunEvents.load(std::memory_order_relaxed) + 1,
                           std::memory_order_relaxed);
      inUnderrun = true;
    }
  }
  return pwmPeriod / 2;
}

void __isr __not_in_flash_func(audioInterrupt)() {
  pwm_clear_irq(audioSlice);
  if (++carrierPhase == CarrierCycles) {
    carrierPhase = 0;
    pwm_set_chan_level(audioSlice, audioChannel, nextDuty());
  }
}

bool cancelled(const Request &request) {
  return request.epoch != gateEpoch.load(std::memory_order_acquire);
}

Entry bankEntry(const Request &request) {
  const Bank &bank = banks[request.bank];
  if (bank.entries) {
    return bank.entries[request.item];
  }
  const uint8_t code = bank.firstCode + request.item;
  const char *label = code >= FirstEffectCode ? emyEffectLabels[code - FirstEffectCode]
                                             : sj_sound_name(code);
  return sound(label, code);
}

bool loadEntry(const Entry &entry) {
  if (!entry.bytes) {
    return sj_write_byte(chip, entry.singleCode);
  }
  for (uint i = 0; i < entry.count; ++i) {
    if (!sj_write_byte(chip, entry.bytes[i])) {
      return false;
    }
  }
  return true;
}

void appendQueued(uint32_t epoch) {
  Request next;
  bool added = false;
  while (queue_try_peek(&requests, &next) && next.mode == PlaybackMode::Queue &&
         next.epoch == epoch && !cancelled(next)) {
    const Entry entry = bankEntry(next);
    const int length = entry.bytes ? entry.count : 1;
    if (sj_buffered(chip) + length > SJ_BUFFER_SIZE) {
      break;
    }
    // Core 1 is the only consumer. Gate cuts invalidate requests by epoch.
    queue_try_remove(&requests, &next);
    if (loadEntry(entry)) {
      Report queued{next, Event::Queued};
      queued.pending = queue_get_level(&requests);
      queued.bufferBytes = sj_buffered(chip);
      queue_try_add(&reports, &queued);
      added = true;
    }
  }
  if (added) {
    sj_join_pending(chip);
  }
}

void renderEntry(const Request &request) {
  Report report{request, Event::Play};
  if (chip) {
    sj_power_on(chip);
  }
  if (!chip || !loadEntry(bankEntry(request))) {
    report.event = Event::Error;
    queue_try_add(&reports, &report);
    return;
  }
  if (cancelled(request)) {
    return;
  }

  readIndex.store(0, std::memory_order_relaxed);
  writeIndex.store(0, std::memory_order_relaxed);
  renderDone.store(false, std::memory_order_relaxed);
  underrunEvents.store(0, std::memory_order_relaxed);
  underrunSamples.store(0, std::memory_order_relaxed);
  inUnderrun = false;
  streamEpoch = request.epoch;
  queue_try_add(&reports, &report);

  uint32_t write = 0;
  float samples[BlockSamples];
  bool started = false;
  bool more = true;
  const bool queued = request.mode == PlaybackMode::Queue;
  const uint ahead = queued ? QueueAheadSamples : RingSize;
  while (more && !cancelled(request)) {
    if (write - readIndex.load(std::memory_order_acquire) > ahead - BlockSamples) {
      tight_loop_contents();
      continue;
    }
    const uint32_t startUs = micros();
    if (queued) {
      appendQueued(request.epoch);
    }
    more = sj_render(chip, samples, BlockSamples);
    for (uint i = 0; i < BlockSamples; ++i) {
      const float sample = constrain(samples[i], -1.0f, 1.0f);
      duties[(write + i) & RingMask] = uint16_t((sample + 1.0f) * (pwmPeriod * 0.5f));
    }
    const uint32_t elapsedUs = micros() - startUs;
    ++report.blocks;
    report.totalUs += elapsedUs;
    report.peakUs = max(report.peakUs, elapsedUs);
    report.lateBlocks += elapsedUs > BlockBudgetUs;
    if (cancelled(request)) {
      break;
    }
    write += BlockSamples;
    writeIndex.store(write, std::memory_order_release);
    if (!more) {
      renderDone.store(true, std::memory_order_release);
    }
    if (!started && (write >= PrefillSamples || !more)) {
      playing.store(true, std::memory_order_release);
      started = true;
    }
  }
  while (playing.load(std::memory_order_acquire)) {
    tight_loop_contents();
  }

  report.event = cancelled(request) ? Event::Cut : Event::Done;
  report.underrunEvents = underrunEvents.load(std::memory_order_relaxed);
  report.underrunSamples = underrunSamples.load(std::memory_order_relaxed);
  queue_try_add(&reports, &report);
}

int formatReport(char *line, size_t capacity, const Report &report) {
  const auto &request = report.request;
  const Entry entry = bankEntry(request);
  char nativeDetails[32] = {};
  if (!entry.bytes) {
    if (entry.singleCode >= FirstEffectCode) {
      snprintf(nativeDetails, sizeof(nativeDetails), " code=%u sj=%s",
               unsigned(entry.singleCode), sj_sound_name(entry.singleCode));
    } else {
      snprintf(nativeDetails, sizeof(nativeDetails), " code=%u", unsigned(entry.singleCode));
    }
  }
  const char *mode = request.mode == PlaybackMode::Queue ? "queue" :
                     request.mode == PlaybackMode::Gate ? "gate" : "speech";
  if (report.event == Event::Play) {
    return snprintf(line, capacity, "PLAY bank=%u item=%u label=%s%s mode=%s\r\n",
                    unsigned(request.bank), unsigned(request.item) + 1,
                    entry.label, nativeDetails, mode);
  }
  if (report.event == Event::Queued) {
    return snprintf(line, capacity,
                    "QUEUE bank=%u item=%u label=%s%s pending=%u buffer_bytes=%u\r\n",
                    unsigned(request.bank), unsigned(request.item) + 1, entry.label,
                    nativeDetails, unsigned(report.pending), unsigned(report.bufferBytes));
  }
  if (report.event == Event::Dropped) {
    return snprintf(line, capacity, "DROP bank=%u item=%u label=%s%s mode=%s reason=full\r\n",
                    unsigned(request.bank), unsigned(request.item) + 1,
                    entry.label, nativeDetails, mode);
  }
  const char *status = report.event == Event::Done ? "done" :
                       report.event == Event::Cut ? "cut" : "error";
  const uint32_t meanUs = report.blocks ? report.totalUs / report.blocks : 0;
  return snprintf(line, capacity,
                  "RESULT bank=%u item=%u label=%s status=%s blocks=%lu avg_us=%lu "
                  "peak_us=%lu budget_us=%u late_blocks=%lu underrun_events=%lu "
                  "underrun_samples=%lu%s mode=%s\r\n",
                  unsigned(request.bank), unsigned(request.item) + 1, entry.label, status,
                  (unsigned long)report.blocks, (unsigned long)meanUs,
                  (unsigned long)report.peakUs, BlockBudgetUs, (unsigned long)report.lateBlocks,
                  (unsigned long)report.underrunEvents, (unsigned long)report.underrunSamples,
                  nativeDetails, mode);
}

} // namespace

bool core1_separate_stack = true;

void setupAudio() {
  queue_init(&requests, sizeof(Request), PendingCapacity);
  queue_init(&reports, sizeof(Report), 16);

  audioSlice = pwm_gpio_to_slice_num(AudioPin);
  audioChannel = pwm_gpio_to_channel(AudioPin);
  constexpr uint carrierRate = SJ_SAMPLE_RATE * CarrierCycles;
  pwmPeriod = (clock_get_hz(clk_sys) + carrierRate / 2) / carrierRate;

  pwm_config config = pwm_get_default_config();
  pwm_config_set_wrap(&config, pwmPeriod - 1);
  pwm_init(audioSlice, &config, false);
  pwm_set_chan_level(audioSlice, audioChannel, pwmPeriod / 2);
  gpio_set_function(AudioPin, GPIO_FUNC_PWM);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, audioInterrupt);
  pwm_clear_irq(audioSlice);
  pwm_set_irq_enabled(audioSlice, true);
  irq_set_enabled(PWM_IRQ_WRAP, true);
  pwm_set_enabled(audioSlice, true);
  audioReady.store(true, std::memory_order_release);
}

void setup1() {
  while (!audioReady.load(std::memory_order_acquire)) {
    tight_loop_contents();
  }
  chip = sj_new();
}

void loop1() {
  Request request;
  queue_peek_blocking(&requests, &request);
  // Publish busy before the request stops being visible in the queue.
  requestActive.store(true, std::memory_order_release);
  queue_try_remove(&requests, &request);
  if (!cancelled(request)) {
    renderEntry(request);
  }
  requestActive.store(false, std::memory_order_release);
}

void triggerAudio(uint8_t bank, uint8_t item, PlaybackMode mode) {
  if (mode == PlaybackMode::Speech &&
      (!queue_is_empty(&requests) || requestActive.load(std::memory_order_acquire))) {
    return;
  }
  const Request request{gateEpoch.load(std::memory_order_relaxed), bank, item, mode};
  if (!queue_try_add(&requests, &request)) {
    const Report dropped{request, Event::Dropped};
    queue_try_add(&reports, &dropped);
  }
}

void stopAudio() {
  gateEpoch.store(gateEpoch.load(std::memory_order_relaxed) + 1,
                  std::memory_order_release);
}

bool audioPlaying() {
  return playing.load(std::memory_order_relaxed);
}

void serviceConsole() {
  static char line[256];
  static size_t sent = 0;
  static size_t length = 0;
  static bool connected = false;
  Report report;
  if (!Serial) {
    connected = false;
    sent = length;
    while (queue_try_remove(&reports, &report)) {}
    return;
  }
  const int available = Serial.availableForWrite();
  if (available <= 0) {
    return;
  }

  if (sent == length) {
    int count;
    if (!connected) {
      count = snprintf(line, sizeof(line),
                       "Talko 2 SpeakJet bench v0.7 mode=native engine=%s sample_hz=%u "
                       "block_samples=%u budget_us=%u prefill_samples=%u\r\n",
                       SJ_VERSION, unsigned(SJ_SAMPLE_RATE), BlockSamples, BlockBudgetUs,
                       PrefillSamples);
      connected = true;
    } else if (queue_try_remove(&reports, &report)) {
      count = formatReport(line, sizeof(line), report);
    } else {
      return;
    }
    length = count > 0 ? min(size_t(count), sizeof(line) - 1) : 0;
    sent = 0;
  }
  sent += Serial.write(reinterpret_cast<const uint8_t *>(line) + sent,
                       min(length - sent, size_t(available)));
}
