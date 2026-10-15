# Freeze

A granular VST3 effect that turns a rhythm, such as a pluck pattern, into one sustained sound. It keeps the last couple of seconds of sound you play (silence is skipped) and replays them as a cloud of overlapping grains.

| Control | What it does |
|---|---|
| **Amount** (big knob) | Blends from your dry signal (0 %) to the full freeze (100 %). At 0 % the audio passes through untouched. |
| **Grain** | Grain length, from 40 ms (shimmery, textured) to 500 ms (smooth, slow). |
| **FX** | Reverb and delay on the frozen sound only. |
| **Preset** (‹ › at the top) | Ice, Glass, Haze, Ember, Abyss. Each preset has its own colours, grain density and kind of reverb/delay, and sets Grain and FX to a starting point. |

The freeze keeps holding after you stop playing, as long as Amount is up.

## Build on macOS (no admin needed)

```bash
scripts/setup.sh      # one time: Python venv with cmake + ninja, and JUCE 8.0.15, all inside this folder
scripts/build.sh      # builds everything and runs the tests
```

The VST3 is copied to `~/Library/Audio/Plug-Ins/VST3/Freeze.vst3`. The Standalone app is `build/Freeze_artefacts/Release/Standalone/Freeze.app`: it plays your mic or audio interface input through Freeze live.

## Listening without a DAW

```bash
# every preset on the built-in 8 s pluck rhythm -> renders/01 Ice.wav ... 05 Abyss.wav (+ 00 Dry.wav)
build/render_artefacts/Release/render --test-pluck --all

# one preset with Amount sweeping 0 -> 100 %
build/render_artefacts/Release/render --test-pluck --preset Haze --sweep --out "renders/Haze sweep.wav"

# your own audio, with your own knob settings (0..1)
build/render_artefacts/Release/render --in my_plucks.wav --preset Glass --grain 0.3 --fx 0.5 --out renders/mine.wav
```

To see the plugin window as a PNG (Amount 0..1, preset 0..4):

```bash
build/snapshot_artefacts/Release/snapshot renders/ui.png 0.65 2
```

## Windows

Push this folder to a GitHub repository. The workflow in `.github/workflows/build.yml` builds Freeze on macOS and Windows, runs the tests and pluginval, and attaches ready-to-install zips to the run. Download **Freeze-download-Windows** from the run's *Artifacts* section and move `Freeze.vst3` into `C:\Program Files\Common Files\VST3`.

For a Mac download zip (Apple silicon and Intel): `scripts/package-mac.sh` → `dist/Freeze-1.0.0-mac.zip`.

## Layout

| Path | Contents |
|---|---|
| `source/dsp/` | JUCE-free DSP: capture buffer and grain cloud (`FreezeEngine`), reverb and delay (`Fx`), presets |
| `source/` | JUCE plugin: parameters, presets as programs, editor |
| `tools/render`, `tools/snapshot` | Offline audio renderer and UI-to-PNG tool |
| `tests/` | Unit tests (run by `scripts/build.sh`) |
| `docs/superpowers/` | Design spec and implementation plan |

## Fonts and knob design

The UI uses Montserrat (ExtraLight for the logo, Light in spaced capitals for everything else), embedded in the plugin so it looks the same on macOS and Windows. Montserrat is under the SIL Open Font License; the licence is in `resources/fonts/OFL.txt` and ships in the download zips. The knob started from KnobGallery item #1349 (CC0), see `resources/knobs/CREDITS.md`.

## Licence note

JUCE is licensed under AGPLv3 or a commercial licence. Check its terms before distributing the plugin.
