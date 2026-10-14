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

    window.resize (kWindowTableSize + 1);
    for (int i = 0; i <= kWindowTableSize; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / kWindowTableSize));
    fx.prepare (sampleRate, numChannels);

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
    bypassed = false;
    fx.reset();
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

void FreezeEngine::setGrainSeconds (double seconds)
{
    grainSeconds = std::clamp (seconds, 0.040, 0.500);
}

void FreezeEngine::setDensity (double grainsOverlapping)
{
    density = std::clamp (grainsOverlapping, 2.0, 16.0);
}

void FreezeEngine::setFxCharacter (const FxCharacter& character)
{
    fx.setCharacter (character);
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

double FreezeEngine::spacingSeconds() const
{
    return std::min (grainSeconds / density, kMaxSpacingSeconds);
}

int FreezeEngine::nextSpawnInterval (Channel& c)
{
    const double jitter = 1.0 + kSpawnJitter * (2.0 * c.random.uniform() - 1.0);
    return std::max (1, (int) std::lround (spacingSeconds() * sampleRate * jitter));
}

void FreezeEngine::spawnGrain (Channel& c)
{
    const int grainLength = std::max (2, (int) std::lround (grainSeconds * sampleRate));

    // Candidates: segments from the last kSelectSeconds of committed material, leaving out the newest
    // ones so that a whole grain fits before the write head.
    const int64_t excluded = (grainLength / 2 + segmentLength - 1) / segmentLength;
    const int64_t windowSegments = (int64_t) std::lround (kSelectSeconds / kSegmentSeconds);
    const int64_t last = c.committedSegments - excluded;            // exclusive
    const int64_t first = std::max<int64_t> ((grainLength / 2 + segmentLength - 1) / segmentLength, c.committedSegments - windowSegments);
    if (last <= first)
        return;

    float loudest = 0.0f;
    double sumSquares = 0.0;
    for (int64_t s = first; s < last; ++s)
    {
        const float e = c.energy[(size_t) (s % bufferSegments)];
        loudest = std::max (loudest, e);
        sumSquares += (double) e * e;
    }
    const double targetRms = std::sqrt (sumSquares / (double) (last - first));

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

    // Level-match the grain: its windowed RMS is brought towards the recent input level (within +-6 dB),
    // so short grains on loud attacks are not louder than long grains over whole notes.
    const auto capacity = (int64_t) c.buffer.size();
    double grainSquares = 0.0, windowSquares = 0.0;
    for (int i = 0; i < grainLength; i += 4)
    {
        const double w = 0.5 - 0.5 * std::cos (kTwoPi * i / (grainLength - 1));
        const double x = c.buffer[(size_t) ((voice->start + i) % capacity)];
        grainSquares += w * w * x * x;
        windowSquares += w * w;
    }
    const double grainRms = std::sqrt (grainSquares / windowSquares);
    voice->gain = grainRms > 0.0 ? (float) std::clamp (targetRms / grainRms, 0.5, 2.0) : 1.0f;
    voice->age = 0;
    voice->length = grainLength;
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
        // Hann window from the table, linearly interpolated for any grain length
        const float phase = (float) g.age * (float) kWindowTableSize / (float) (g.length - 1);
        const auto idx = std::min ((int) phase, kWindowTableSize - 1);
        const float w = window[(size_t) idx] + (window[(size_t) idx + 1] - window[(size_t) idx]) * (phase - (float) idx);
        sum += c.buffer[(size_t) ((g.start + g.age) % capacity)] * w * g.gain;
        if (++g.age >= g.length)
            g.active = false;
    }
    // Power-sum normalisation for uncorrelated Hann grains at the actual overlap
    return sum * (float) (1.0 / std::sqrt (grainSeconds / spacingSeconds() * 0.375));
}

void FreezeEngine::process (float* const* channels, int numChannels, int numSamples, const float* freezeAmount,
                            const float* fxAmount)
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
                float wet = renderGrains (c);
                if (fxAmount != nullptr)
                {
                    const float f = fxAmount[i];
                    const float effect = fx.processSample (ch, wet);
                    wet = (wet + f * effect) / std::sqrt (1.0f + f * f);
                }
                channels[ch][i] = channels[ch][i] * dryGain + wet * wetGain;
            }
        }

        ++sampleCounter;
    }

    // While bypassed, capture keeps running but no grains or effects play; the cloud restarts
    // immediately when raised.
    if (bypass && ! bypassed)
        fx.reset();
    bypassed = bypass;
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
