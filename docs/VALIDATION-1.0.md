# Talko SpeakJet 1.0 bench results

Tested 21 November 2026 on a Talko 2 (RP2040, 133 MHz). The previous Talko SpeakJet release was 0.9.3 with engine 0.4.3. Version 1.0 incorporates [SpeakJet 0.5.5](https://github.com/andrew-davie/SpeakJet/blob/b05428e95fb1e2f3dba4fb7a62f9f89320364e49/CHANGELOG.md). The existing bank data feeds the new synthesizer directly. Vowel glides and joins across short pauses follow the revised formant model. Several effects have different oscillator assignments, levels and transitions.

## Build and desktop checks

- PlatformIO platform 4d1ad09, Arduino-Pico b8864711, GCC 14.2.0, normal pico build. The release UF2 has a SHA-256 sidecar.
- The host suite passed under AddressSanitizer and UndefinedBehaviorSanitizer. It rendered all 347 selections, tested 24 modifier and Speed streams, and exercised 6,156 live phoneme pairs. The renderer could join 5,634 pairs at the tested arrival time. The other 522 played separately to preserve audio already rendered.
- A native-rate comparison used the upstream AtariVox output model at 8,026 Hz. All 127 individual sound codes and three connected sequences finished in the same 64-sample block as upstream. The largest relative RMS waveform differences were approximately 0.3% for a single sound and 2.9% for the NE–GO–OW sequence.
- An effect followed by a vowel exercises a local fix that prevents the previous effect code from indexing the allophone table. Other regressions cover tagged/plain audio parity, live pitch and rate, panel logic, ERROR freeze and the serial protocol.

## Hardware

- Saved and verified the instrument's original flash before the comparison. Installed the 0.9.3 reference and then the 1.0 candidate on the same board and USB port. The candidate identified itself as `Talko_SpeakJet 1.0 build=normal engine=0.5.5`.
- The reference and candidate played HELLO, MM–AW–P4–IY, B6, C8, C9 and M1 into the same Yamaha MG-XU input at the same gain. Both stereo captures used 44.1 kHz, 16-bit PCM. All six windows had signal, with zero capture overflows and peaks below full scale.
- The panel was in Speech mode, with pitch and speed near centre and the FX pot near minimum. USB tests exercised Queue, Cut, repeated spoken labels, animation and serial disconnect/reconnect. Physical gate/CV performance at other panel settings and subjective listening remain separate checks.

The six captured selections also completed with zero device underruns on 1.0. HELLO averaged 4,622 µs per 64-sample block. The 0.9.3 and 1.0 WAV peaks were respectively 2,170 and 3,048 on the signed 16-bit scale. The mixer gain stayed fixed, so the recordings retain the level differences between the two versions.

The bank scan exposed a timing regression on ALARM_7. After optimizing the per-tone glide and level calculations, three repeated hardware plays of ALARM_7 averaged 6,239–6,247 µs per block and lost zero samples. The quick USB test now includes this effect.

| Device run | Streams | Blocks | Peak render time | Blocks over 7,974 µs | Missing samples |
|---|---:|---:|---:|---:|---:|
| 0.9.3 USB quick test | 23 | 1,649 | 7,980 µs | 1 | 0 |
| 1.0 same commands, taken from the full run | 23 | 1,659 | 7,565 µs | 0 | 0 |
| 1.0 all bank selections | 347 | 12,874 | 7,896 µs | 0 | 0 |
| 1.0 full USB run, including the bank selections | 358 | 13,754 | 7,896 µs | 0 | 0 |

The PWM ring covered any blocks that exceeded the deadline. The final full run returned all 12 spoken labels for the rapid Queue sequence and resumed USB reporting after the serial port reopened. The rows for 1.0 overlap.
