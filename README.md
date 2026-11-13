# Talko_SpeakJet 0.9.1

SpeakJet firmware for the **Talko 2 (RP2040)**. It brings the Emy-style
banks of vowels, allophones and effects to Talko, along with a bank of spoken
digits. In Queue mode, closely spaced triggers can blend phonemes into a
continuous voice.

At startup, SpeakJet says “zero point nine point one” as the front display shows
`0`, the dot, then `9`. The rear LED flashes blue once and green nine
times. A second dot/blue separator and one green flash announce the patch `1`.
Red flashes count the major version when it is above zero.

## Install and play

1. Connect Talko with a USB data cable. Hold **BOOTSEL**, press and release
   **RESET**, then release **BOOTSEL** when the `RPI-RP2` drive appears.
2. Copy [Talko_SpeakJet-0.9.1.uf2](Talko_SpeakJet-0.9.1.uf2) to that drive.
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
| PITCH | Live vocal pitch: lower counterclockwise, higher clockwise; noon is the native default. Unvoiced hisses and fixed tones behave like the chip and have no vocal pitch. |
| SPEED | Live allophone rate, approximately one quarter to four times normal; noon is normal. It changes timing independently of pitch. |
| FX | Amount of the selected output effect. Fully counterclockwise is dry, with a small dead zone. Default effect: Delay. |
| Encoder press | Switch between bank and FX pages. The dot is steady on the FX page and off on the bank page. Turn the encoder to select the bank/effect on the current page. |
| ERROR | Hold to repeat a short recent sound loop. PITCH changes loop playback rate; SPEED changes loop length; FX remains live. Release to resume the paused speech, or finish an idle hold. |

| Switch | Mode | Action |
|---|---|---|
| Left | **Speech** | A trigger plays the selection once to completion. Speech takes priority and additional triggers during playback are ignored. |
| Middle | **Gate** | A rising gate starts playback. A falling gate cuts playback and clears waiting sounds. |
| Right | **Queue** | Each rising edge feeds the current selection into the same native 64-byte SpeakJet MSA input FIFO while synthesis continues. Adjacent allophones use the engine's context-dependent transitions. Lower SPEED and clock phonemes into the buffer to hear the blending. |

For a first Queue patch, select **bank 1**, set SOUND fully counterclockwise
for **IY**, turn **FX fully left** and **SPEED left**, then send gates at about **10–15 Hz**. Move SOUND between gates to
queue different vowels. Stop the clock to hear the queue
drain. The rear LED turns green when the native buffer is more than half full,
as in Emy. With a sequencer, let the SOUND CV settle before each gate.

The switch briefly shows `S`, `G` or `q` when changed, then returns to the
selected bank/FX page. In Gate mode, a falling gate cuts the stream and waiting
selections. Speech mode lets the utterance complete and ignores retriggers
while busy. Queue lets incoming allophones reach one ongoing synthesis stream;
it does not restart the emulator for every allophone.

FX choices are Off, Ring modulation, Reverb, Delay, Bit crush, Drive, Low-pass
and High-pass. Start dry, choose an effect on the FX page, then raise FX. Delay
and reverb can continue for up to two seconds after the voice. The ERROR display
uses the stock irregular brightness flicker while preserving the FX-page dot.

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
playing. On macOS, look for a `/dev/cu.usbmodem…` port. The USB device name is **Talko_SpeakJet 0.9.1** (manufacturer Polaxis).
The connection banner shows firmware/engine versions and `build=normal`.
Normal operation sends readable speech labels and STOP; timing and repeated
panel snapshots are enabled only by explicit serial commands.

| Serial line | Meaning |
|---|---|
| Plain label / `STOP` | The selection reaches playback / the stream finishes or is cut. Repeated Queue selections repeat their labels. |
| `PANEL` | Current mode, switch pins and physical ADC/button readings; sent on panel context changes or on request. |
| `PLAY` | The stream starts with this bank, item, and label. Items are numbered from 1. Native entries include `code`. Effects also include the upstream mnemonic as `sj`. |
| `QUEUE` | A selection entered the current stream. `pending` counts selections waiting to enter the synth. `buffer_bytes` counts bytes in the synth buffer, including the active command. |
| `DROP` | A new trigger arrived while all 16 pending slots were occupied. |
| `RESULT` | Timing and underruns for the whole stream. `status=done` marks completion and `status=cut` marks a gate cutoff. |

