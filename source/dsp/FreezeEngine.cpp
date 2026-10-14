#include "FreezeEngine.h"
#include <algorithm>

namespace gf
{
namespace
{
constexpr double kHalfPi = 1.5707963267948966;
constexpr double kTwoPi = 6.283185307179586;
} // namespace

void FreezeEngine::prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels)
{
    sampleRate = newSampleRate;
    segmentLength = std::max (1, (int) std::lround (kSegmentSeconds * sampleRate));
    bufferSegments = (int) std::ceil (kBufferSeconds / kSegmentSeconds);
    grainLength = std::max (2, (int) std::lround (kGrainSeconds * sampleRate));

    window.resize ((size_t) grainLength);
    for (int i = 0; i < grainLength; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / (grainLength - 1)));

    channelState.assign ((size_t) std::max (1, numChannels), Channel {});
    for (auto& c : channelState)
    {
        c.staging.assign ((size_t) segmentLength, 0.0f);
        c.buffer.assign ((size_t) (segmentLength * bufferSegments), 0.0f);
        c.energy.assign ((size_t) bufferSegments, 0.0f);
    }
    reset();
}

void FreezeEngine::reset()
{
    stagingFill = 0;
    sampleCounter = 0;
    for (size_t ch = 0; ch < channelState.size(); ++ch)
    {
        auto& c = channelState[ch];
        std::fill (c.staging.begin(), c.staging.end(), 0.0f);
        std::fill (c.buffer.begin(), c.buffer.end(), 0.0f);
        std::fill (c.energy.begin(), c.energy.end(), 0.0f);
        c.committedSegments = 0;
        for (auto& g : c.grains)
            g = Grain {};
        // Each channel draws its own grain positions, so left and right decorrelate.
        c.random.seed (seed * 0x9e3779b97f4a7c15ull + ch + 1);
        c.nextSpawn = 0;
    }
}

void FreezeEngine::setSeed (uint64_t newSeed)
{
    seed = newSeed;
    reset();
}

int64_t FreezeEngine::recordedSamples (int channel) const
{
    return channelState[(size_t) channel].committedSegments * segmentLength;
}

void FreezeEngine::commitSegment (Channel& c)
{
    double sum = 0.0;
    for (float x : c.staging)
        sum += (double) x * x;
    const auto segmentRms = (float) std::sqrt (sum / segmentLength);

    // Silence hold: quiet segments are dropped, so the buffer keeps the last sounding material.
    if (segmentRms < kSilenceRms)
        return;

    const auto slot = (size_t) (c.committedSegments % bufferSegments);
    std::copy (c.staging.begin(), c.staging.end(), c.buffer.begin() + (std::ptrdiff_t) (slot * (size_t) segmentLength));
    c.energy[slot] = segmentRms;
    ++c.committedSegments;
}

int FreezeEngine::nextSpawnInterval (Channel& c)
{
    const double jitter = 1.0 + kSpawnJitter * (2.0 * c.random.uniform() - 1.0);
    return std::max (1, (int) std::lround (kSpawnSeconds * sampleRate * jitter));
}

void FreezeEngine::spawnGrain (Channel& c)
{
    // Candidates: segments from the last kSelectSeconds of committed material, leaving out the newest
    // ones so that a whole grain fits before the write head.
    const int64_t excluded = (grainLength / 2 + segmentLength - 1) / segmentLength;
    const int64_t windowSegments = (int64_t) std::lround (kSelectSeconds / kSegmentSeconds);
    const int64_t last = c.committedSegments - excluded;            // exclusive
    const int64_t first = std::max<int64_t> ((grainLength / 2 + segmentLength - 1) / segmentLength, c.committedSegments - windowSegments);
    if (last <= first)
        return;

    float loudest = 0.0f;
    for (int64_t s = first; s < last; ++s)
        loudest = std::max (loudest, c.energy[(size_t) (s % bufferSegments)]);

    // Pick uniformly among the segments loud enough (favours attacks and bodies over quiet tails).
    const float threshold = kSelectThreshold * loudest;
    int eligible = 0;
    for (int64_t s = first; s < last; ++s)
        eligible += c.energy[(size_t) (s % bufferSegments)] >= threshold ? 1 : 0;

    int pick = c.random.below (eligible);
    int64_t chosen = first;
    for (int64_t s = first; s < last; ++s)
        if (c.energy[(size_t) (s % bufferSegments)] >= threshold && pick-- == 0)
        {
            chosen = s;
            break;
        }

    // Use a free voice, or steal the one that has played longest.
    Grain* voice = &c.grains[0];
    for (auto& g : c.grains)
    {
        if (! g.active)
        {
            voice = &g;
            break;
        }
        if (g.age > voice->age)
            voice = &g;
    }

    voice->start = chosen * segmentLength + c.random.below (segmentLength) - grainLength / 2;
    voice->age = 0;
    voice->active = true;
}

float FreezeEngine::renderGrains (Channel& c)
{
    const auto capacity = (int64_t) c.buffer.size();
    float sum = 0.0f;
    for (auto& g : c.grains)
    {
        if (! g.active)
            continue;
        sum += c.buffer[(size_t) ((g.start + g.age) % capacity)] * window[(size_t) g.age];
        if (++g.age >= grainLength)
            g.active = false;
    }
    return sum * kWetGain;
}

void FreezeEngine::process (float* const* channels, int numChannels, int numSamples, const float* freezeAmount)
{
    numChannels = std::min (numChannels, (int) channelState.size());

    bool bypass = true;
    for (int i = 0; i < numSamples && bypass; ++i)
        bypass = freezeAmount[i] <= 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            channelState[(size_t) ch].staging[(size_t) stagingFill] = channels[ch][i];

        if (++stagingFill == segmentLength)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                commitSegment (channelState[(size_t) ch]);
            stagingFill = 0;
        }

        if (! bypass)
        {
            const double k = freezeAmount[i] * kHalfPi;
            const auto dryGain = (float) std::cos (k);
            const auto wetGain = (float) std::sin (k);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = channelState[(size_t) ch];
                if (sampleCounter >= c.nextSpawn)
                {
                    spawnGrain (c);
                    c.nextSpawn += nextSpawnInterval (c);
                }
                const float wet = renderGrains (c);
                channels[ch][i] = channels[ch][i] * dryGain + wet * wetGain;
            }
        }

        ++sampleCounter;
    }

    // While bypassed, capture keeps running but no grains play; the cloud restarts immediately when raised.
    if (bypass)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channelState[(size_t) ch];
            for (auto& g : c.grains)
                g.active = false;
            c.nextSpawn = sampleCounter;
        }
}
} // namespace gf
