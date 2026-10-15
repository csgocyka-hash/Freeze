#pragma once
// Preset table: colours for the UI, grain and FX starting points, density and effect character.
#include "FreezeEngine.h"
#include "Fx.h"
#include "Tone.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gf
{
constexpr double kMinGrainSeconds = 0.040;
constexpr double kMaxGrainSeconds = 0.500;

// Grain knob (0..1) to grain length, logarithmic from 40 ms to 500 ms.
inline double grainKnobToSeconds (float knob)
{
    return kMinGrainSeconds * std::pow (kMaxGrainSeconds / kMinGrainSeconds, (double) std::clamp (knob, 0.0f, 1.0f));
}

inline float grainSecondsToKnob (double seconds)
{
    return (float) (std::log (seconds / kMinGrainSeconds) / std::log (kMaxGrainSeconds / kMinGrainSeconds));
}

struct Preset
{
    const char* name;
    uint32_t accent, accentLight, accentDeep; // ARGB
    double grainSeconds;
    double density; // grains overlapping: spacing = grain length / density
    float fx;
    FxCharacter character;
    double smoothOverlap = 6.0; // grains overlapping at full Amount; short-grain presets use 8
    ToneCharacter tone {};      // EQ, saturation, chorus and width of the wet signal
    float noise = 0.0f;         // starting Noise level
    EngineMode mode = EngineMode::normal;
    float octaveShare = 0.0f;   // share of grains an octave up
};

constexpr int kNumPresets = 9;

inline const Preset& preset (int index)
{
    //                       name     accent       light        deep         grain  dens  fx     size   damp  rvb   delay fb     dly
    // tone:                 low cut  low shelf Hz / dB  high shelf Hz / dB  saturation  chorus mix / rate  width
    static const Preset presets[kNumPresets] = {
        { "Ice",    0xff4f9dff, 0xffa8e0ff, 0xff2a55e6, 0.120, 6.0,  0.00f, { 0.50f, 0.3f, 1.0f, 0.3f, 0.00f, 0.0f }, 6.0,
          { 120.0f, 200.0f, 0.0f, 8000.0f, 2.0f, 0.00f, 0.00f, 0.20f, 1.0f }, 0.00f },
        { "Glass",  0xff2cc4d8, 0xffa6f2f5, 0xff0f8aa8, 0.050, 10.0, 0.20f, { 0.60f, 0.1f, 1.0f, 0.3f, 0.00f, 0.0f }, 8.0,
          { 200.0f, 200.0f, 0.0f, 10000.0f, 3.0f, 0.00f, 0.15f, 0.20f, 1.0f }, 0.15f },
        { "Haze",   0xff8f6cf0, 0xffd2c4ff, 0xff5a35c9, 0.300, 6.0,  0.40f, { 0.85f, 0.5f, 1.0f, 0.3f, 0.00f, 0.0f }, 6.0,
          { 0.0f, 300.0f, 2.0f, 5000.0f, -4.0f, 0.10f, 0.30f, 0.20f, 1.0f }, 0.10f },
        { "Ember",  0xfff07a2c, 0xffffc78f, 0xffd2401c, 0.180, 4.0,  0.35f, { 0.50f, 0.4f, 0.4f, 0.3f, 0.45f, 0.8f }, 6.0,
          { 0.0f, 200.0f, 3.0f, 4000.0f, -5.0f, 0.25f, 0.00f, 0.20f, 1.0f }, 0.20f },
        { "Abyss",  0xff3f4fc4, 0xff8f9cf2, 0xff1d2370, 0.500, 4.0,  0.70f, { 0.95f, 0.6f, 1.0f, 0.6f, 0.55f, 0.6f }, 6.0,
          { 0.0f, 120.0f, 3.0f, 3000.0f, -8.0f, 0.15f, 0.25f, 0.08f, 1.4f }, 0.00f },
        // Some grains an octave up, spread wide
        { "Octave", 0xffe86fb0, 0xffffc4e1, 0xffb8327a, 0.150, 6.0,  0.35f, { 0.80f, 0.3f, 1.0f, 0.3f, 0.00f, 0.0f }, 6.0,
          { 150.0f, 200.0f, 0.0f, 9000.0f, 2.0f, 0.00f, 0.15f, 0.25f, 1.7f }, 0.05f, EngineMode::normal, 0.35f },
        // Steps to a new slice on every division
        { "Rhythm", 0xff2fbf8f, 0xffa8f0d6, 0xff12805c, 0.080, 8.0,  0.15f, { 0.50f, 0.3f, 1.0f, 0.3f, 0.00f, 0.0f }, 8.0,
          { 100.0f, 200.0f, 0.0f, 8000.0f, 1.0f, 0.05f, 0.00f, 0.20f, 1.0f }, 0.00f, EngineMode::rhythm },
        // Volume pulse that speeds up with Amount
        { "Pulse",  0xfff2b33d, 0xffffe3a3, 0xffc27d0e, 0.120, 6.0,  0.25f, { 0.60f, 0.4f, 1.0f, 0.3f, 0.00f, 0.0f }, 6.0,
          { 0.0f, 200.0f, 1.0f, 8000.0f, 0.0f, 0.10f, 0.10f, 0.20f, 1.0f }, 0.00f, EngineMode::pulse },
        // Plays only while the input plays (or, inverted, only while it is silent)
        { "Follow", 0xffef5b5b, 0xffffbcbc, 0xffb82a2a, 0.100, 6.0,  0.20f, { 0.55f, 0.4f, 1.0f, 0.3f, 0.00f, 0.0f }, 6.0,
          { 80.0f, 200.0f, 0.0f, 8000.0f, 0.0f, 0.05f, 0.00f, 0.20f, 1.0f }, 0.00f, EngineMode::follow },
    };
    return presets[std::clamp (index, 0, kNumPresets - 1)];
}
} // namespace gf