Use `STATS ON` to enable PLAY/QUEUE/RESULT diagnostics and `STATS OFF` to
disable them (the default). `MONITOR ON` enables ten panel snapshots per second;
`MONITOR OFF` returns to normal.

The RP2040 renders 64 samples per block at **8,026 Hz**, giving each block
a **7,974 µs** playback budget on the tested clock. The firmware computes
the deadline from the actual system clock, PWM wrap and carrier count. `avg_us` and `peak_us` report the mean and
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
[SpeakJet 0.4.3](https://github.com/andrew-davie/SpeakJet/tree/ea2935018ed8ee90becb28b48fc2c83d03f683fe).
The library source carries its [MIT license and upstream notices](lib/SpeakJet/LICENSE).
The bank 0 phrases were generated with upstream `text2speakjet` using
`--no-phrasealator`. The bank ranges, spoken digits and effect labels come
from Polaxis Emy's SpeakJet firmware.

## Verification

Talko_SpeakJet 0.9.1 builds for RP2040. The normal image has been flashed,
readback-verified and left installed. The user confirmed PITCH, FX, ERROR and
all switch modes working on the instrument. Host tests exercise the actual
panel loop with simulated GPIO/ADC input, native slow phoneme joining, pitch
independent of duration, FX dry/switch behavior and ERROR loop playback.

The final normal-image scan completed **262 streams**, including every bank
selection, transitions, live Queue and serial-disconnected playback, with
**zero underruns**. Two blocks exceeded the 7,974 µs deadline; the maximum was
8,206 µs and buffering covered both. The separate diagnostic build measured
pitch, rate, all eight FX choices, native FIFO filling and ERROR with zero late
blocks or underruns. A 1 ms panel/USB update interval prevents the CPU contention
found in the first control candidate; gate and encoder interrupts still capture
edges between updates.

See [the 0.9.1 validation record](validation/0.9.1/README.md) for exact images,
measurements, the earlier failing tests and limits. Run
`SANITIZE=1 sh tests/run_host.sh` for the full host suite with
address/undefined-behavior checks.

The [0.9 validation archive](validation/0.9/README.md) documents the earlier
engine-only prototype; it did not validate physical panel controls.

### Historical checks

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

The Talko firmware version (currently **Talko_SpeakJet 0.9.1**) is separate
from the embedded SpeakJet engine version (**0.4.3**). This is a maintained
RP2040 port, not an unmodified copy of the desktop emulator.

| Area | Talko adaptation and reason |
|---|---|
| Synthesis arithmetic | `float` state, float math functions and `-fsingle-precision-constant` avoid the upstream double-precision synthesis cost on RP2040. |
| Oscillators | `speakjet_sine.h` supplies a quarter-wave lookup for a 4096-step cycle. Oscillator phases stay bounded during long effects and repeats. |
| Audio rate and output stage | `SJ_SAMPLE_RATE=8026`, matching the emulator's measured native chip tick. The analogue output model is advanced once per held sample with cached pole coefficients. Do not restore upstream's 48 kHz renderer accidentally. |
| Expensive calculations | Master volume gain is cached until Volume changes. Exact coefficients of stationary noise bands are cached. C8/C9 noise-filter sweeps are prepared once in `sj_new()` and survive `sj_power_on()` resets. |
| RAM placement | `SJ_TALKO_RP2040` places the renderer and sine table in time-critical RAM sections. Keep the library flags in `library.json`. |
| Core ownership | Core 1 owns the emulator, FIFO writes and rendering. Core 0 handles the panel, USB and PWM interrupt. Keep its panel/USB loop at about 1 kHz: an unrestricted loop contends with the renderer for shared memory and caused underruns. Encoder/gate edges remain interrupt-driven. Never access one emulator concurrently from both cores. |
| Audio buffering | 64-sample blocks; 1024-sample duty ring; 128-sample startup prefill. All modes limit rendering ahead to 192 samples (about 24 ms), keeping pitch, speed, FX and ERROR responsive. Native Queue flow control stops feeding at Buffer Half Full (>32 bytes). |
| PWM output | GPIO 1 carries PWM at 12 carrier periods per audio sample. The interrupt only consumes prepared duties; synthesis stays outside interrupts. |
| Live joining | `sj_join_pending()` revisits a phoneme's tail after appending live input between render blocks. A reusable scratch planner rebuilds the sound with the newly available context and accepts it only when the rendered past is unchanged. Late arrivals and explicit Repeat-command copies remain separate. This API is a Talko addition. |
| Stream cancellation | Epoch-based gate cuts invalidate playing and waiting requests. The pending selection queue has 16 slots; a full queue reports `DROP`. |
| Instrument identity | Nine Emy-style banks, the existing allophone/effect byte order, Emy effect labels and spoken digits, and the six bank-0 phrases. Preserve these byte streams during an engine-only update. |
| Panel behavior | Settled quadrature inputs on GPIO 3/4; SOUND/FX/SPEED/PITCH on ADC GPIO 26/27/28/29; ERROR on GPIO 0; encoder press on 13; inverted GATE/SPEAK on 2; switch 14/15; display 5–12 at brightness 100. Speech/Gate/Queue use distinct retrigger/cutoff policies. |
| Startup | `talko_startup.h` and `ws2812.pio.h`: spoken version synchronized with the front display, rear RGB LED on GPIO 16, and bank-page decimal point off. |
| Live performance | `sj_set_performance()` applies Q8 pitch/rate ratios. Rate advances allophone/frame timing; oscillator/output time remains native-rate. Preserve native command streams and transition planning. The interpolation reciprocal is cached, constant hot-loop divisions use reciprocals, and the phase wrap uses a fast bounded path. |
| ERROR / FX | Core 1 runs fixed-point output FX and a recent-PCM freeze with phoneme tags. Hold pauses engine advancement while the ring continues; controls remain live. Dry FX invalidates old tails. |
| Playback metadata | `sj_render_tagged()` carries the held sample's allophone and command-byte offset through the audio ring. Labels and optional mouth events follow playback, including Queue and cuts. The explicit adapter sends SAM mnemonics, never SpeakJet numeric IDs. |
| Diagnostics | Nonblocking USB writes and per-stream mean/peak render times, late blocks and actual underrun counts. A late block alone does not prove an audible gap. |

### Source layout

- `src/main.cpp`: all panel inputs, debounced mode switch, bank/FX pages, ERROR flicker and display.
- `src/panel_logic.h`: testable switch mapping, pot ratios, dry zone and selection bounds.
- `src/dsp.h` / `dsp.cpp`: fixed-point FX and ERROR history loop, owned by core 1.
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

PITCH affects voiced allophones and applicable pitch-driven effects; unvoiced
sounds and fixed-frequency effects retain their native behavior. At very low
pitch the voice becomes clicks, like the real chip. The Console adapter uses
representative poses for glides rather than reconstructing their full internal
articulation. Native-rate PWM does not reproduce every high-frequency image
of upstream's 48 kHz desktop output. Zero measured underruns do not establish
that every possible patch will meet its deadline.


## Developer serial commands (0.9.1)

Commands end with a newline. Work is bounded and nonblocking on the panel
core; playback still obeys the selected request mode.

| Command | Action |
|---|---|
| `STATUS` | Report firmware/build/engine, block size and deadline, followed by PANEL state. |
| `PANEL` | Report mode, raw switch pins, physical pots, ERROR, FIFO occupancy and freeze state. |
| `STATS ON` / `STATS OFF` | Enable/disable timing and input-buffer diagnostics. Default OFF. |
| `MONITOR ON` / `MONITOR OFF` | Enable/disable repeated physical input snapshots. Default OFF. |
| `PLAY 0 1` | Play bank 0, item 1 (HELLO), with Speech-mode busy behavior. Banks are zero-based; items are one-based. |
| `QUEUE 1 10` | Append bank 1, item 10 (OW), using live Queue behavior. |
| `RAW 141 179 137` | Play a test byte stream (NE GO OW), up to 64 decimal bytes, separated by spaces or commas. Accepted only while idle. These are SpeakJet command bytes, not text. |
| `CUT` | Cut playback and invalidate waiting selections. |
| `LAST` | Report the last completed or cut stream, including when USB was disconnected during playback. |

Console can request the optional mouth stream with `RS + SAM + US`
(`\x1eSAM\x1f`). The explicit SpeakJet adapter replies with
`RS + SAM: + mnemonic + US`; `-` is silence. It reports the current pose
on enable and follows the samples consumed by PWM, including pauses and cuts.
Effects have readable labels and a silence mouth pose. Normal serial monitors
receive readable labels and diagnostics without mouth frames.

For a repeatable device scan, install `pyserial`, close other serial clients,
identify the Talko's USB serial number, and run:

```sh
python3 scripts/bench_serial.py --serial-id YOUR_TALKO_SERIAL --output validation/0.9.1
```

Add `--quick` for selected words/effects and the transition, Queue, cutoff,
Console and serial-closed checks. This script makes sound and changes playback;
it does not flash firmware. Keep the identified board and audio gain settings
consistent when comparing builds.

`scripts/record_audio.py OUTPUT.wav SECONDS` records only MG-XU through
PortAudio with conversion to stereo 16-bit PCM at the interface's current
rate. Use a Python and PortAudio library of the same architecture. On this
Mac the installed PortAudio is Intel, so the capture command is
`arch -x86_64 /usr/bin/python3 scripts/record_audio.py OUTPUT.wav SECONDS`.
Check the recorder's ready message before starting playback and its overflow
count afterward.

## Version provenance

- `55561ba`: original Talko_SpeakJet 0.8 sources and exact rollback UF2,
  using SpeakJet 0.4.0 (`67312e570ddd1e486898fe48f0b65638261d29ab`).
- `13a0fd0`: documented the baseline adaptations before the engine update.
- Talko_SpeakJet 0.9: engine code/tables from upstream
  `ea2935018ed8ee90becb28b48fc2c83d03f683fe` (0.4.3; same runtime code as
  `4bba1baf69f9fd55c4a2023a88f77b87657e6361`), with the maintained native-rate
  RP2040 port, shared live transition planner, exact stationary-noise cache,
  playback metadata, serial diagnostics and bounded encoder clicks.


## Normal and diagnostic builds

`pio run` builds only the normal `pico` environment. It contains no test-input
override command and performs no automatic benchmark. The release UF2 and USB
identity are **Talko_SpeakJet 0.9.1**; its banner says `build=normal`.

`pio run -e diagnostic` is a separately labelled developer build containing
`TEST pitchADC speedADC fxADC effectIndex errorFlag` and `TEST OFF` for controlled
measurements. It is not the release image. Physical input monitoring, STATS and
normal playback commands are also available in the normal build. Always leave
the normal image installed after diagnostic testing and verify the USB product
and normal banner after reboot.

Queue behavior follows the [Emy/SpeakJet manual](https://www.polaxis.be/product/speakjet-vox/)
and the supplied SpeakJet user manual's native MSA input buffer and transition
model. The panel's input FIFO policy selects when allophones reach that engine;
it does not concatenate separately rendered audio clips.

0.9.1 retains the 0.4.3 engine/table provenance above and implements the previously
missing PITCH, FX and ERROR controls, and adds switch debounce and visible mode
feedback. Flashing 0.8 does not restore PITCH, FX or ERROR: the earlier panel
loop did not read those inputs. Its basic switch-mode logic was already present. Keep 0.8 and 0.9 UF2s for rollback. A physical
control check is required for future releases in addition to serial rendering
and underrun measurements.
