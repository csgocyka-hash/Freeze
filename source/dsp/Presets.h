#pragma once
// Preset table: colours for the UI, grain and FX starting points, density and effect character.
#include "Fx.h"
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
};

constexpr int kNumPresets = 5;

inline const Preset& preset (int index)
{
    //                       name     accent       light        deep         grain  dens  fx     size   damp  rvb   delay fb     dly
    static const Preset presets[kNumPresets] = {
        { "Ice",   0xff4f9dff, 0xffa8e0ff, 0xff2a55e6, 0.120, 6.0,  0.00f, { 0.50f, 0.3f, 1.0f, 0.3f, 0.00f, 0.0f } },
        { "Glass", 0xff2cc4d8, 0xffa6f2f5, 0xff0f8aa8, 0.050, 10.0, 0.20f, { 0.60f, 0.1f, 1.0f, 0.3f, 0.00f, 0.0f } , 8.0 },
        { "Haze",  0xff8f6cf0, 0xffd2c4ff, 0xff5a35c9, 0.300, 6.0,  0.40f, { 0.85f, 0.5f, 1.0f, 0.3f, 0.00f, 0.0f } },
        { "Ember", 0xfff07a2c, 0xffffc78f, 0xffd2401c, 0.180, 4.0,  0.35f, { 0.50f, 0.4f, 0.4f, 0.3f, 0.45f, 0.8f } },
        { "Abyss", 0xff3f4fc4, 0xff8f9cf2, 0xff1d2370, 0.500, 4.0,  0.70f, { 0.95f, 0.6f, 1.0f, 0.6f, 0.55f, 0.6f } },
    };
    return presets[std::clamp (index, 0, kNumPresets - 1)];
}
} // namespace gf
