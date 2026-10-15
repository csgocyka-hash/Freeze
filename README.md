# Freeze

A granular VST3 effect that turns a rhythm, such as a pluck pattern, into one sustained sound. It keeps the last couple of seconds of sound you play (silence is skipped) and replays them as a cloud of overlapping grains.

| Control | What it does |
|---|---|
| **Amount** (big knob) | Blends from your dry signal (0 %) to the full freeze (100 %). At 0 % the audio passes through untouched. Up to 50 % the freeze is a grain cloud; from 50 % to 100 % the grains get twice as long and about 24 overlap, they read from a copy of the input with each note's attack and decay flattened and every note start faded in (no clicks), and a quiet high-passed spectral layer (20 %) adds a steady shimmer. At 100 % there is no pluck left. |
| **Grain** | Grain length, from 40 ms (shimmery, textured) to 500 ms (smooth, slow). Near 100 % Amount it also sets how quickly the shimmer layer follows new notes (about 0.3 s to 4 s). |
| **FX** | Reverb and delay on the frozen sound only. |
| **Freeze** (snowflake, top right) | Freezes the sound playing right now: nothing new is recorded, so the grains keep playing that moment for as long as Freeze is on, while new notes pass through dry. Click again to release. |
| **Noise** (dots, bottom left) | Drag up or down to set the level (0–100 %, double-click resets); the dots fill with colour from the bottom up. Adds clean, airy noise with no low end (high-passed at 800 Hz). It also follows the big knob: louder and brighter as you turn up, sweeping from about 1.5 kHz to 14 kHz, up to about -32 dBFS. |
| **Detune** (lines, bottom right) | Drag up or down (0–100 %, double-click resets); the lines fill with colour from the bottom up. Each grain is pitched randomly up or down, by up to 50 cents at 100 %, for a wider, chorus-like cloud. |
| **Preset** (‹ › at the top) | Ice, Glass, Haze, Ember, Abyss, Octave, Rhythm, Pulse, Follow. Each preset has its own colours, grain density, tone (EQ, saturation, chorus, width) and kind of reverb/delay, and sets Grain, FX and Noise to a starting point. |
| **Octave** | Some grains play an octave up, spread wide. |
| **Rhythm** — division (top left, e.g. "1/4") | The frozen sound steps to a new slice every 1/8, 1/8 dotted, 1/4, 1/2 or 1 bar, in time with the host (120 BPM without one). Click the division to choose. |
| **Pulse** — UNLINK / TIME (top left) | A volume pulse that speeds up with Amount from 1/2 to 1/64, getting shallower as it speeds up, and steady at 100 %. UNLINK shows a TIME knob that sets the speed on its own. |
| **Follow** — INVERT (top left) | The frozen sound plays only while the input plays. INVERT: only while the input is silent (sidechain style). |

The freeze keeps holding after you stop playing, as long as Amount is up.

The window opens small (308 × 280). Click the FREEZE logo for Scaling (50, 75, 100, 125 or 150 %), or resize it from its edges; the size is saved with your project.

## Build on macOS (no admin needed)

```bash
scripts/setup.sh      # one time: Python venv with cmake + ninja, and JUCE 8.0.15, all inside this folder
scripts/build.sh      # builds everything and runs the tests
```

The VST3 is copied to `~/Library/Audio/Plug-Ins/VST3/Freeze.vst3`. The Standalone app is `build/Freeze_artefacts/Release/Standalone/Freeze.app`: it plays your mic or audio interface input through Freeze live.

## Listening without a DAW

```bash
# every preset on the built-in 8 s pluck rhythm -> renders/01 Ice.wav ... 09 Follow.wav (+ 00 Dry.wav)
build/render_artefacts/Release/render --test-pluck --all

# one preset with Amount sweeping 0 -> 100 %
build/render_artefacts/Release/render --test-pluck --preset Haze --sweep --out "renders/Haze sweep.wav"

# Detune sweeping from 0 to 100 % over the file, at Amount 45 %
build/render_artefacts/Release/render --test-pluck --preset Ice --freeze 0.45 --detune-sweep --out "renders/Ice detune sweep.wav"

# Grain knob sweeping from 40 ms to 500 ms over the file, at Amount 45 %
build/render_artefacts/Release/render --test-pluck --preset Ice --grain-sweep --freeze 0.45 --out "renders/Ice grain sweep.wav"

# switch Freeze (the snowflake) on 2.6 s in: the chord playing at that moment keeps sounding while the input moves on
build/render_artefacts/Release/render --test-pluck --preset Ice --hold-at 2.6 --out "renders/Ice hold.wav"

# your own audio, with your own knob settings (0..1)
build/render_artefacts/Release/render --test-pluck --preset Rhythm --freeze 1 --division 1/8D --bpm 100 --out "renders/Rhythm.wav"
# Pulse with its speed unlinked (--time 0..1), Follow inverted
build/render_artefacts/Release/render --test-pluck --preset Pulse --freeze 0.6 --time 0.5 --out "renders/Pulse.wav"
build/render_artefacts/Release/render --test-pluck --preset Follow --freeze 1 --invert --out "renders/Follow invert.wav"
build/render_artefacts/Release/render --in my_plucks.wav --preset Glass --grain 0.3 --fx 0.5 --out renders/mine.wav
```

To see the plugin window as a PNG (Amount 0..1, preset 0..8; add `unlink` or `invert` to switch those on):

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
