# Talko 2 SpeakJet 0.8 prototype

SpeakJet firmware for the **Talko 2 (RP2040)**. It brings the Emy-style
banks of vowels, allophones and effects to Talko, along with a bank of spoken
digits. In Queue mode, closely spaced triggers can blend phonemes into a
continuous voice.

At startup, SpeakJet says “zero point eight” as the front display shows
`0`, the dot, then `8`. The rear LED flashes blue once and green eight
times. Red flashes count the major version when it is above zero.

## Install and play

1. Connect Talko with a USB data cable. Hold **BOOTSEL**, press and release
   **RESET**, then release **BOOTSEL** when the `RPI-RP2` drive appears.
2. Copy [Talko_SpeakJet-0.8.uf2](Talko_SpeakJet-0.8.uf2) to that drive.
   Talko restarts automatically.
3. Move the switch **left**, turn the encoder to bank **0**, and turn
   **SOUND** fully counterclockwise. Press **SPEAK** to hear **HELLO**.
4. Try bank **8** for spoken numbers. SOUND selects **ZERO** at the left end
   and **NINE** at the right end.

| Control | Action |
|---|---|
| Encoder turn | Select a bank. The display shows its number. |
| SOUND knob or CV | Select an entry within the current bank. |
| SPEAK button or GATE input | Trigger the selected entry. |
| SPEED, PITCH, FX and ERROR | Voice settings are fixed in 0.8. Select built-in effects with SOUND in bank 6. |

| Switch | Mode | Action |
|---|---|---|
| Left | **Speech** | A trigger plays the selection once to completion. Speech takes priority and additional triggers during playback are ignored. |
| Middle | **Gate** | A rising gate starts playback. A falling gate cuts playback and clears waiting sounds. |
| Right | **Queue** | Each rising edge adds the current SOUND selection in order. If the next phoneme arrives before the current one starts fading, the two can blend. |

For a first Queue patch, select **bank 1**, set SOUND fully counterclockwise
for **IY**, and send a short burst of gates at about **15 Hz**. Move SOUND between gates to
queue different vowels. Stop the clock to hear the queue
drain. With a sequencer, let the SOUND CV settle before each gate.

## Banks

SOUND scans each row from left to right. Banks 1–7 follow the SpeakJet code
ranges used by Emy.

| Display | Bank | Entries | SOUND order |
|---|---|---:|---|
| 0 | Words and test sentence | 6 | HELLO, READY, ROBOT, DANGER, TALKO, THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG |
| 1 | Vowels | 12 | Codes 128–139 |
| 2 | Voiced allophones | 54 | Codes 128–181 |
| 3 | Voiced stops | 12 | Codes 170–181 |
| 4 | Voiceless allophones | 18 | Codes 182–199 |
| 5 | All allophones | 72 | Codes 128–199 |
| 6 | SpeakJet effects | 55 | Codes 200–254, including robot sounds, alarms, beeps, biological sounds and DTMF |
| 7 | DTMF telephone tones | 12 | 0–9, star, pound (codes 240–251) |
| 8 | Spoken digits | 10 | ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE |

For the swept-noise test, select bank **6** and turn SOUND about **70%**
clockwise. This selects **BIO_8** (item 39, SpeakJet **C8**). Fully clockwise
selects **WOW** (SpeakJet **M2**).

## USB serial

Open PlatformIO Monitor or another serial terminal at **115200 baud** before
playing. On macOS, look for a `/dev/cu.usbmodem…` port. The connection banner
shows the firmware version.

| Serial line | Meaning |
|---|---|
| `PLAY` | The stream starts with this bank, item, and label. Items are numbered from 1. Native entries include `code`. Effects also include the upstream mnemonic as `sj`. |
| `QUEUE` | A selection entered the current stream. `pending` counts selections waiting to enter the synth. `buffer_bytes` counts bytes in the synth buffer, including the active command. |
| `DROP` | A new trigger arrived while all 16 pending slots were occupied. |
| `RESULT` | Timing and underruns for the whole stream. `status=done` marks completion and `status=cut` marks a gate cutoff. |

