# Talko_SpeakJet 0.9.3 — Emy grouping and serial correction

The normal RP2040 image was flashed to serial `E6647C74032B9C24`, readback-verified
by picotool, and identified as **Talko_SpeakJet 0.9.3** over USB. No serial
commands, playback benchmark or audio recording were used after flashing;
listening and physical control testing are left to the user as requested.

- [Normal UF2](../../Talko_SpeakJet-0.9.3.uf2)
- SHA-256: `a20c2f89210e69739c0cee735de9fa85ec9c1e6dde8f109c7cbdf55db51afeab`
- [Build](build.log), [host tests](host-tests.log), [source hashes](source-sha256.txt)
- [Flash verification](flash-verification.log), [USB version](usb-version.json), [summary](summary.json)
- RP2040, 133 MHz, 2 MB flash; static RAM 44,348 bytes, linked flash 172,200 bytes.
- SpeakJet engine remains 0.4.3 with the existing Talko synthesis/Queue/DSP adaptations.
- Normal image contains no diagnostic `TEST` override; previous UF2s are preserved.

## Restored bank layout

| Display | Bank | Group | Entries | Native range |
|---|---:|---|---:|---|
| 0 | 0 | Robot essentials | 16 | curated |
| 1 | 1 | Performance commands | 16 | curated |
| 2 | 2 | Synth laboratory | 16 | curated |
| 3 | 3 | Space | 16 | curated |
| 4 | 4 | Rhythmic syllables | 16 | curated |
| 5 | 5 | Numbers | 16 | curated |
| 6 | 6 | Short transmissions | 16 | curated |
| 7 | 7 | Vowels | 12 | 128–139 |
| 8 | 8 | Voiced | 54 | 128–181 |
| 9 | 9 | Voiced Stop | 12 | 170–181 |
| A | 10 | Voiceless | 18 | 182–199 |
| b | 11 | All | 72 | 128–199 |
| C | 12 | All Effects | 55 | 200–254 |
| d | 13 | DTMF | 12 | 240–251 |

All seven native ranges and their order match the original Emy grouping from
baseline `55561ba`. The seven curated bank entry lists are unchanged from
0.9.2; only their bank numbers move. [Comparison record](restoration-check.log).
There are 347 selections. The generator enforces seven curated banks followed
by the complete Emy groups, and locates the startup digits through the numbers
bank's role. The longest curated entry remains 16 bytes of the native 64-byte FIFO.

## Serial output

Normal output consists exclusively of spoken labels, one newline-terminated
line at each utterance start, including repeated Queue selections. There is no
automatic banner, PANEL/context line, diagnostic report or end/cut STOP marker.
The vocabulary word STOP is still sent as its own spoken label.

STATUS/PANEL requests and STATS/MONITOR modes remain explicit developer tools.
An explicit Console handshake enables phoneme frames and STOP markers for that
client. A disconnect restores labels-only defaults. Serial writes remain bounded
by available USB capacity and audio remains on core 1.

## Host checks

`SANITIZE=1 sh tests/run_host.sh` passed with address/undefined-behavior sanitizers.
The generator consistency check and engine strict single-precision checks passed.

- All 347 selections terminate with finite bounded PCM and empty FIFOs;
  tagged/plain PCM matches and Speed/modifier/FIFO regressions pass.
- All 6,156 live pair/arrival cases pass (5,757 accepted identical joins,
  399 safely separate), plus late-arrival/Repeat/reset/noise-cache guards.
- Pitch/rate independence, slow native joins, half-full flow control, FX dry
  switching and ERROR loop/rate tests pass.
- The actual panel loop displays and triggers every bank through d, checks
  high-bank glyphs and upper-limit reversal, and retains mode/ADC/FX/ERROR checks.
- The actual serial-service implementation is tested with simulated USB and
  queues: silent connection/control activity; labels only; repeated HELLO;
  STOP vocabulary; DTMF labels; suppressed default reports; explicit STATUS,
  STATS and SAM responses; reconnect reset; and one-byte USB write capacity.

Hardware timing results in earlier validation archives describe their respective
images. This correction does not claim new on-device underrun measurements.
