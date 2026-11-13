# Talko_SpeakJet 0.9.2 — 13 November 2026

The **normal RP2040 image** was flashed to Talko serial `E6647C74032B9C24`
with picotool readback verification. USB reports `Talko_SpeakJet 0.9.2` after
reboot. The saved UF2 contains no diagnostic input override. The user requested
that listening and physical control testing be left to them, so no on-device
playback/underrun benchmark or MG-XU capture was run for this version. The
separate diagnostic image was built but was never flashed.

- [Release UF2](../../Talko_SpeakJet-0.9.2.uf2)
- SHA-256: `976cef5b7db906c535e5943eb02d8a821edf9c42d4fc53f17e11417ef37bc4bc`
- [Build log](build.log), [host tests](host-tests.log), [source hashes](source-sha256.txt)
- [Flash readback verification](flash-verification.log), [USB version](usb-version.log), [summary](summary.json)
- Baseline: `fb4abd7` (0.9.1 controls/Queue repair). Earlier UF2s are preserved.
- RP2040 B2, 133 MHz, 2 MB flash; PlatformIO `pico`, release `-O2`.
- Static RAM 44,348 bytes; linked flash 172,016 bytes. The additional banks cost
  4,328 bytes of flash compared with 0.9.1, with unchanged static RAM usage.
- Emulator source/tables retain SpeakJet 0.4.3 provenance at
  `ea2935018ed8ee90becb28b48fc2c83d03f683fe` and all existing Talko adaptations.

## Bank content

| Bank | Content | Entries |
|---|---|---:|
| 0 | Robot essentials | 16 |
| 1 | Native vowels | 12 |
| 2 | Performance commands | 16 |
| 3 | Synth laboratory | 16 |
| 4 | Space | 16 |
| 5 | All native allophones | 72 |
| 6 | Native chip sounds, including DTMF | 55 |
| 7 | Rhythmic syllables | 16 |
| 8 | ZERO–FIFTEEN | 16 |
| 9 | Short transmissions | 16 |

There are 251 selections. The longest curated entry uses 16 of 64 native FIFO
bytes. Every curated bank has 16 entries and distinct labels. Unknown mnemonic
tokens, invalid native ranges, incorrect bank counts and stale generated data
are rejected by `scripts/generate_banks.py --check`.

The editable catalogue records dictionary hash
`f8dceea132f59f62dfdff3c85df103ba5b7c2ca46e9545b4d8abdf31afd685a2`.
New dictionary words use the supplied SpeakJet mnemonics, with P4 between words.
The five original words and ZERO–NINE preserve their 0.9.1 bytes. Open syllables
have no added pause. See [catalogue/provenance notes](../../banks/README.md).
The long pangram has been removed. Startup speaks/displays 0.9.2 and the USB
product and UF2 filename use that same version.

## Host validation

`SANITIZE=1 sh tests/run_host.sh` passed with address and undefined-behavior
sanitizers. The generator consistency check also passed.

- All 251 entries render finite bounded PCM, terminate and leave an empty FIFO.
- Tagged/plain rendering is sample-identical; 24 Speed/modifier streams and
  overflow/reset bounds pass.
- All 6,156 native pair/arrival cases pass: 5,757 accepted joins match
  prebuffered PCM; 399 remain safely separate. Late arrivals, explicit Repeat,
  reset and exact noise-cache regressions pass.
- Live pitch/rate independence, slow native joins and half-full flow control pass.
- FX dry/switch-tail behavior, ERROR looping/rate and control scaling pass.
- Actual panel-loop tests pass for all switch positions, Gate cuts, Queue
  selections, ADCs, FX page/dot and ERROR. Bank-limit reversal now checks the
  new bank 9 upper limit.

The benchmark script is updated for the new bank counts, version, phrase
requests and relocated RAW diagnostic bank. It is available for future testing
but was not run after the user requested manual testing. The hardware timing
results in the 0.9.1 archive describe that earlier image, not this release.
