#include "Fx.h"
#include <algorithm>
#include <cmath>

namespace gf
{
namespace
{
// Freeverb tunings at 44.1 kHz; the right channel is offset for width.
constexpr int kCombTunings[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
constexpr int kAllpassTunings[4] = { 556, 441, 341, 225 };
constexpr int kStereoSpread = 23;
constexpr float kReverbInputGain = 0.015f;
constexpr float kReverbOutputGain = 3.0f;
constexpr double kMaxDelaySeconds = 2.0;
constexpr double kDelayGlideSeconds = 0.05;
constexpr double kTwoPi = 6.283185307179586;
} // namespace

void FxChain::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    const double scale = sampleRate / 44100.0;
    channels.assign ((size_t) std::max (1, numChannels), Channel {});
    for (size_t ch = 0; ch < channels.size(); ++ch)
    {
        auto& c = channels[ch];
        const int spread = ch == 1 ? kStereoSpread : 0;
        for (int i = 0; i < 8; ++i)
            c.combs[i].buffer.assign ((size_t) std::lround ((kCombTunings[i] + spread) * scale), 0.0f);
        for (int i = 0; i < 4; ++i)
            c.allpasses[i].buffer.assign ((size_t) std::lround ((kAllpassTunings[i] + spread) * scale), 0.0f);
        c.delay.assign ((size_t) std::ceil (kMaxDelaySeconds * sampleRate) + 2, 0.0f);
    }
    delayGlide = 1.0 - std::exp (-1.0 / (kDelayGlideSeconds * sampleRate));
    loopLowpass = (float) (1.0 - std::exp (-kTwoPi * 4000.0 / sampleRate));
    setCharacter (fx);
    reset();
}

void FxChain::reset()
{
    for (auto& c : channels)
    {
        for (auto& comb : c.combs)
        {
            std::fill (comb.buffer.begin(), comb.buffer.end(), 0.0f);
            comb.pos = 0;
            comb.store = 0.0f;
        }
        for (auto& ap : c.allpasses)
        {
            std::fill (ap.buffer.begin(), ap.buffer.end(), 0.0f);
            ap.pos = 0;
        }
        std::fill (c.delay.begin(), c.delay.end(), 0.0f);
        c.delayWrite = 0;
        c.delayLowpass = 0.0f;
        c.delayCurrent = delayTarget;
    }
}

void FxChain::setCharacter (const FxCharacter& character)
{
    fx = character;
    combFeedback = 0.7f + 0.28f * std::clamp (fx.reverbSize, 0.0f, 1.0f);
    // Longer rooms build up more energy (comb power ~ 1 / (1 - g^2)); keep the level as at size 0.5.
    constexpr float referenceFeedback = 0.84f;
    sizeGain = std::sqrt ((1.0f - combFeedback * combFeedback) / (1.0f - referenceFeedback * referenceFeedback));
    damp1 = 0.4f * std::clamp (fx.reverbDamp, 0.0f, 1.0f);
    damp2 = 1.0f - damp1;
    delayTarget = std::clamp ((double) fx.delaySeconds, 0.001, kMaxDelaySeconds) * sampleRate;
}

float FxChain::processSample (int channel, float in)
{
    auto& c = channels[(size_t) channel];
    float out = 0.0f;

    if (fx.reverbLevel > 0.0f)
    {
        const float input = in * kReverbInputGain;
        float sum = 0.0f;
        for (auto& comb : c.combs)
        {
            const float y = comb.buffer[comb.pos];
            comb.store = y * damp2 + comb.store * damp1;
            comb.buffer[comb.pos] = input + comb.store * combFeedback;
            comb.pos = (comb.pos + 1) % comb.buffer.size();
            sum += y;
        }
        for (auto& ap : c.allpasses)
        {
            const float buffered = ap.buffer[ap.pos];
            ap.buffer[ap.pos] = sum + buffered * 0.5f;
            ap.pos = (ap.pos + 1) % ap.buffer.size();
            sum = buffered - sum;
        }
        out += sum * kReverbOutputGain * sizeGain * fx.reverbLevel;
    }

    if (fx.delayLevel > 0.0f)
    {
        c.delayCurrent += (delayTarget - c.delayCurrent) * delayGlide;
        const auto size = (double) c.delay.size();
        double readPos = (double) c.delayWrite - c.delayCurrent;
        if (readPos < 0.0)
            readPos += size;
        const auto i0 = (size_t) readPos;
        const auto i1 = (i0 + 1) % c.delay.size();
        const auto frac = (float) (readPos - (double) i0);
        const float y = c.delay[i0] + (c.delay[i1] - c.delay[i0]) * frac;

        c.delayLowpass += (y - c.delayLowpass) * loopLowpass;
        c.delay[c.delayWrite] = in + c.delayLowpass * fx.delayFeedback;
        c.delayWrite = (c.delayWrite + 1) % c.delay.size();
        out += y * fx.delayLevel;
    }

    return out;
}
} // namespace gf
