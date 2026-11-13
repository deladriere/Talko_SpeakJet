#include "audio.h"
#include "banks.h"
#include "dsp.h"
#include "talko_startup.h"

#include <Arduino.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <pico/util/queue.h>
#include <speakjet.h>

namespace {

constexpr uint AudioPin = 1;
constexpr uint CarrierCycles = 12;
constexpr uint BlockSamples = 64;
uint blockBudgetUs; // derived from the actual PWM divider and system clock
constexpr uint RingSize = 1024;
constexpr uint RingMask = RingSize - 1;
constexpr uint PrefillSamples = BlockSamples * 2;
// Leave room for a new gate to affect a phoneme that is still playing.
constexpr uint QueueAheadSamples = BlockSamples * 3;
constexpr uint PendingCapacity = 16;
constexpr uint8_t StartupBank = BankCount;
constexpr uint8_t FreezeBank = BankCount + 2;
// SpeakJet allophones PO, OWIY, NE, TT.
constexpr uint8_t point[] = {199, 156, 141, 191};
static_assert((RingSize & RingMask) == 0 &&
              RingSize >= QueueAheadSamples && QueueAheadSamples >= PrefillSamples);

struct Request {
  uint32_t epoch;
  uint8_t bank;
  uint8_t item;
  PlaybackMode mode;
  uint8_t byteCount = 0;
  uint8_t bytes[64] = {};
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

std::atomic<uint32_t> performance{(256u<<16)|256u};
std::atomic<uint32_t> effectControl{uint32_t(Fx::Delay)<<16};
std::atomic<uint16_t> speedControl{2048};
std::atomic<bool> errorControl{false},frozen{false},voiceBusy{false};
std::atomic<uint8_t> synthBuffered{0};
PanelControls panelSnapshot;
#ifdef TALKO_DIAGNOSTICS
PanelControls injectedControls;
bool injected=false;
#endif
bool panelPending=true;
bool panelMonitor=false;
Dsp::Effects effects;
Dsp::Freeze freeze;
queue_t requests;
queue_t reports;
struct PlaybackEvent { uint16_t label; }; // zero means STOP
queue_t playbackEvents;
std::atomic<uint8_t> playedCode{0};
uint16_t labelStarts[RingSize];
uint8_t articulation[RingSize];
struct Marker { uint32_t offset; uint16_t label; };
Marker markers[64]; // core 1 only, bounded by the 64-byte synth FIFO
uint32_t markerRead, markerWrite, submittedBytes;
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
bool labelActive = false;
uint16_t duties[RingSize];

sj_chip *chip;
uint audioSlice;
uint audioChannel;
uint pwmPeriod;
uint carrierPhase = 0;

void playbackStopped() {
  labelActive = false;
  playedCode.store(0, std::memory_order_relaxed);
  const PlaybackEvent event{0};
  queue_try_add(&playbackEvents, &event);
}

uint nextDuty() {
  if (!playing.load(std::memory_order_acquire)) {
    return pwmPeriod / 2;
  }
  if (streamEpoch != gateEpoch.load(std::memory_order_relaxed)) {
    playbackStopped();
    playing.store(false, std::memory_order_release);
    return pwmPeriod / 2;
  }

  const uint32_t read = readIndex.load(std::memory_order_relaxed);
  // Read done first so a completed stream includes its final published block.
  const bool done = renderDone.load(std::memory_order_acquire);
  if (read != writeIndex.load(std::memory_order_acquire)) {
    const uint at = read & RingMask;
    if (labelStarts[at]) {
      if (labelActive) {
        const PlaybackEvent end{0};
        queue_try_add(&playbackEvents, &end);
      }
      labelActive = true;
      const PlaybackEvent event{labelStarts[at]};
      queue_try_add(&playbackEvents, &event);
    }
    playedCode.store(articulation[at], std::memory_order_relaxed);
    uint duty = duties[at];
    readIndex.store(read + 1, std::memory_order_release);
    inUnderrun = false;
    return duty;
  }

  playedCode.store(0, std::memory_order_relaxed);
  if (done) {
    playbackStopped();
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
  if (request.bank == FreezeBank) return {"ERROR", point, 0, 0};
  if (request.byteCount) return {"RAW", request.bytes, request.byteCount, 0};
  if (request.bank == StartupBank) {
    return request.item == TalkoStartup::Point ? phrase("POINT", point)
                                               : spokenDigits[request.item];
  }
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
    if (sj_buffer_half_full(chip) || sj_buffered(chip) + length > SJ_BUFFER_SIZE || markerWrite - markerRead >= 64) {
      break;
    }
    // Core 1 is the only consumer. Gate cuts invalidate requests by epoch.
    queue_try_remove(&requests, &next);
    if (loadEntry(entry)) {
      markers[markerWrite++ & 63] = {submittedBytes, uint16_t((next.bank << 8) | (next.item + 1))};
      submittedBytes += length;
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
  const bool idleFreeze = request.bank == FreezeBank;
  if (chip && !idleFreeze) {
    sj_power_on(chip);
  }
  if (!chip || (!idleFreeze && !loadEntry(bankEntry(request)))) {
    report.event = Event::Error;
    queue_try_add(&reports, &report);
    return;
  }
  if (cancelled(request)) {
    return;
  }

  markerRead = 0;
  markerWrite = idleFreeze ? 0 : 1;
  markers[0] = {0, uint16_t((request.bank << 8) | (request.item + 1))};
  const Entry initial = bankEntry(request);
  submittedBytes = initial.bytes ? initial.count : 1;
  readIndex.store(0, std::memory_order_relaxed);
  writeIndex.store(0, std::memory_order_relaxed);
  renderDone.store(false, std::memory_order_relaxed);
  underrunEvents.store(0, std::memory_order_relaxed);
  underrunSamples.store(0, std::memory_order_relaxed);
  inUnderrun = false;
  labelActive = false;
  streamEpoch = request.epoch;
  queue_try_add(&reports, &report);

  uint32_t write = 0;
  float samples[BlockSamples];
  sj_trace trace[BlockSamples];
  bool started = false;
  bool more = true;
  bool engineMore = !idleFreeze;
  bool idleLabelSent = false;
  const bool queued = request.mode == PlaybackMode::Queue;
  const uint ahead = QueueAheadSamples;
  while (more && !cancelled(request)) {
    if (write - readIndex.load(std::memory_order_acquire) > ahead - BlockSamples) {
      tight_loop_contents();
      continue;
    }
    const uint32_t startUs = micros();
    if (queued) {
      appendQueued(request.epoch);
    }
    if(queued && sj_buffered(chip)>0)engineMore=true;
    const uint32_t perf = performance.load(std::memory_order_relaxed);
    const uint16_t pitchScale = perf >> 16, rateScale = perf & 65535;
    const uint32_t fx = effectControl.load(std::memory_order_relaxed);
    effects.configure(Fx(fx >> 16), fx & 65535);
    const bool holding = errorControl.load(std::memory_order_acquire) && freeze.available();
    frozen.store(holding, std::memory_order_relaxed);
    const bool rendered = engineMore && !holding;
    if (rendered) {
      sj_set_performance(chip, pitchScale, rateScale);
      engineMore = sj_render_tagged(chip, samples, trace, BlockSamples);
    } else {
      for (uint i=0;i<BlockSamples;++i) { samples[i]=0;trace[i]={0,0}; }
    }
    if (!holding) freeze.release();
    synthBuffered.store(sj_buffered(chip), std::memory_order_relaxed);
    for (uint i = 0; i < BlockSamples; ++i) {
      const uint at = (write + i) & RingMask;
      labelStarts[at] = 0;
      articulation[at] = trace[i].code;
      if (trace[i].code && markerRead != markerWrite &&
          trace[i].byte_offset >= markers[markerRead & 63].offset) {
        labelStarts[at] = markers[markerRead++ & 63].label;
      }
      int16_t raw = int16_t(constrain(samples[i], -1.0f, 1.0f) * 32767.0f);
      if (holding) {
        raw = freeze.sample(speedControl.load(std::memory_order_relaxed),pitchScale,articulation[at]);
        if (idleFreeze && !idleLabelSent) {
          labelStarts[at] = uint16_t((FreezeBank<<8)|1);idleLabelSent=true;
        }
      } else if (rendered) freeze.remember(raw,trace[i].code);
      const int32_t output = effects.process(raw);
      duties[at] = uint16_t((uint32_t(output+32768)*pwmPeriod)>>16);
    }
    voiceBusy.store(engineMore || holding,std::memory_order_release);
    more = engineMore || holding || effects.ringing();
    const uint32_t elapsedUs = micros() - startUs;
    ++report.blocks;
    report.totalUs += elapsedUs;
    report.peakUs = max(report.peakUs, elapsedUs);
    report.lateBlocks += elapsedUs > blockBudgetUs;
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

  voiceBusy.store(false,std::memory_order_release);
  frozen.store(false,std::memory_order_relaxed);
  synthBuffered.store(0,std::memory_order_relaxed);
  if(cancelled(request)) freeze.clear();
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
                  (unsigned long)report.peakUs, blockBudgetUs, (unsigned long)report.lateBlocks,
                  (unsigned long)report.underrunEvents, (unsigned long)report.underrunSamples,
                  nativeDetails, mode);
}

} // namespace

bool core1_separate_stack = true;

void setupAudio() {
  queue_init(&requests, sizeof(Request), PendingCapacity);
  queue_init(&reports, sizeof(Report), 16);
  queue_init(&playbackEvents, sizeof(PlaybackEvent), 64);

  audioSlice = pwm_gpio_to_slice_num(AudioPin);
  audioChannel = pwm_gpio_to_channel(AudioPin);
  constexpr uint carrierRate = SJ_SAMPLE_RATE * CarrierCycles;
  pwmPeriod = (clock_get_hz(clk_sys) + carrierRate / 2) / carrierRate;
  blockBudgetUs = uint((uint64_t(BlockSamples) * pwmPeriod * CarrierCycles * 1000000u) /
                       clock_get_hz(clk_sys));

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
  effects.begin();
  audioReady.store(true, std::memory_order_release);
}

void setup1() {
  while (!audioReady.load(std::memory_order_acquire)) {
    tight_loop_contents();
  }
  chip = sj_new();
}

void loop1() {
  static bool idleHoldConsumed=false;
  if(!errorControl.load(std::memory_order_relaxed))idleHoldConsumed=false;
  Request request;
  if(!queue_try_peek(&requests,&request)) {
    if(!idleHoldConsumed && errorControl.load(std::memory_order_relaxed) && freeze.available()) {
      request={gateEpoch.load(std::memory_order_relaxed),FreezeBank,0,PlaybackMode::Speech};
      voiceBusy.store(true,std::memory_order_release);
      requestActive.store(true,std::memory_order_release);
      renderEntry(request);
      voiceBusy.store(false,std::memory_order_release);
      requestActive.store(false,std::memory_order_release);idleHoldConsumed=true;
    } else tight_loop_contents();
    return;
  }
  voiceBusy.store(true,std::memory_order_release);
  requestActive.store(true,std::memory_order_release);
  queue_try_remove(&requests,&request);
  if(!cancelled(request))renderEntry(request);
  if(errorControl.load(std::memory_order_relaxed))idleHoldConsumed=true;
  voiceBusy.store(false,std::memory_order_release);
  requestActive.store(false,std::memory_order_release);
}

void triggerAudio(uint8_t bank, uint8_t item, PlaybackMode mode) {
  if (mode == PlaybackMode::Speech) {
    if(voiceBusy.load(std::memory_order_acquire) || playedCode.load(std::memory_order_relaxed) ||
       !queue_is_empty(&requests))return;
    // A delay/reverb tail must not lock out the next spoken note.
    if(requestActive.load(std::memory_order_acquire))stopAudio();
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

void sayStartupPart(uint8_t part) {
  if (part <= TalkoStartup::Point) {
    triggerAudio(StartupBank, part, PlaybackMode::Speech);
  }
}

bool audioBusy() {
  return !queue_is_empty(&requests) || requestActive.load(std::memory_order_acquire);
}

bool audioFrozen(){return frozen.load(std::memory_order_relaxed);}
uint8_t nativeBuffered(){return synthBuffered.load(std::memory_order_relaxed);}
void setPanelControls(const PanelControls &controls,bool announce) {
#ifdef TALKO_DIAGNOSTICS
  const PanelControls &active = injected ? injectedControls : controls;
#else
  const PanelControls &active = controls;
#endif
  static uint16_t oldPitch=65535,oldSpeed=65535,pitchScale=256,rateScale=256;
  if(active.pitch!=oldPitch){oldPitch=active.pitch;pitchScale=Panel::scale(active.pitch);}
  if(active.speed!=oldSpeed){oldSpeed=active.speed;rateScale=Panel::scale(active.speed);}
  static uint32_t lastPanelAt=0;
  if(panelMonitor && millis()-lastPanelAt>=100){panelPending=true;lastPanelAt=millis();}
  panelSnapshot=controls;panelPending|=announce;
  performance.store((uint32_t(pitchScale)<<16)|rateScale,std::memory_order_relaxed);
  speedControl.store(active.speed,std::memory_order_relaxed);
  effectControl.store((uint32_t(active.effect)<<16)|active.fx,std::memory_order_relaxed);
  errorControl.store(active.error,std::memory_order_release);
}

// Explicit SpeakJet -> SAM mnemonic adapter; never transmit SpeakJet codes as SAM IDs.
const char *mouthCode(uint8_t code) {
  static const char *const map[] = {
    "IY", // 128 IY
    "IH", // 129 IH
    "EY", // 130 EY
    "EH", // 131 EH
    "AE", // 132 AY
    "AX", // 133 AX
    "UH", // 134 UX
    "AA", // 135 OH
    "AA", // 136 AW
    "OW", // 137 OW
    "UH", // 138 UH
    "UW", // 139 UW
    "M", // 140 MM
    "N", // 141 NE
    "N", // 142 NO
    "NX", // 143 NGE
    "NX", // 144 NGO
    "L", // 145 LE
    "L", // 146 LO
    "W", // 147 WW
    "R", // 148 RR
    "IY", // 149 IYRR
    "EH", // 150 EYRR
    "ER", // 151 AXRR
    "ER", // 152 AWRR
    "OW", // 153 OWRR
    "EY", // 154 EYIY
    "EY", // 155 OHIY
    "OY", // 156 OWIY
    "AW", // 157 OHIH
    "AY", // 158 IYEH
    "IY", // 159 EHLL
    "IY", // 160 IYUW
    "AX", // 161 AXUW
    "IH", // 162 IHWW
    "AW", // 163 AYWW
    "OW", // 164 OWWW
    "J", // 165 JH
    "V", // 166 VV
    "Z", // 167 ZZ
    "ZH", // 168 ZH
    "DH", // 169 DH
    "B", // 170 BE
    "B", // 171 BO
    "B", // 172 EB
    "B", // 173 OB
    "D", // 174 DE
    "D", // 175 DO
    "D", // 176 ED
    "D", // 177 OD
    "G", // 178 GE
    "G", // 179 GO
    "G", // 180 EG
    "G", // 181 OG
    "CH", // 182 CH
    "/H", // 183 HE
    "/H", // 184 HO
    "WH", // 185 WH
    "F", // 186 FF
    "S", // 187 SE
    "S", // 188 SO
    "SH", // 189 SH
    "TH", // 190 TH
    "T", // 191 TT
    "T", // 192 TU
    "S", // 193 TS
    "K", // 194 KE
    "K", // 195 KO
    "K", // 196 EK
    "K", // 197 OK
    "P", // 198 PE
    "P", // 199 PO
  };
  static_assert(sizeof(map) / sizeof(map[0]) == 72);
  return code >= 128 && code <= 199 ? map[code - 128] : "-";
}

void serviceConsole() {
  static char line[256];
  static size_t sent = 0;
  static size_t length = 0;
  static bool connected = false;
  Report report;
  static Report lastCompleted{};
  static bool haveLast = false, requestLast = false;
  PlaybackEvent playback;
  static bool mouthEnabled = false;
  static bool statsEnabled = false;
  static uint8_t lastMouth = 255;
  static uint8_t requestOffset = 0;
  static char command[288];
  static uint commandLength = 0;
  static bool commandOverflow = false;
  constexpr char request[] = "\x1eSAM\x1f";
  if (!Serial) {
    connected = false;
    sent = length;
    while (queue_try_remove(&reports, &report)) {
      if (report.event == Event::Done || report.event == Event::Cut) {
        lastCompleted = report; haveLast = true;
      }
    }
    while (queue_try_remove(&playbackEvents, &playback)) {}
    mouthEnabled = false;
    requestOffset = commandLength = 0;
    commandOverflow = false;
    return;
  }
  // Bound receive work so long commands cannot starve the panel.
  for (uint n = 0; n < 32 && Serial.available(); ++n) {
    const int byte = Serial.read();
    if (byte == request[requestOffset]) {
      if (++requestOffset == sizeof(request) - 1) {
        mouthEnabled = true; lastMouth = 255; requestOffset = 0;
      }
    } else requestOffset = byte == request[0] ? 1 : 0;
    if (byte == 0x1f || byte == 0x1e) {
      commandLength = 0; commandOverflow = false;
      continue;
    }
    if (byte == '\n' || byte == '\r') {
      command[commandLength] = 0;
      if (!commandOverflow) {
        unsigned bank, item;
#ifdef TALKO_DIAGNOSTICS
        unsigned tp,ts,tf,te,terr;
        if(!strcmp(command,"TEST OFF")){injected=false;panelPending=true;}
        else if(sscanf(command,"TEST %u %u %u %u %u",&tp,&ts,&tf,&te,&terr)==5 &&
                tp<=4095 && ts<=4095 && tf<=4095 && te<unsigned(Fx::Count) && terr<=1){
          injectedControls=panelSnapshot;injectedControls.pitch=tp;injectedControls.speed=ts;
          injectedControls.fx=tf;injectedControls.effect=Fx(te);injectedControls.error=terr;
          injected=true;panelPending=true;
        }else
#endif
        if (!strcmp(command, "CUT")) stopAudio();
        else if (!strcmp(command, "STATUS")) {connected = false;panelPending=true;}
        else if (!strcmp(command,"PANEL")) panelPending=true;
        else if (!strcmp(command,"MONITOR ON")) panelMonitor=true;
        else if (!strcmp(command,"MONITOR OFF")) panelMonitor=false;
        else if (!strcmp(command,"STATS ON")) statsEnabled=true;
        else if (!strcmp(command,"STATS OFF")) statsEnabled=false;
        else if (!strcmp(command, "LAST")) requestLast = true;
        else if ((sscanf(command, "PLAY %u %u", &bank, &item) == 2 ||
                  sscanf(command, "QUEUE %u %u", &bank, &item) == 2) &&
                 bank < BankCount && item >= 1 && item <= banks[bank].count) {
          triggerAudio(bank, item - 1, command[0] == 'Q' ? PlaybackMode::Queue : PlaybackMode::Speech);
        } else if (!strncmp(command, "RAW ", 4) && !audioBusy()) {
          Request raw{gateEpoch.load(std::memory_order_relaxed), uint8_t(BankCount + 1), 0,
                      PlaybackMode::Speech};
          char *at = command + 4;
          bool valid = true;
          while (*at) {
            while (*at == ' ' || *at == ',') ++at;
            if (!*at) break;
            char *end;
            long value = strtol(at, &end, 10);
            if (end == at || value < 0 || value > 255 || raw.byteCount == 64 ||
                (*end && *end != ' ' && *end != ',')) { valid = false; break; }
            raw.bytes[raw.byteCount++] = uint8_t(value);
            at = end;
          }
          if (valid && raw.byteCount) queue_try_add(&requests, &raw);
        }
      }
      commandLength = 0; commandOverflow = false;
    } else if (byte >= 32 && byte <= 126) {
      if (commandLength < sizeof(command) - 1) command[commandLength++] = char(byte);
      else commandOverflow = true;
    }
  }
  const int available = Serial.availableForWrite();
  if (available <= 0) {
    return;
  }

  if (sent == length) {
    int count;
    if (!connected) {
      count = snprintf(line, sizeof(line),
                       "Talko_SpeakJet %s build=%s engine=%s sample_hz=%u "
                       "block_samples=%u budget_us=%u prefill_samples=%u\r\n",
                       FirmwareVersion,
#ifdef TALKO_DIAGNOSTICS
                       "diagnostic",
#else
                       "normal",
#endif
                       SJ_VERSION,
                       unsigned(SJ_SAMPLE_RATE), BlockSamples, blockBudgetUs,
                       PrefillSamples);
      connected = true;
    } else if (panelPending) {
      panelPending=false;
      count=snprintf(line,sizeof(line),"PANEL mode=%s pins=%u%u pitch=%u speed=%u fx=%u effect=%u error=%u sound=%u buffered=%u frozen=%u\r\n",
        Panel::modeName(panelSnapshot.mode),unsigned(panelSnapshot.modeA),unsigned(panelSnapshot.modeB),
        unsigned(panelSnapshot.pitch),unsigned(panelSnapshot.speed),unsigned(panelSnapshot.fx),
        unsigned(panelSnapshot.effect),unsigned(panelSnapshot.error),unsigned(panelSnapshot.sound),
        unsigned(nativeBuffered()),unsigned(audioFrozen()));
    } else if (queue_try_remove(&playbackEvents, &playback)) {
      if (!playback.label) count = snprintf(line, sizeof(line), "STOP\r\n");
      else {
        const uint bank = playback.label >> 8, item = (playback.label & 255) - 1;
        const Request label{0, uint8_t(bank), uint8_t(item), PlaybackMode::Speech};
        count = snprintf(line, sizeof(line), "%s\r\n",
                         bank == FreezeBank ? "ERROR" : bank > StartupBank ? "RAW" : bankEntry(label).label);
      }
    } else if (mouthEnabled && playedCode.load(std::memory_order_relaxed) != lastMouth) {
      lastMouth = playedCode.load(std::memory_order_relaxed);
      count = snprintf(line, sizeof(line), "\x1eSAM:%s\x1f", mouthCode(lastMouth));
    } else if (requestLast && haveLast) {
      requestLast = false;
      count = formatReport(line, sizeof(line), lastCompleted);
    } else if (queue_try_remove(&reports, &report)) {
      if (report.event == Event::Done || report.event == Event::Cut) {
        lastCompleted = report; haveLast = true;
      }
      if(!statsEnabled)return;
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
