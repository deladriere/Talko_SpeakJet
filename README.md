# Talko_SpeakJet

Speech and sound firmware for the **Polaxis Talko 2 (RP2040)**, based on
SpeakJet. Version **1.0** has 14 banks, 347 selections, live pitch/speed,
effects, ERROR freeze and native phoneme blending in Queue mode.

[User manual](https://www.polaxis.be/talko-speakjet-user-manual/) ·
[Firmware catalogue and browser installer](https://www.polaxis.be/talko-alternative-firmwares/).

The 1.0 engine updates phoneme transitions and effect timing. To reproduce the earlier sound, use the [0.9.3 image](firmware/Talko_SpeakJet-0.9.3.uf2).

## Install

1. Download [Talko_SpeakJet-1.0.uf2](firmware/Talko_SpeakJet-1.0.uf2).
2. Connect Talko by USB. Hold **BOOTSEL**, press and release **RESET**, then
   release BOOTSEL when the `RPI-RP2` drive appears.
3. Copy the UF2 to that drive. Talko restarts and announces its version.
4. Select bank **0**, turn SOUND fully left and press **SPEAK** for HELLO.

[Checksums and rollback images](firmware/README.md).

## Controls

| Control | Function |
|---|---|
| Encoder turn | Select a bank, or an effect on the FX page. Selection stops at both ends. |
| Encoder press | Switch bank/FX pages. The decimal point stays lit on the FX page. |
| SOUND / CV | Select an entry in the bank. |
| SPEAK / GATE | Trigger the selection. |
| PITCH | Live vocal pitch; noon is the native default. |
| SPEED | Live playback rate, independently of pitch; noon is normal. |
| FX | Effect amount. Fully left is dry. |
| ERROR | Hold a recent sound loop. PITCH changes its rate, SPEED its length; FX stays live. Release to resume. |

Switch positions: **left = Speech** (play once), **middle = Gate** (cut when
the gate falls), **right = Queue** (feed phonemes into one continuous voice).
For a Queue patch, use bank **7**, lower SPEED, turn FX fully left and clock
triggers while moving SOUND. The rear LED turns green when the native FIFO
is more than half full.

Effects: Off, Ring modulation, Reverb, Delay, Bit crush, Drive, Low-pass and
High-pass. Delay is selected by default.

## Banks

The seven original Emy groups follow the curated banks, in their original
order. The display continues after 9 with **A, b, C, d**.

| Display | Bank | Entries |
|---|---|---:|
| 0 | Robot essentials | 16 |
| 1 | Performance commands | 16 |
| 2 | Synth laboratory | 16 |
| 3 | Space | 16 |
| 4 | Rhythmic syllables | 16 |
| 5 | Numbers: ZERO–FIFTEEN | 16 |
| 6 | Short transmissions | 16 |
| 7 | Vowels | 12 |
| 8 | Voiced | 54 |
| 9 | Voiced Stop | 12 |
| A | Voiceless | 18 |
| b | All | 72 |
| C | All Effects | 55 |
| d | DTMF | 12 |

[Full word lists and bank editing](banks/README.md).

## USB serial

The USB name is **Talko_SpeakJet 1.0**. Normal output is only the spoken
label, one line per utterance, including repeated Queue selections.
[Optional developer commands and Console animation](docs/DEVELOPMENT.md#serial-interface).

## Build and test

Use PlatformIO with the `pico` environment. Close the serial monitor before uploading.

```sh
pio run                 # .pio/build/pico/firmware.uf2
pio run -t upload
SANITIZE=1 sh tests/run_host.sh
```

The tests use Python 3 and a C/C++ compiler. The bank generator is checked by
the host suite. See [development notes](docs/DEVELOPMENT.md) for hardware pins,
architecture and preserving the Talko adaptations during upstream updates.
[1.0 bench results](docs/VALIDATION-1.0.md).

## Credits

Emulator: Andrew Davie’s [SpeakJet 0.5.5](https://github.com/andrew-davie/SpeakJet/tree/b05428e95fb1e2f3dba4fb7a62f9f89320364e49),
with an RP2040 port and Talko controls. [Upstream MIT notice](lib/SpeakJet/LICENSE).
The original phoneme grouping and number dictionary come from Polaxis Emy.
New vocabulary uses CMU-derived SpeakJet pronunciations; [dictionary notice](banks/CMUDICT-LICENSE).