The RP2040 renders 64 samples per block at **8,026 Hz**, giving each block
a **7,974 µs** playback budget. `avg_us` and `peak_us` report the mean and
longest time to prepare a block. Playback starts with two blocks buffered,
so a `late_blocks` count can be greater than zero even when audio stays
continuous. `underrun_events` counts gaps and `underrun_samples` counts the
missing samples in those gaps.

## Build from source

Open this folder in PlatformIO. Close the serial monitor before uploading.

| Command | Action |
|---|---|
| `pio run` | Build `.pio/build/pico/firmware.uf2`. |
| `pio run -t upload` | Upload through the running firmware's USB serial port. |
| `pio device monitor -b 115200` | Open the serial monitor. |

Use the BOOTSEL copy steps above if the running firmware does not expose
a serial port.

## Source and license

The emulator is adapted from Andrew Davie's
[SpeakJet 0.4.0](https://github.com/andrew-davie/SpeakJet/tree/67312e570ddd1e486898fe48f0b65638261d29ab).
The library source carries its [MIT license and upstream notices](lib/SpeakJet/LICENSE).
The bank 0 phrases were generated with upstream `text2speakjet` using
`--no-phrasealator`. The bank ranges, spoken digits and effect labels come
from Polaxis Emy's SpeakJet firmware.

## Verification

SpeakJet 0.7 builds for RP2040. Desktop checks exercised all 251 bank
selections, the three switch modes and live vowel joining. Appending
**OW** during **IY** produced the same samples as buffering the pair upfront.
Initial Talko testing of 0.7 was reported working. A serial timing capture
for a Queue burst is still pending.

| Talko hardware check | Build | Peak per block | Late blocks | Underrun samples |
|---|---|---:|---:|---:|
| BIO_8 / C8 | 0.6 | 6,142 µs | 0 | 0 |
| ZERO | 0.6 | 7,724 µs | 0 | 0 |
| NINE | 0.6 | 5,574 µs | 0 | 0 |

ZERO and NINE were both recognizable by ear. The C8 retest used the noise
sweep optimization introduced in 0.5.


## Talko-specific implementation: preserve during upstream updates

The Talko firmware version (currently **Talko_SpeakJet 0.8**) is separate
from the embedded SpeakJet engine version (**0.4.0**). This is a maintained
RP2040 port, not an unmodified copy of the desktop emulator.

| Area | Talko adaptation and reason |
|---|---|
| Synthesis arithmetic | `float` state, float math functions and `-fsingle-precision-constant` avoid the upstream double-precision synthesis cost on RP2040. |
| Oscillators | `speakjet_sine.h` supplies a quarter-wave lookup for a 4096-step cycle. Oscillator phases stay bounded during long effects and repeats. |
| Audio rate and output stage | `SJ_SAMPLE_RATE=8026`, matching the emulator's measured native chip tick. The analogue output model is advanced once per held sample with cached pole coefficients. Do not restore upstream's 48 kHz renderer accidentally. |
| Expensive calculations | Master volume gain is cached until Volume changes. C8/C9 noise-filter sweeps are prepared once in `sj_new()` and survive `sj_power_on()` resets. |
| RAM placement | `SJ_TALKO_RP2040` places the renderer and sine table in time-critical RAM sections. Keep the library flags in `library.json`. |
| Core ownership | Core 1 owns the emulator, FIFO writes and rendering. Core 0 handles the panel, USB and PWM interrupt. Never access one emulator concurrently from both cores. |
| Audio buffering | 64-sample blocks; 1024-sample duty ring; 128-sample startup prefill. Queue mode limits rendering ahead to 256 samples so a new gate can still influence an upcoming join. |
| PWM output | GPIO 1 carries PWM at 12 carrier periods per audio sample. The interrupt only consumes prepared duties; synthesis stays outside interrupts. |
| Live joining | `sj_join_pending()` revisits a phoneme's tail after appending live input between render blocks. Join only while the affected audio remains unrendered. This API is a Talko addition. |
| Stream cancellation | Epoch-based gate cuts invalidate playing and waiting requests. The pending selection queue has 16 slots; a full queue reports `DROP`. |
| Instrument identity | Nine Emy-style banks, the existing allophone/effect byte order, Emy effect labels and spoken digits, and the six bank-0 phrases. Preserve these byte streams during an engine-only update. |
| Panel behavior | Settled quadrature inputs on GPIO 3/4, SOUND on ADC GPIO 26, inverted GATE/SPEAK on GPIO 2, mode switch on GPIO 14/15, display GPIO 5–12 at brightness 100. Speech/Gate/Queue retain their distinct retrigger and cutoff behavior. |
| Startup | `talko_startup.h` and `ws2812.pio.h`: spoken version synchronized with the front display, rear RGB LED on GPIO 16, and bank-page decimal point off. |
| Diagnostics | Nonblocking USB writes and per-stream mean/peak render times, late blocks and actual underrun counts. A late block alone does not prove an audible gap. |

### Source layout

- `src/main.cpp`: encoder, switch, SOUND selection, display and trigger handling.
- `src/audio.cpp` / `audio.h`: core ownership, ring buffer, PWM, stream modes,
  cancellation, startup speech, serial reports and Talko version.
- `src/banks.h`: immutable selections, phrase bytes, digit dictionary and Emy labels.
- `src/talko_startup.h` / `ws2812.pio.h`: display and rear LED startup sequence.
- `lib/SpeakJet/src/speakjet.c`: adapted engine and Talko's live-join hook.
- `lib/SpeakJet/src/speakjet_tables.c`: generated upstream sound parameters,
  included by `speakjet.c`; do not compile it as a separate translation unit.
- `lib/SpeakJet/src/speakjet.h`: engine API, engine version and native sample rate.
- `lib/SpeakJet/src/speakjet_sine.h`: Talko oscillator lookup table.

### Updating the upstream emulator

1. Commit the working firmware and versioned UF2 first. Preserve earlier UF2s.
2. Record the old and new upstream commits and read their code, table and API
   differences together. Use a fixed commit, not a moving `main` dependency.
3. Port the engine rules and table-layout changes together. Reapply each Talko
   adaptation above; copying upstream `speakjet.c` and `.h` wholesale removes
   the native renderer, optimizations and live-join API.
4. Keep stored phrases and live Queue input on the same transition rules.
   Incoming context can only change the unrendered future: late gates cannot
   reproduce an anticipation that should already have sounded.
5. Test all bank selections, repeated sounds, pauses, Speed/Fast/Slow, vowel
   joins, nasal/stop closures, late arrivals, FIFO capacity and gate cuts.
   Keep the original bank byte streams fixed for old/new comparisons.
6. Build for `pico` (RP2040). Start with 64-sample blocks. Check peak time
   against the actual PWM-derived block deadline, and check underruns with
   serial open and closed. Larger buffers can hide lateness and increase
   interaction latency; they are not a substitute for measuring CPU cost.
7. Identify the connected USB target before uploading. Reconnect and verify
   the Talko and engine versions after reboot. Capture the same phrases and
   effects through the same audio interface/settings for before/after checks.
8. Update the Talko version in `audio.h`, USB identity, startup, README and UF2
   filename together; update all `SJ_VERSION*` macros and `library.json` for
   the engine. Record measured results separately from desktop checks and
   historical results. Commit the candidate with its exact source provenance.

### Current limitations

SPEED, PITCH, FX and ERROR are not implemented as panel controls in 0.8.
Serial currently sends diagnostic `PLAY`/`QUEUE`/`RESULT` lines rather than
standalone spoken labels or playback articulation events; Console lip sync
is not supported by this baseline. Audio is native-rate PWM, so it does not
reproduce all high-frequency images of upstream's 48 kHz desktop output.
The historical measurements above do not establish 0.8's worst-case timing.
