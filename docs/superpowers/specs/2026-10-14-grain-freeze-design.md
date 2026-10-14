# Grain Freeze — Design Spec

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
- Builds as VST3 on macOS and Windows from one codebase.

## Scope

In: VST3, macOS + Windows, stereo (mono also works), one parameter, minimal UI.
Out (v1): AU/CLAP/AAX, presets, extra parameters (grain size, density, pitch),
custom graphics, tempo sync, MIDI.

## Tech

- C++17, JUCE 8 fetched via CMake `FetchContent` (pinned tag).
- CMake project; generators: Xcode / Ninja on macOS, Visual Studio on Windows.
- Plugin name "Grain Freeze", format VST3 only.
- Tests: Catch2 v3 via `FetchContent`, run with CTest.

## Architecture

```
Source/
  FreezeEngine.h/.cpp   pure C++ DSP, no JUCE dependency, unit-tested
  PluginProcessor.h/.cpp JUCE AudioProcessor: parameter, smoothing, calls engine
  PluginEditor.h/.cpp   one rotary slider bound to the parameter
Tests/
  FreezeEngineTests.cpp
CMakeLists.txt
```

`FreezeEngine` interface:
- `prepare(double sampleRate, int maxBlockSize, int numChannels)` — allocates
  everything. No allocation anywhere else.
- `reset()` — clears buffers and grains.
- `process(float* const* channels, int numChannels, int numSamples, const float* freezeAmount)`
  — in-place; `freezeAmount` is the per-sample smoothed knob value (0..1).
- `setSeed(uint32_t)` — deterministic RNG for tests.

`PluginProcessor` owns the `AudioProcessorValueTreeState` with one float
parameter `freeze` (0..1, default 0, displayed as 0–100 %), smooths it with a
50 ms linear `SmoothedValue`, fills a per-sample amount array, and calls the
engine. Reported latency: 0. State save/restore via the value tree.

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

Plain JUCE editor, ~240×260 px: title "Grain Freeze", one large rotary knob
labelled "Freeze" showing 0–100 %. Uses an APVTS slider attachment.

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
  input stays below a bound derived from the sine's own max slope × 4.
- **Silence in → silence out:** with no prior input, output is all zeros.
- **Knob sweep:** ramping amount 0→1 over 1 s produces no sample-diff spikes
  beyond the bound above.

Manual: load the VST3 in a DAW on macOS and Windows, run a pluck loop, sweep
the knob, check CPU and saving/reloading the project.
