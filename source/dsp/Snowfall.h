#pragma once
// Snowfall: clean, airy noise added to the output. Its level and brightness follow the Amount knob: a
// 24 dB/oct high-pass at 800 Hz removes all low end, and a smooth (non-resonant) low-pass sweeps from
// 1.5 kHz at low Amount to 14 kHz at full Amount. Switching it on or off fades over 30 ms.
// JUCE-free; everything is allocated in prepare().
#include "Random.h"
#include <cstdint>
#include <vector>

namespace gf
{
class Snowfall
{
public:
    void prepare (double sampleRate, int numChannels, uint64_t seed = 1);
    void reset();

    // immediately = jump to the new state without the fade (for loading a project)
    void setEnabled (bool shouldBeOn, bool immediately);

    // Adds the noise to the audio in place. amount holds the smoothed Amount knob value (0..1) per sample.
    void process (float* const* channels, int numChannels, int numSamples, const float* amount);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float run (float x, float* state); // state: x1, x2, y1, y2
    };
    struct Channel
    {
        Random random;
        float highPassState[2][4] {}, lowPassState[4] {};
    };

    static Biquad makeHighPass (double frequency, double sampleRate);
    static Biquad makeLowPass (double frequency, double sampleRate);

    double sampleRate = 48000.0;
    uint64_t seed = 1;
    std::vector<Channel> channels;
    Biquad highPass, lowPass;
    float gain = 0.0f, gainTarget = 0.0f, gainStep = 0.0f, level = 0.0f;
    int counter = 0;
};
} // namespace gf
