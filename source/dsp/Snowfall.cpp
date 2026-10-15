#include "Snowfall.h"
#include <algorithm>
#include <cmath>

namespace gf
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;
constexpr double kHighPassHz = 800.0;
constexpr double kSweepFromHz = 1500.0, kSweepToHz = 14000.0;
constexpr float kFullLevel = 0.063f; // -24 dBFS RMS at full Amount
constexpr int kCoefficientInterval = 16; // samples between low-pass updates while sweeping
} // namespace

float Snowfall::Biquad::run (float x, float* s)
{
    const float y = b0 * x + b1 * s[0] + b2 * s[1] - a1 * s[2] - a2 * s[3];
    s[1] = s[0];
    s[0] = x;
    s[3] = s[2];
    s[2] = y;
    return y;
}

Snowfall::Biquad Snowfall::makeHighPass (double frequency, double sr)
{
    const double w0 = kTwoPi * frequency / sr, alpha = std::sin (w0) / (2.0 * 0.7071), c = std::cos (w0), a0 = 1.0 + alpha;
    return { (float) ((1.0 + c) / 2.0 / a0), (float) (-(1.0 + c) / a0), (float) ((1.0 + c) / 2.0 / a0),
             (float) (-2.0 * c / a0), (float) ((1.0 - alpha) / a0) };
}

Snowfall::Biquad Snowfall::makeLowPass (double frequency, double sr)
{
    const double w0 = kTwoPi * frequency / sr, alpha = std::sin (w0) / (2.0 * 0.7071), c = std::cos (w0), a0 = 1.0 + alpha;
    return { (float) ((1.0 - c) / 2.0 / a0), (float) ((1.0 - c) / a0), (float) ((1.0 - c) / 2.0 / a0),
             (float) (-2.0 * c / a0), (float) ((1.0 - alpha) / a0) };
}

void Snowfall::prepare (double newSampleRate, int numChannels, uint64_t newSeed)
{
    sampleRate = newSampleRate;
    seed = newSeed;
    channels.assign ((size_t) std::max (1, numChannels), Channel {});
    highPass = makeHighPass (kHighPassHz, sampleRate);
    lowPass = makeLowPass (kSweepFromHz, sampleRate);
    gainStep = (float) (1.0 / (0.030 * sampleRate));
    reset();
}

void Snowfall::reset()
{
    for (size_t ch = 0; ch < channels.size(); ++ch)
    {
        auto& c = channels[ch];
        c.random.seed (seed * 0x9e3779b97f4a7c15ull + ch + 101);
        for (auto& st : c.highPassState)
            std::fill (std::begin (st), std::end (st), 0.0f);
        std::fill (std::begin (c.lowPassState), std::end (c.lowPassState), 0.0f);
    }
    gain = gainTarget;
    counter = 0;
}

void Snowfall::setEnabled (bool shouldBeOn, bool immediately)
{
    gainTarget = shouldBeOn ? 1.0f : 0.0f;
    if (immediately)
        gain = gainTarget;
}

void Snowfall::process (float* const* audio, int numChannels, int numSamples, const float* amount)
{
    if (gain <= 0.0f && gainTarget <= 0.0f)
        return; // off: the audio is untouched

    numChannels = std::min (numChannels, (int) channels.size());
    for (int i = 0; i < numSamples; ++i)
    {
        if (counter-- <= 0)
        {
            // Sweep: brighter as Amount rises. The level is set against the passband width, so loudness
            // follows Amount rather than jumping as the filter opens.
            counter = kCoefficientInterval - 1;
            const double a = std::clamp ((double) amount[i], 0.0, 1.0);
            const double cutoff = std::min (kSweepFromHz * std::pow (kSweepToHz / kSweepFromHz, a), 0.45 * sampleRate);
            lowPass = makeLowPass (cutoff, sampleRate);
            const double bandwidth = std::max (cutoff - kHighPassHz, 300.0) / (0.5 * sampleRate);
            level = (float) (kFullLevel / (0.577 * std::sqrt (bandwidth)));
        }

        gain = gain < gainTarget ? std::min (gainTarget, gain + gainStep) : std::max (gainTarget, gain - gainStep);
        const float scale = std::clamp (amount[i], 0.0f, 1.0f) * level * gain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            float x = (float) (2.0 * c.random.uniform() - 1.0);
            x = highPass.run (x, c.highPassState[0]);
            x = highPass.run (x, c.highPassState[1]);
            x = lowPass.run (x, c.lowPassState);
            audio[ch][i] += x * scale;
        }
    }
}
} // namespace gf
