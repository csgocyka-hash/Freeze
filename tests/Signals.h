#pragma once
// Test signals and helpers for driving FreezeEngine. JUCE-free.
#include "dsp/FreezeEngine.h"
#include <algorithm>
#include <cmath>
#include <vector>

constexpr double kTwoPi = 6.283185307179586;

// Plucked sine: each pluck decays as amp * exp(-t / tau) and is hard-zeroed after 200 ms.
inline std::vector<float> pluckTrain (double sr, double seconds, double freqHz = 220.0,
                                      double plucksPerSec = 4.0, float amp = 0.5f, double tau = 0.025)
{
    std::vector<float> out ((size_t) std::lround (sr * seconds), 0.0f);
    const double period = 1.0 / plucksPerSec;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = (double) i / sr;
        const double local = std::fmod (t, period);
        if (local < 0.2)
            out[i] = (float) (amp * std::exp (-local / tau) * std::sin (kTwoPi * freqHz * local));
    }
    return out;
}

inline std::vector<float> sine (double sr, double seconds, double freqHz, float amp)
{
    std::vector<float> out ((size_t) std::lround (sr * seconds));
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = (float) (amp * std::sin (kTwoPi * freqHz * (double) i / sr));
    return out;
}

inline double rms (const float* x, size_t n)
{
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i)
        sum += (double) x[i] * x[i];
    return n > 0 ? std::sqrt (sum / (double) n) : 0.0;
}

// Processes `input` (one vector per channel, equal lengths) in blocks of `blockSize`.
inline std::vector<std::vector<float>> runEngine (gf::FreezeEngine& engine, std::vector<std::vector<float>> input,
                                                  const std::vector<float>& amount, int blockSize)
{
    const int numChannels = (int) input.size();
    const size_t length = input[0].size();
    std::vector<float*> ptrs ((size_t) numChannels);

    for (size_t pos = 0; pos < length; pos += (size_t) blockSize)
    {
        const int n = (int) std::min ((size_t) blockSize, length - pos);
        for (int ch = 0; ch < numChannels; ++ch)
            ptrs[(size_t) ch] = input[(size_t) ch].data() + pos;
        engine.process (ptrs.data(), numChannels, n, amount.data() + pos);
    }
    return input;
}
