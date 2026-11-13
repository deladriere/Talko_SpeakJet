# Talko_SpeakJet 0.9.1 validation — 13 November 2026

The release is the **normal RP2040 build**, flashed with picotool verification
and left installed on Talko serial `E6647C74032B9C24`. USB identifies it as
`Talko_SpeakJet 0.9.1`; the serial banner reports `build=normal engine=0.4.3`.
The normal UF2 contains no `TEST` input override. Timing and repeated panel
monitoring default off, and were disabled after these tests.

## Image and source

- Release: [Talko_SpeakJet-0.9.1.uf2](../../Talko_SpeakJet-0.9.1.uf2).
- SHA-256: `44364c7b07c710a045e2c4a7b28e74e40de9c18c1bd95eced292e370a374c3be`.
- Parent: `a32be9d` (Talko_SpeakJet 0.9); baseline 0.8 is `55561ba`.
- Engine/table provenance remains upstream SpeakJet 0.4.3 at
  `ea2935018ed8ee90becb28b48fc2c83d03f683fe`, with the Talko adaptations
  documented in the [developer README](../../README.md).
- RP2040 B2, 133 MHz, 2 MB flash. Platform 1.15.0+sha.4d1ad09;
  framework-arduinopico 1.40700.0+sha.b8864711; toolchain 5.140200.250530.
- Static RAM: 44,348 bytes; linked flash: 167,688 bytes. The chip and its
  reusable live planner also allocate storage at startup; no per-block heap
  allocation was added.

[Normal build](normal-build.log), [diagnostic build](diagnostic-build.log),
[source hashes](source-sha256.txt), [flash verification](normal-flash-verification.log),
[normal banner/smoke](normal-image.log). The separately labelled diagnostic
image's hash is recorded in [diagnostic-image-sha256.txt](diagnostic-image-sha256.txt);
that image is not distributed as the release.

## Controls and native Queue

The user tested an earlier 0.9.1 candidate on the physical instrument and
confirmed PITCH, FX, ERROR and the three switch positions working. The final
changes add two echo-tail fixes and reduce CPU contention; the host panel test
was rerun after those changes.

- PITCH and SPEED are independent live Q8 ratios, with native defaults at noon.
- FX has a dry dead zone, eight choices and a steady FX-page dot.
- ERROR repeats recent PCM with articulation tags, pauses the synth, and keeps
  pitch, loop length and FX live. Releasing resumes synthesis.
- Speech ignores retriggers during the voice; Gate cuts on a falling input;
  Queue adds byte streams to one ongoing native MSA engine.
- At low speed, native FIFO occupancy reached **33 bytes**, with no dropped
  requests in the test burst. Feeding pauses at Buffer Half Full (>32 bytes).
  Accepted early live context uses the same allophone transition planner as
  prebuffered phrases; context arriving too late cannot rewrite the past.
- A queued vowel arriving during a delay tail now resumes native rendering.
  Speech can start its next utterance once the voice ends, even if FX is still
  ringing. [Tail regressions](tail-results.json), [serial log](tail-regression.log).

