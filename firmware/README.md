# Firmware images

Current release: **[Talko_SpeakJet 1.0](Talko_SpeakJet-1.0.uf2)** for Talko 2
(RP2040). This is the normal build, with all seven Emy groups and labels-only
serial output. [Installation instructions](../README.md#install).

Verify the downloaded image from this directory:

```sh
shasum -a 256 -c Talko_SpeakJet-1.0.uf2.sha256
```

## Rollback

| Image | Purpose |
|---|---|
| [0.9.3](Talko_SpeakJet-0.9.3.uf2) | Previous release with the 0.4.3 engine. |
| [0.9.1](rollback/Talko_SpeakJet-0.9.1.uf2) | Earlier working controls/Queue version, before the new word banks. |
| [0.8](rollback/Talko_SpeakJet-0.8.uf2) | Original baseline for sound comparisons; PITCH, FX and ERROR panel controls were not implemented yet. |

Each rollback image has a SHA-256 sidecar. These are the original binaries.
Other prototypes and raw measurements are archived locally and retained in Git
history rather than included in the current public tree.
