# Development

The normal release is Talko_SpeakJet **1.0** for RP2040, using a maintained
port of Andrew Davie’s [SpeakJet 0.5.5](https://github.com/andrew-davie/SpeakJet/tree/b05428e95fb1e2f3dba4fb7a62f9f89320364e49).
These notes preserve the port’s behavior and optimizations during engine updates.

## Talko-specific implementation: preserve during upstream updates

The Talko firmware version (currently **Talko_SpeakJet 1.0**) is separate
from the embedded SpeakJet engine version (**0.5.5**). This is a maintained
RP2040 port, not an unmodified copy of the desktop emulator.

| Area | Talko adaptation and reason |
|---|---|
| Synthesis arithmetic | `float` state, float math functions and `-fsingle-precision-constant` avoid the upstream double-precision synthesis cost on RP2040. |
| Oscillators | `speakjet_sine.h` supplies a quarter-wave lookup for a 4096-step cycle. Oscillator phases stay bounded during long effects and repeats. |
| Audio rate and output stage | `SJ_SAMPLE_RATE=8026`, matching the emulator's measured native chip tick. The analogue output model is advanced once per held sample with cached pole coefficients. Do not restore upstream's 48 kHz renderer accidentally. |
| Expensive calculations | Master volume gain is cached until Volume changes. Exact coefficients of stationary noise bands are cached. C8/C9 noise-filter sweeps are prepared once in `sj_new()` and survive `sj_power_on()` resets. Continuous oscillator glides use a cheap bound before the 25% leap check. Geometric sweeps cache their log ratios per segment. Recheck ALARM_7 on hardware when changing either path. |
| RAM placement | `SJ_TALKO_RP2040` places the renderer and sine table in time-critical RAM sections. Keep the library flags in `library.json`. |
| Core ownership | Core 1 owns the emulator, FIFO writes and rendering. Core 0 handles the panel, USB and PWM interrupt. Keep its panel/USB loop at about 1 kHz: an unrestricted loop contends with the renderer for shared memory and caused underruns. Encoder/gate edges remain interrupt-driven. Never access one emulator concurrently from both cores. |
| Audio buffering | 64-sample blocks; 1024-sample duty ring; 128-sample startup prefill. All modes limit rendering ahead to 192 samples (about 24 ms), keeping pitch, speed, FX and ERROR responsive. Native Queue flow control stops feeding at Buffer Half Full (>32 bytes). |
| PWM output | GPIO 1 carries PWM at 12 carrier periods per audio sample. The interrupt only consumes prepared duties; synthesis stays outside interrupts. |
| Live joining | `sj_join_pending()` revisits a phoneme's tail after appending live input between render blocks. A reusable scratch planner rebuilds the sound with the newly available context and accepts it only when the rendered past is unchanged. Late arrivals and explicit Repeat-command copies remain separate. This API is a Talko addition. |
| Stream cancellation | Epoch-based gate cuts invalidate playing and waiting requests. The pending selection queue has 16 slots; a full queue reports `DROP`. |
| Instrument identity | Seven curated banks followed by all seven Emy groups in their original order/ranges; preserve native byte order, effect labels and startup digits. Preserve their byte streams during an engine-only update. |
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
- `banks/catalogue.json`: editable selections, pronunciations and source provenance.
- `scripts/generate_banks.py`: validates the catalogue and generates the bank header.
- `src/banks.h` / `banks_generated.h`: entry helpers, compiled selections, startup numbers and Emy labels.
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


## Serial interface

Commands end with a newline. Work is bounded and nonblocking on the panel
core; playback still obeys the selected request mode.

| Command | Action |
|---|---|
| `STATUS` | Report firmware/build/engine, block size and deadline, followed by PANEL state. |
| `PANEL` | Report mode, raw switch pins, physical pots, ERROR, FIFO occupancy and freeze state. |
| `STATS ON` / `STATS OFF` | Enable/disable timing and input-buffer diagnostics. Default OFF. |
| `MONITOR ON` / `MONITOR OFF` | Enable/disable repeated physical input snapshots. Default OFF. |
| `PLAY 0 1` | Play bank 0, item 1 (HELLO), with Speech-mode busy behavior. Banks are zero-based; items are one-based. |
| `QUEUE 7 10` | Append bank 7, item 10 (OW), using live Queue behavior. |
| `RAW 141 179 137` | Play a test byte stream (NE GO OW), up to 64 decimal bytes, separated by spaces or commas. Accepted only while idle. These are SpeakJet command bytes, not text. |
| `CUT` | Cut playback and invalidate waiting selections. |
| `LAST` | Report the last completed or cut stream, including when USB was disconnected during playback. |

Console can request the optional mouth stream with `RS + SAM + US`
(`\x1eSAM\x1f`). The explicit SpeakJet adapter replies with
`RS + SAM: + mnemonic + US`; `-` is silence. It reports the current pose
on enable and follows the samples consumed by PWM, including pauses and cuts.
Effects have readable labels and a silence mouth pose. Plain serial monitors receive labels only. Diagnostic output requires an
explicit command; the Console handshake explicitly enables animation.

## Builds and measurements

`pio run` builds only the normal `pico` environment. `pio run -e diagnostic`
builds a separately labelled developer image with `TEST pitchADC speedADC fxADC
effectIndex errorFlag` and `TEST OFF`. Keep diagnostic images out of releases.
Use the normal image after diagnostic work and verify its USB product name.

Install `pyserial` for device measurements, close other serial clients, and
supply the intended board’s USB serial number:

```sh
python3 scripts/bench_serial.py --serial-id YOUR_TALKO_SERIAL --output reports/1.0
```

Add `--quick` for a shorter scan. This makes sound and changes playback; it does
not flash firmware. Results are written under ignored `reports/` by default.
Keep board, controls and audio gain settings consistent when comparing images.

Audio blocks contain 64 samples at nominal 8,026 Hz. On the tested 133 MHz board
the PWM-derived block deadline is 7,974 µs. `avg_us`/`peak_us` measure render cost;
`late_blocks` counts blocks above their deadline. Startup buffering can cover a
transient overrun. `underrun_events` and `underrun_samples` count actual gaps.
Report those separately rather than inferring gaps from peak time alone.

`scripts/record_audio.py reports/capture.wav 10` captures MG-XU through PortAudio.
Use matching Python/PortAudio architectures. An Intel PortAudio installation on
Apple Silicon needs an Intel Python, for example `arch -x86_64 /usr/bin/python3`.
Check the recorder’s ready message and input-overflow count. Raw captures and
logs belong under `reports/`, not among the source files.

## Provenance

- `55561ba`: original 0.8 source/UF2, with SpeakJet 0.4.0 at
  `67312e570ddd1e486898fe48f0b65638261d29ab`.
- `13a0fd0`: port-preservation notes before the engine update.
- `a32be9d`: SpeakJet 0.4.3 port, based on
  `ea2935018ed8ee90becb28b48fc2c83d03f683fe` (same runtime source as
  `4bba1baf69f9fd55c4a2023a88f77b87657e6361`).
- `fb4abd7`: 0.9.1 live controls, FX/ERROR, FIFO flow control and panel scheduling.
- `760d5cb`: 0.9.3, restored Emy groups and labels-only default serial.
- 1.0 uses upstream commit `b05428e95fb1e2f3dba4fb7a62f9f89320364e49`. The runtime uses native-rate PWM and two-digit version announcements. The new effect decoder and table must be updated together. The port includes deterministic seeding and uses a fixed native-rate output stage. Upstream’s selectable 48 kHz clean/AtariVox output is a desktop feature. Live Queue rebuilds from the saved start of an allophone and carries accepted cross-boundary formant timing into the next sound.

Queue follows the native MSA/FIFO model described in the
[Emy/SpeakJet reference](https://www.polaxis.be/product/speakjet-vox/).
See [bank editing](../banks/README.md), [release images](../firmware/README.md)
and [release validation](VALIDATION-1.0.md) for the current snapshot.
