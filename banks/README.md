# Talko_SpeakJet bank catalogue

`catalogue.json` is the editable source for the ten banks. Curated banks 0, 2,
3, 4, 7, 8 and 9 each contain 16 entries; the native banks keep their full
ranges. SOUND/CV scans entries in the listed order. Regenerate with:

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

Bank 5 retains every allophone; bank 6 retains every native effect, including
DTMF. The former overlapping voiced/stop/voiceless/DTMF panel banks are replaced
by vocabulary and syllable banks. The long pangram is absent from this release.
