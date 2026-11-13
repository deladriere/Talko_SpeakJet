# Talko_SpeakJet bank catalogue

`catalogue.json` is the editable source for fourteen banks. Curated banks 0–6
each contain 16 entries. The original Emy groups follow at 7–13, displayed as
7, 8, 9, A, b, C and d. SOUND/CV scans entries in the listed order. Keep these
seven groups separate and in their original order and ranges:

| Display | Group | Native range | Entries |
|---|---|---|---:|
| 7 | Vowels | 128–139 | 12 |
| 8 | Voiced | 128–181 | 54 |
| 9 | Voiced Stop | 170–181 | 12 |
| A | Voiceless | 182–199 | 18 |
| b | All | 128–199 | 72 |
| C | All Effects | 200–254 | 55 |
| d | DTMF | 240–251 | 12 |

Regenerate with:

```sh
python3 scripts/generate_banks.py
python3 scripts/generate_banks.py --check
```

The generated `src/banks_generated.h` contains the byte arrays, entry tables,
startup number table and bank map. Never edit it directly. The generator uses
the vendored engine's actual mnemonic table, rejects unknown tokens, checks
all curated bank sizes and requires each entry to fit the native 64-byte FIFO.
Host tests check that the generated header matches this catalogue.

New word pronunciations were selected from the user's existing
`SpeakJet_vox/SD/cmudict.txt`, a CMU-derived dictionary already converted to
SpeakJet mnemonics. Its SHA-256 is stored in the catalogue. Only selected
phoneme strings are stored here: the 5 MB dictionary is not loaded onto Talko
and is not needed to regenerate this snapshot. See the
[CMU dictionary notice](CMUDICT-LICENSE), copied from the
[upstream notice](https://github.com/cmusphinx/cmudict/blob/master/LICENSE).

Entries with `source: cmudict: ...` use the default pronunciation of each named
word. Consecutive repeated vowels are retained. Multiword entries insert P4
(the engine's short native pause) between words. They add no speed, pitch,
volume or bend commands, so a word cannot unexpectedly change the next queued
word's controls. Pronunciation is an approximation inherited from the local
conversion; listening and manual adjustments are appropriate when refining it.

HELLO, READY, ROBOT, DANGER, TALKO and ZERO through NINE preserve the exact
0.9.1 byte sequences. Rhythmic syllables are curated open syllables with no
inserted pause: A/E/I/O/U use AW/EH/IY/OW/UW. BA/BO/BU use BO; BE/BI use BE.
TA/TO/TU use TU; TE/TI use TT. The M series uses MM; SHH uses SH. These short
entries are intended for live Queue blending rather than dictionary spelling.

To add or refine an entry, keep its readable uppercase label, then supply
exactly one of `phonemes` (mnemonics, including P0–P6 where needed) or `bytes`
(native byte stream). Keep 16 entries in each curated bank and preserve the
first ten entries of `spokenDigits` in numeric order: startup uses them too.
Document the source for every manual override and run the generator and host
suite after editing.

Bank 5 now holds ZERO–FIFTEEN and is marked `role: numbers` in the catalogue.
The generator uses that role for the startup table rather than a fixed bank
number. It checks the first ten spoken labels remain ZERO–NINE in order.
All seven Emy groups are enforced by generation checks. The long pangram is
absent; overlapping native groups are intentional and must be preserved.