The primary references were the supplied `speakjet-usermanual.pdf` (July 2004,
revision 1.0: FIFO p.5, MSA transitions p.12, controls p.15), the Emy
`phonSynth()` implementation, and the [Emy manual](https://www.polaxis.be/product/speakjet-vox/).
The supplied `7300-DBSPEAKJET.pdf` describes PhraseALator rather than the chip's
complete serial/MSA behavior. Reference PDFs were not copied into this repository.

## Host checks

[Sanitized host suite](host-tests.log) and [final panel rerun](panel-sanitized.log)
passed with address/undefined-behavior sanitizers. The engine compiles with
`-Wdouble-promotion -Werror`.

- All 251 bank selections terminate, with finite bounded samples and FIFO bounds.
- Tagged/plain rendering is sample-identical; 24 Speed/modifier streams pass.
- 6,156 live pair/arrival cases: 5,757 accepted joins equal prebuffered PCM,
  while 399 cases remain safely separate. Late arrival, explicit Repeat,
  reset and exact noise-coefficient cache regressions pass.
- Live pitch keeps duration fixed; quarter/four-times speed, slow native joins
  and half-full flow control pass.
- Every FX is dry at zero; effect switching clears old tails. ERROR loop/rate
  behavior, pot scales and switch decoding pass.
- The test includes the actual `main.cpp` panel loop with GPIO/ADC stubs:
  all switch positions, Gate cuts, Queue selections, every ADC, FX page/dot,
  ERROR and bank-limit reversal pass.

Constant hot-loop divisions were changed to reciprocal multiplies. A separate
comparison against the intermediate 0.9.1 engine covered all 251 entries and
394,176 samples: all durations identical, peak difference 0.000236409 and RMS
difference 0.00000754349 in normalized PCM. This is a small arithmetic change,
not a claim of bit-identical audio. [Comparison](timing-arithmetic-comparison.log),
[reference source hash](timing-reference-sha256.txt).

## RP2040 timing

64 samples at nominal 8,026 Hz have a PWM-derived deadline of **7,974 µs**.
Startup prefill is 128 samples, with at most 192 rendered ahead in every mode.
A peak above this deadline means the renderer temporarily consumes its buffer
reserve. Underrun counters establish whether playback actually ran out.

An intermediate candidate passed controlled tests but had underruns with the real
pot settings: the unrestricted core-0 panel/USB loop contended with the renderer.
The final loop yields for 1 ms between updates; encoder/gate interrupts still
capture edges. The fixed interval and arithmetic changes avoid those gaps
without extending the audio look-ahead.

| Test | Streams | Peak µs | Late blocks | Underrun samples |
|---|---:|---:|---:|---:|
| Intermediate normal, physical pots | 18 | 9707 | 587 | 1124 |
| Final normal quick, physical pots | 18 | 7578 | 0 | 0 |
| Final normal: full bank/serial scan | 262 | 8206 | 2 | 0 |
| Final diagnostic: pitch/rate/all FX/ERROR | 16 | 7825 | 0 | 0 |
| Final diagnostic: two echo-tail cases | 2 | 6184 | 0 | 0 |


The full normal-image scan exercises all 251 entries, eight raw transition/
modifier streams, 12 alternating live vowels at 25 Hz, a cutoff, Console mouth
frames, and playback with serial disconnected. Every PLAY result is checked
against its requested bank/item. All 12 Queue labels arrived. Before/final quick
scans use physical knob values, not injected input overrides.

[Final full scan](normal-all-banks/all-banks-results.json),
[full serial log](normal-all-banks/all-banks-serial.log),
[final quick scan](normal-quick/quick-results.json),
[controlled measurements](controls-results.json), [control log](controls-serial.log),
[earlier failing scan](before-panel-interval/quick-results.json), [summary](summary.json).
The eight-FX diagnostic test uses the full pangram at maximum FX amount for
each effect. It also checks low/noon/high pitch, slow/noon/fast rate, native
FIFO filling, and ERROR hold/reshape/resume. All are measured on the final
source with only the diagnostic flag/USB label differing from the normal build.

These measurements cover the listed settings; they do not establish that every
possible patch meets every deadline. First-click and very fast physical encoder
motion remain hands-on checks; the simulated panel tests verify the decoder.

## MG-XU audio

The final diagnostic-source build rendered repeated IY (`RAW 26 16 128`) with
pitch ADC set to 0, 2048 and 4095. All three runs produced 158 blocks, so pitch
changed independently of duration. PortAudio captured MG-XU at 44,100 Hz,
stereo 16-bit PCM, with zero input overflows. No gain normalization was applied.

| Pitch | Measured fundamental | Capture |
|---|---:|---|
| low | 21.98 Hz | [low](pitch-low.wav) |
| noon | 87.15 Hz | [noon](pitch-noon.wav) |
| high | 250.57 Hz | [high](pitch-high.wav) |


[Pitch analysis](pitch-analysis.json) estimates periodicity from a sustained
window by autocorrelation within each expected octave range. Run
`python3 analyze-pitch.py` with NumPy installed to repeat it. These recordings
establish actual Talko output and the pitch response. No genuine SpeakJet
reference recording was made, so they do not score hardware spectral fidelity.
