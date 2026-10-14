# Freeze — Design Spec

Date: 2026-10-14

## Goal

A one-knob granular VST3 effect that turns rhythmic input (e.g. a pluck pattern)
into one sustained, continuous sound. Turning the knob crossfades from the dry
signal to a granular "freeze" cloud built from the recent input.

**Success criteria**
- On a pluck rhythm at 100%, the gaps between plucks are filled: output is a
  continuous sustained tone with the input's pitch/timbre, no audible rhythm.
- No clicks or level jumps when turning the knob or during steady playback.
- At 0%, output equals input exactly.
- Builds as VST3 on macOS (locally) and Windows (GitHub Actions) from one
  codebase, and passes pluginval.
- Can be heard and checked without a DAW (render tool + Standalone).

## Scope

In: VST3, macOS + Windows, stereo (mono also works), one parameter, minimal UI.
Out (v1): AU/CLAP/AAX, presets, extra parameters (grain size, density, pitch),
custom graphics, tempo sync, MIDI.

## Tech and workflow

A standalone project written from scratch: no code, assets or files are taken
from any other plugin. The workflow is built for a Mac **without a DAW and
without admin rights**.

- C++17, JUCE 8.0.15 cloned into `external/JUCE` by `scripts/setup.sh`.
- **"Freeze" by fouren**: product name "Freeze", company name "fouren",
  manufacturer code `Fren` (the same code for every fouren plugin, so hosts
  group them together), plugin code `Frz1`, bundle id `com.fouren.freeze`.
  CMake target `Freeze`.
- Formats: **VST3 + Standalone**. On macOS the VST3 is copied to
  `~/Library/Audio/Plug-Ins/VST3` after build (not on Windows).
- Buses: mono→mono, stereo→stereo.
- Tests: a tiny JUCE-free test harness (`Check.h` with `TEST`/`CHECK`
  macros, `TestMain.cpp` runner), one test executable run by CTest. No external
  test framework to download.

## Architecture

```
freeze/
├── CMakeLists.txt            # dsp lib + tests always; plugin/render/snapshot behind GF_BUILD_PLUGIN
├── scripts/setup.sh          # venv with cmake + ninja, clone JUCE 8.0.15 (no admin)
├── scripts/build.sh          # configure, build, run tests (--no-plugin for DSP only)
├── scripts/package-mac.sh    # universal VST3 -> dist/Freeze-<ver>-mac.zip
├── source/
│   ├── dsp/
│   │   ├── Random.h          # seeded xoshiro PRNG
│   │   └── FreezeEngine.h/.cpp  # JUCE-free, unit tested
│   ├── PluginProcessor.h/.cpp
│   └── PluginEditor.h/.cpp
├── tools/render/main.cpp     # offline render CLI
├── tools/snapshot/main.cpp   # editor -> PNG
├── tests/                    # Check.h, TestMain.cpp, test_*.cpp
└── .github/workflows/build.yml  # macOS + Windows build, tests, pluginval, zips
```

`FreezeEngine` interface:
- `prepare(double sampleRate, int maxBlockSize, int numChannels)` — allocates
  everything. No allocation anywhere else.
- `reset()` — clears buffers and grains.
- `process(float* const* channels, int numChannels, int numSamples, const float* freezeAmount)`
  — in-place; `freezeAmount` is the per-sample smoothed knob value (0..1).
- `setSeed(uint64_t)` — deterministic RNG for tests and renders.

Grain scheduling and segment analysis run on an absolute sample counter, so
output is identical for any host block size.

`PluginProcessor` owns the `AudioProcessorValueTreeState` with one float
parameter `freeze` (0..1, default 0, displayed as 0–100 %), smooths it with a
50 ms linear `SmoothedValue`, fills a per-sample amount array, and calls the
engine. Reported latency: 0. `ScopedNoDenormals`. State saved as APVTS XML.

## DSP (per channel)

1. **Capture buffer.** 2 s circular buffer per channel. Input is analysed in
   10 ms segments; for each segment the RMS is stored in a parallel energy table.
2. **Silence hold.** If a segment's RMS is below −60 dBFS, the write head does
   not advance (the segment is not recorded). The buffer therefore always holds
   the most recent *sounding* material, so the freeze sustains through gaps and
   after the input stops entirely.
3. **Grain scheduling.** A new grain starts every 20 ms (±25 % random jitter),
   giving ~6× overlap. Up to 16 grain voices per channel; if all are busy the
   oldest is reused.
4. **Grain playback.** Each grain is 120 ms, Hann-windowed, read at original
   pitch from its start position. Start position is chosen at random from the
   segments in the last 1 s of recorded material whose energy is
   ≥ 25 % of the loudest segment in that window (energy-weighted selection, so
   quiet decay tails are favoured less than attacks/bodies). Grain must not
   overlap the write head while playing (start ≥ 120 ms behind it).
5. **Stereo.** Each channel uses its own random draws, so L/R decorrelate and
   the cloud is wide.
6. **Wet gain.** Grains are summed and scaled by a fixed `1 / sqrt(overlap · 0.375)`
   (power-sum normalisation for uncorrelated Hann grains) so the wet level
   roughly matches the input level.
7. **Mix.** Equal-power crossfade per sample:
   `out = dry · cos(k·π/2) + wet · sin(k·π/2)`, with `k` the smoothed amount.
8. **Bypass at 0.** When `k` is 0 for the whole block, grain rendering is
   skipped (capture continues so material is ready when the knob is raised) and
   the input passes through untouched.

Edge cases: before any sound has been captured, wet output is silence.
Sample-rate change → `prepare` reallocates and resets. Real-time safe: no
allocation, locks, or I/O in `process`.

