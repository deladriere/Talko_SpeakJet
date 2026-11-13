# Release validation

**Talko_SpeakJet 0.9.3**, source commit `760d5cb`, was built for RP2040 at
133 MHz, flashed with picotool readback verification and identified as 0.9.3
over USB. The user subsequently confirmed it working.

| Item | Result |
|---|---|
| Bank layout | Seven curated banks, then all seven original Emy groups |
| Selections | 347; every entry renders finite bounded PCM and terminates |
| Emy grouping | Original order and code ranges match baseline `55561ba` |
| Host suite | Passed with address and undefined-behavior sanitizers |
| Live-join cases | 6,156 passed: 5,757 identical accepted joins; 399 safely separate |
| Panel checks | All bank glyphs/triggers through d, encoder reversal, modes, ADCs, FX and ERROR |
| Serial checks | Labels-only default, explicit diagnostics/animation, reconnect reset and bounded writes |
| Static RAM | 44,348 bytes |
| Linked flash | 172,200 bytes |

The host tests also cover tagged/plain PCM parity, Speed/modifier streams,
FIFO/reset bounds, independent pitch/rate, FX dry switching and ERROR looping.
No new on-device underrun benchmark was run for 0.9.3; physical testing was left
to the user. Earlier timing captures describe their own firmware versions.

The [release image](../firmware/Talko_SpeakJet-0.9.3.uf2) has SHA-256:

```text
a20c2f89210e69739c0cee735de9fa85ec9c1e6dde8f109c7cbdf55db51afeab
```

Run `SANITIZE=1 sh tests/run_host.sh` to repeat host checks. Device measurements
are opt-in; see [development notes](DEVELOPMENT.md#builds-and-measurements).
Raw logs, captures and machine-specific paths are kept out of the public working
tree. The original detailed records remain in Git history at `760d5cb`.
