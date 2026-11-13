# Bank catalogue

The display uses 0–9, then A, b, C and d. Serial commands use decimal bank
numbers 0–13 and entry numbers starting at 1. SOUND/CV scans each row left to
right. There are 16 entries per curated bank.

## Words, syllables and numbers

### 0 — Robot essentials

HELLO, READY, ROBOT, DANGER, TALKO, YES, NO, OKAY, GO, STOP, WAIT, ERROR, WARNING, WELCOME, THANK YOU, GOODBYE.

### 1 — Performance commands

START, STOP, PLAY, PAUSE, RECORD, ERASE, LOOP, REPEAT, FASTER, SLOWER, HIGHER, LOWER, FORWARD, REVERSE, HOLD, RELEASE.

### 2 — Synth laboratory

SIGNAL, VOLTAGE, CURRENT, CIRCUIT, PULSE, CLOCK, WAVE, NOISE, FILTER, FREQUENCY, TRIGGER, GATE, RHYTHM, SEQUENCE, TEMPO, FEEDBACK.

### 3 — Space

SPACE, TIME, ORBIT, PLANET, STAR, MOON, SUN, MARS, VENUS, SATURN, COSMIC, GALAXY, ALIEN, ROCKET, RADAR, LASER.

### 4 — Rhythmic syllables

BA, BE, BI, BO, BU, MA, ME, MI, MO, MU, TA, TE, TI, TO, TU, SHH.

### 5 — Numbers

ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, ELEVEN, TWELVE, THIRTEEN, FOURTEEN, FIFTEEN.

### 6 — Short transmissions

ARE YOU READY, SYSTEM ONLINE, SYSTEM FAILURE, ACCESS DENIED, HELLO HUMAN, WE ARE ROBOTS, MAKE SOME NOISE, FOLLOW THE RHYTHM, LOST IN SPACE, TAKE ME HOME, DO NOT PANIC, THANK YOU, GOOD MORNING, SEE YOU LATER, TRANSMISSION ENDED, END OF LINE.

## Original Emy groups

Keep all seven groups separate, in this order. Their overlapping ranges are
intentional.

| Display | Serial bank | Group | Native range | Entries |
|---|---:|---|---|---:|
| 7 | 7 | Vowels | 128–139 | 12 |
| 8 | 8 | Voiced | 128–181 | 54 |
| 9 | 9 | Voiced Stop | 170–181 | 12 |
| A | 10 | Voiceless | 182–199 | 18 |
| b | 11 | All | 128–199 | 72 |
| C | 12 | All Effects | 200–254 | 55 |
| d | 13 | DTMF | 240–251 | 12 |

## Editing

Edit [catalogue.json](catalogue.json), then regenerate the compiled header:

```sh
python3 scripts/generate_banks.py
python3 scripts/generate_banks.py --check
SANITIZE=1 sh tests/run_host.sh
```

Commands run from the repository root. Do not edit `src/banks_generated.h`
directly. The generator checks bank order, 16-entry curated banks, all Emy
ranges, known mnemonics, startup digit order and the 64-byte native FIFO limit.
The numbers bank uses `role: numbers`; its first ten labels must remain ZERO–NINE.

Each entry contains a readable uppercase `label`, exactly one of `phonemes`
(mnemonics, with P0–P6 when needed) or `bytes` (native bytes), and its `source`.
Document manual pronunciation changes in that source field.

## Pronunciation sources

New vocabulary was selected from the existing CMU-derived SpeakJet mnemonic
dictionary `SpeakJet_vox/SD/cmudict.txt`; its SHA-256 is recorded in the catalogue.
Only selected phoneme strings are compiled. The full dictionary is not required
to build or regenerate this snapshot. See the [CMU notice](CMUDICT-LICENSE).

Dictionary entries retain consecutive repeated vowels and insert P4 between
words, without changing queued speed, pitch, volume or bend. HELLO, READY,
ROBOT, DANGER, TALKO and ZERO–NINE preserve their original byte streams.

Rhythmic syllables add no pause. A/E/I/O/U use AW/EH/IY/OW/UW; BA/BO/BU use BO,
BE/BI use BE, TA/TO/TU use TU, TE/TI use TT, and the M series uses MM. SHH uses SH.
These short sequences are intended for live Queue blending. The old pangram is
not part of the catalogue.
