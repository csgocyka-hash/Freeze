#include "Tone.h"
#include <algorithm>
#include <cmath>

namespace gf
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;
constexpr double kChorusBaseSeconds = 0.014, kChorusDepthSeconds = 0.004;

// RBJ cookbook filters
void highPass (double f, double sr, float& b0, float& b1, float& b2, float& a1, float& a2)
{
    const double w = kTwoPi * f / sr, alpha = std::sin (w) / (2.0 * 0.7071), c = std::cos (w), a0 = 1.0 + alpha;
    b0 = (float) ((1.0 + c) / 2.0 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - alpha) / a0);
}

void shelf (bool high, double f, double db, double sr, float& b0, float& b1, float& b2, float& a1, float& a2)
{
    const double A = std::pow (10.0, db / 40.0), w = kTwoPi * f / sr, c = std::cos (w), s = std::sin (w);
    const double alpha = s / 2.0 * std::sqrt (2.0), k = 2.0 * std::sqrt (A) * alpha;
    double nb0, nb1, nb2, na0, na1, na2;
    if (high)
    {
        nb0 = A * ((A + 1) + (A - 1) * c + k); nb1 = -2 * A * ((A - 1) + (A + 1) * c); nb2 = A * ((A + 1) + (A - 1) * c - k);
        na0 = (A + 1) - (A - 1) * c + k; na1 = 2 * ((A - 1) - (A + 1) * c); na2 = (A + 1) - (A - 1) * c - k;
    }
    else
    {
        nb0 = A * ((A + 1) - (A - 1) * c + k); nb1 = 2 * A * ((A - 1) - (A + 1) * c); nb2 = A * ((A + 1) - (A - 1) * c - k);
        na0 = (A + 1) + (A - 1) * c + k; na1 = -2 * ((A - 1) + (A + 1) * c); na2 = (A + 1) + (A - 1) * c - k;
    }
    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0); a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}
} // namespace

float Tone::run (const Biquad& f, float x, float* s)
{
    const float y = f.b0 * x + f.b1 * s[0] + f.b2 * s[1] - f.a1 * s[2] - f.a2 * s[3];
    s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
    return y;
}

void Tone::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    channels.assign ((size_t) std::max (1, numChannels), Channel {});
    for (auto& c : channels)
        c.delay.assign ((size_t) std::ceil ((kChorusBaseSeconds + kChorusDepthSeconds) * sampleRate) + 4, 0.0f);
    setCharacter (tone);
    reset();
}

void Tone::reset()
{
    for (auto& c : channels)
    {
        std::fill (std::begin (c.lowCut), std::end (c.lowCut), 0.0f);
        std::fill (std::begin (c.lowShelf), std::end (c.lowShelf), 0.0f);
        std::fill (std::begin (c.highShelf), std::end (c.highShelf), 0.0f);
        std::fill (c.delay.begin(), c.delay.end(), 0.0f);
        c.write = 0;
    }
    chorusPhase = 0.0;
}

void Tone::setCharacter (const ToneCharacter& t)
{
    tone = t;
    if (tone.lowCutHz > 0.0f)
        highPass (tone.lowCutHz, sampleRate, lowCut.b0, lowCut.b1, lowCut.b2, lowCut.a1, lowCut.a2);
    shelf (false, tone.lowShelfHz, tone.lowShelfDb, sampleRate, lowShelf.b0, lowShelf.b1, lowShelf.b2, lowShelf.a1, lowShelf.a2);
    shelf (true, std::min ((double) tone.highShelfHz, 0.45 * sampleRate), tone.highShelfDb, sampleRate,
           highShelf.b0, highShelf.b1, highShelf.b2, highShelf.a1, highShelf.a2);
}

void Tone::process (float* const* audio, int numChannels, int numSamples)
{
    numChannels = std::min (numChannels, (int) channels.size());
    const bool eqLow = tone.lowCutHz > 0.0f, eqLowShelf = tone.lowShelfDb != 0.0f, eqHigh = tone.highShelfDb != 0.0f;
    const bool saturate = tone.saturation > 0.0f, chorus = tone.chorusMix > 0.0f, widen = tone.width != 1.0f && numChannels == 2;
    if (! (eqLow || eqLowShelf || eqHigh || saturate || chorus || widen))
        return; // neutral: untouched

    const double phaseStep = (double) tone.chorusRate / sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            float x = audio[ch][i];
            if (eqLow)
                x = run (lowCut, x, c.lowCut);
            if (eqLowShelf)
                x = run (lowShelf, x, c.lowShelf);
            if (eqHigh)
                x = run (highShelf, x, c.highShelf);
            if (saturate) // gentle tanh curve blended in, with a little make-up gain so the level stays put
                x += tone.saturation * (std::tanh (2.0f * x) * 0.5f - x + 0.12f * x);
            if (chorus)
            {
                // A modulated copy 10-18 ms behind, left and right a quarter-cycle apart for width
                const auto size = (int) c.delay.size();
                c.delay[(size_t) c.write] = x;
                const double lfo = std::sin (kTwoPi * (chorusPhase + 0.25 * ch));
                const double d = (kChorusBaseSeconds + kChorusDepthSeconds * lfo) * sampleRate;
                double readPos = c.write - d;
                while (readPos < 0.0)
                    readPos += size;
                const auto i0 = (int) readPos;
                const auto frac = (float) (readPos - i0);
                const float delayed = c.delay[(size_t) i0] + (c.delay[(size_t) ((i0 + 1) % size)] - c.delay[(size_t) i0]) * frac;
                c.write = (c.write + 1) % size;
                x = x * (1.0f - 0.5f * tone.chorusMix) + delayed * tone.chorusMix;
            }
            audio[ch][i] = x;
        }
        if (chorus)
            chorusPhase = std::fmod (chorusPhase + phaseStep, 1.0);
        if (widen)
        {
            const float mid = 0.5f * (audio[0][i] + audio[1][i]), side = 0.5f * (audio[0][i] - audio[1][i]) * tone.width;
            audio[0][i] = mid + side;
            audio[1][i] = mid - side;
        }
    }
}
} // namespace gf