## UI

Plain JUCE editor, ~240×260 px: title "Freeze", one large rotary knob
labelled "Freeze" showing 0–100 %, APVTS slider attachment. Custom styling is
out of scope for v1.

## Dev environment (no admin, no DAW)

- `scripts/setup.sh` once, then `scripts/build.sh` builds everything and runs
  the tests.
- **Listening:** the `render` CLI:
  - `--test-pluck` — built-in 8 s pluck rhythm (decaying saw notes of a short
    chord progression, 4 per second, silence between);
  - `--in file.wav` for your own audio;
  - `--freeze 0..1` (default 1) or `--sweep` (knob ramps 0 → 1 over the file);
  - writes `renders/00 Dry.wav` plus the processed WAV, so you can A/B.
- **Live:** the Standalone app (`build/Freeze_artefacts/Release/Standalone/`)
  with your mic or audio interface input.
- **UI check:** `snapshot` renders the editor to a PNG.
- **Windows:** push to GitHub; the workflow builds on macOS (universal) and
  Windows, runs unit tests and pluginval (strictness 5), and uploads
  `Freeze-<ver>-windows.zip` / `-mac.zip` with an Install.txt each.

## Testing

Unit tests on `FreezeEngine` (fixed seed, 48 kHz):
- **Pass-through:** amount 0 → output bit-identical to input.
- **Gap filling:** pluck train (decaying 220 Hz sine, 4 per second, silence
  between). At amount 1, after a 1 s warm-up, RMS measured in every 10 ms window
  stays above −20 dB relative to the overall output RMS (no gaps).
- **Sustain after input stops:** feed plucks for 2 s, then 3 s silence → output
  in the silent section is still within 6 dB of the earlier wet RMS.
- **Level:** wet RMS within ±4 dB of the input's sounding-segment RMS.
- **No clicks:** max sample-to-sample difference of the wet output on a sine
  input stays below 4 × the sine's own max slope.
- **Silence in → silence out:** with no prior input, output is all zeros.
- **Knob sweep:** ramping amount 0→1 over 1 s stays within the same click bound.
- **Determinism:** same seed → identical output.
- **Block-size independence:** block sizes 1, 64 and 512 give identical output.
- **Stability:** 60 s of noise at amount 1 → no NaN/Inf, bounded peak.

Plugin-level: pluginval strictness 5 in CI (macOS + Windows).

Success is judged by ear on `render --test-pluck` and your own audio, then in
a DAW on whichever machine has one.

## Rev 7 (2026-10-14): three knobs, presets, reverb and delay

Requested by the user during implementation; replaces "one parameter" and "presets out of scope".

**Changes from earlier sections (as built):**
- Grains are centred on the chosen loud segment (start = position − L/2), so
  the Hann peak lands on the loud material. The newest `ceil(L/2 / 10 ms)`
  segments are excluded so a grain never reads past the write head.
- The gap test measures residual pluck rhythm (level folded over the 250 ms
  pluck period: quietest bin ≥ 60 % of the mean) and requires every 10 ms
  window above −30 dB. Granulating a steady tone makes single windows wander
  at random, so a −20 dB per-window bound is not meaningful.

**Parameters** (all automatable, saved with the project):

| ID | Name | Range | Default |
|---|---|---|---|
| `freeze` | Amount | 0–100 % | 0 % |
| `grain` | Grain | 0–100 % → grain length 40–500 ms, logarithmic | preset |
| `fx` | FX | 0–100 % | preset |
| `preset` | Preset | Ice, Glass, Haze, Ember, Abyss | Ice |

- Grain spacing = grain length / preset density, so overlap (and therefore level and
  smoothness) stays constant while the knob moves. Wet gain = `1/sqrt(density·0.375)`.
  Each grain keeps the length it was spawned with, so knob moves never click.
- FX: `wetOut = (wet + fx·effects(wet)) / sqrt(1 + fx²)`, where
  `effects = reverbLevel·reverb(wet) + delayLevel·delay(wet)` from the preset.
  Applied to the frozen signal only, before the Amount crossfade, so Amount 0 %
  is still bit-identical pass-through. While bypassed (Amount 0 for a whole block)
  the FX state is cleared too.
- Reverb: JUCE-free Freeverb-style network (8 damped combs + 4 allpasses per
  channel, right channel offset by 23 samples). Delay: feedback delay with a
  one-pole low-pass in the loop; delay time glides over 50 ms on preset change.
- Choosing a preset sets Grain and FX to the preset's starting values (on the
  message thread). Presets are also exposed as host programs.

**Presets:**

| Preset | Accent colours | Grain start | Density | FX start | Reverb (size, damp, level) | Delay (time, feedback, level) |
|---|---|---|---|---|---|---|
| Ice | blue | 120 ms | 6 | 0 % | 0.5, 0.3, 1.0 | — |
| Glass | cyan | 50 ms | 10 | 20 % | 0.6, 0.1, 1.0 | — |
| Haze | violet | 300 ms | 6 | 40 % | 0.85, 0.5, 1.0 | — |
| Ember | orange | 180 ms | 4 | 35 % | 0.5, 0.4, 0.4 | 300 ms, 0.45, 0.8 |
| Abyss | indigo | 500 ms | 4 | 70 % | 0.95, 0.6, 1.0 | 600 ms, 0.55, 0.6 |

**UI:** wide panel (~620×250). Top: letter-spaced FREEZE wordmark (left) and a
`‹ Preset ›` selector (centre). Middle: big Amount knob over the ice splash.
Left: Grain knob, right: FX knob (smaller, same style, coloured labels). The
splash, value rings, labels and snowflake take the preset's accent colours.
