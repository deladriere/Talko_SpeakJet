# Talko_SpeakJet 0.9 validation — 13 November 2026

The exact repository build was uploaded to the identified Polaxis Talko,
readback-verified with picotool, and left installed as **Talko_SpeakJet 0.9**.
Its USB product name and serial banner report that version and engine 0.4.3.
The front display/rear LED version code and spoken version use the same minor
version constant. Their physical appearance and human listening assessment
remain manual checks.

## Provenance and build

- Baseline: `55561ba`, original Talko_SpeakJet 0.8 and its rollback UF2.
- Preservation documentation: `13a0fd0`, committed before changing the engine.
- Upstream: [0.4.3 at ea29350](https://github.com/andrew-davie/SpeakJet/tree/ea2935018ed8ee90becb28b48fc2c83d03f683fe).
- Target: RP2040 B2, 133 MHz, 2 MB flash; PlatformIO `pico` environment.
- Installed platform: 1.15.0+sha.4d1ad09; framework-arduinopico
  1.40700.0+sha.b8864711; toolchain package 5.140200.250530.
- Release: [Talko_SpeakJet-0.9.uf2](../../Talko_SpeakJet-0.9.uf2).
- SHA-256: `2d67d67b41a642fb999c616cf1af18baf1d294c6aa4953f3842ffe0041a3eb6b`.
- Static RAM: 31,696 bytes; linked flash: 151,704 bytes. The engine and its
  reusable live-planning scratch chip allocate another 28,944 bytes once at
  startup, plus queue/allocator storage. There is no per-block heap allocation.

`platformio.ini` retains the existing platform Git URL. Toolchain upgrades
therefore need fresh builds and timing checks; the vendored engine source is
pinned by provenance rather than downloaded at build time. [Build log](build.log), [source hashes](source-sha256.txt).

## Host checks

`SANITIZE=1 sh tests/run_host.sh` passes with address and undefined-behavior
sanitizers. The engine also compiles with `-Wdouble-promotion -Werror`.

- All 251 bank selections terminate with finite, bounded PCM and empty FIFOs.
- Tagged and ordinary rendering produce identical PCM.
- 24 Speed/modifier streams and FIFO overflow/reset checks pass.
- 6,156 pair/arrival cases: 5,757 joins accepted and sample-identical to
  prebuffered playback; 399 arrivals safely remain separate. This includes all
  5,184 two-allophone pairs after 64 samples and a subset at later arrivals.
- A gate arriving after release begins leaves the rendered plan untouched.
- An explicit Repeat command's final copy is excluded from live replanning;
  its regression completes with finite audio and an unchanged playing plan.
- Power-on resets preserve the scratch planner and immutable sweep tables.
- For all 127 sound codes, cached coefficients and forced per-sample
  recomputation produce identical PCM, including different caller block sizes.
- The upstream library unit tests pass against the adapted engine.

The tests exercise causal joins: upstream rules cannot retroactively alter an
onset or anticipation that has already been rendered. This is deliberately
different from assuming every late gate can equal a prebuffered phrase.

## Device checks

Audio uses 64-sample blocks at nominal 8,026 Hz; the PWM-derived deadline is
7,974 microseconds on this board. Prefill is 128 samples; Queue renders at most
256 samples ahead. The complete bank/serial scan was repeated after the final explicit-Repeat
guard. The following are final-image results:

| Test | Mean/block µs | Peak/block µs | Late blocks | Underrun samples |
|---|---:|---:|---:|---:|
| C8 / BIO_8 | 6103 | 7154 | 0 | 0 |
| ZERO | 6291 | 8591 | 3 | 0 |
| NINE | 6669 | 8602 | 6 | 0 |
| 12 alternating live vowels, 25 Hz | 6076 | 6707 | 0 | 0 |
| Slowed speech, serial disconnected | 6951 | 9264 | 19 | 0 |

All 262 completed streams reported zero underruns. They include every bank
selection, eight explicit transition/modifier byte streams, live vowels,
Console playback, and serial disconnected during speech. Each bank result was
checked against the requested bank/item. A cutoff also produced STOP and a
`status=cut` report. All 12 repeated Queue selections emitted their labels.
The optional SAM handshake reported silence at rest and phoneme poses during
HELLO; ordinary serial commands continued working after the handshake.

There were 106 late blocks across the completed streams.
The highest peak was 9264 µs; the highest per-stream mean was
6991 µs. Buffering covered the transient peaks
in these tests. This is not proof that every patch is underrun-free.

[Raw serial log](all-banks-serial.log), [per-stream results](all-banks-results.json),
[summary](summary.json). The scan is repeatable with `scripts/bench_serial.py`;
it requires the intended device's explicit USB serial number and makes sound.

## MG-XU recording

Both baseline and final-image startup were captured through MG-XU at its
44,100 Hz stereo setting, converted by CoreAudio/PortAudio to 16-bit PCM.
Both captures reported zero input-overflow events and zero full-scale digital
samples. No audio-interface gain changes were made by the test scripts.

- [Short comparison: 0.8 then 0.9](audio/startup-comparison-0.8-then-0.9.wav).
- [0.8 startup](audio/startup-0.8.wav): ZERO POINT EIGHT.
- [0.9 startup](audio/startup-0.9.wav): ZERO POINT NINE.
- Full captures retain the pre-start recording and reset/flash transients:
  [0.8](audio/baseline-0.8-startup.wav), [0.9](audio/candidate-0.9-startup.wav).
- [Capture metrics](audio-metrics.json) and capture/serial logs accompany them.

The short clips exclude the earlier reset/flash transients and add no gain
normalization. The last spoken digit differs, so these are startup examples,
not identical complete phrases for a spectral-fidelity score. The common ZERO
and POINT entries each render four blocks longer in 0.9 (about 31.9 ms), with
the original byte sequences preserved.

## Remaining manual checks

No physical SpeakJet/AtariVox reference recording was available for a direct
spectral or pronunciation comparison. These recordings establish actual Talko
output and digital capture levels, not improved subjective sound quality.
First encoder click, fast turns, reversals at both bank limits, physical gate
edges and live pot/CV movement still need hands-on assessment. Serial tests
exercise playback requests and cuts, not the complete physical input path.
SPEED, PITCH, FX and ERROR remain unimplemented panel controls in this prototype.
