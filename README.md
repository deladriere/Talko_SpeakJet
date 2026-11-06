# Talko 2 SpeakJet 0.7 prototype

SpeakJet firmware for the **Talko 2 (RP2040)**. It brings the Emy-style
banks of vowels, allophones and effects to Talko, along with a bank of spoken
digits. In Queue mode, closely spaced triggers can blend phonemes into a
continuous voice.

## Install and play

1. Connect Talko with a USB data cable. Hold **BOOTSEL**, press and release
   **RESET**, then release **BOOTSEL** when the `RPI-RP2` drive appears.
2. Copy [Talko_SpeakJet-0.7.uf2](Talko_SpeakJet-0.7.uf2) to that drive.
   Talko restarts automatically.
3. Move the switch **left**, turn the encoder to bank **0**, and turn
   **SOUND** fully counterclockwise. Press **SPEAK** to hear **HELLO**.
4. Try bank **8** for spoken numbers. SOUND selects **ZERO** at the left end
   and **NINE** at the right end.

| Control | Action |
|---|---|
| Encoder turn | Select a bank. The display shows its number. |
| SOUND knob or CV | Select an entry within the current bank. |
| SPEAK button or GATE input | Trigger the selected entry. The display dot lights during playback. |
| SPEED, PITCH, FX and ERROR | Voice settings are fixed in 0.7. Select built-in effects with SOUND in bank 6. |

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
