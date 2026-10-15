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
    grainsSpawned = grainsReversed = 0;
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
        c.spectral.prepare (sampleRate, seed * 0x2545f4914f6cdd1dull + ch + 1);
        c.spectral.setHold (hold);
        c.nextSpawn = 0;
        c.reverseAccumulator = 0.5f;
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
    // The Grain knob also sets how fast the spectral freeze follows new notes: 0.3 s to 4 s
    const double t = std::log (grainSeconds / 0.040) / std::log (0.500 / 0.040);
    for (auto& c : channelState)
        c.spectral.setSmoothingSeconds (0.3 * std::pow (4.0 / 0.3, t));
}

void FreezeEngine::setDensity (double grainsOverlapping)
{
    density = std::clamp (grainsOverlapping, 2.0, 16.0);
}

void FreezeEngine::setMaxReverseShare (float share)
{
    maxReverseShare = std::clamp (share, 0.0f, 1.0f);
}

double FreezeEngine::reversedShare() const
{
    return grainsSpawned > 0 ? (double) grainsReversed / (double) grainsSpawned : 0.0;
}

void FreezeEngine::setHold (bool shouldHold)
{
    hold = shouldHold;
    for (auto& c : channelState)
        c.spectral.setHold (shouldHold);
}

void FreezeEngine::setMaxSpectralShare (float share)
{
    maxSpectralShare = std::clamp (share, 0.0f, 1.0f);
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
    // While Hold is on nothing new is recorded, so the grains keep playing what was captured.
    if (hold || segmentRms < kSilenceRms)
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

void FreezeEngine::spawnGrain (Channel& c, float amount)
{
    const int grainLength = std::max (2, (int) std::lround (grainSeconds * sampleRate));
    const int64_t halfGrainSegments = (grainLength / 2 + segmentLength - 1) / segmentLength;

    // Direction first. Some grains play backwards, more of them as Amount rises, so at full Amount attacks
    // and decays blend into one directionless wash. Reversed grains are spread evenly between forward ones
    // (error diffusion, not coin flips) so runs of one direction don't leave holes.
    c.reverseAccumulator += maxReverseShare * amount;
    const bool reverse = c.reverseAccumulator >= 1.0f;
    if (reverse)
        c.reverseAccumulator -= 1.0f;

    // Candidates: segments from the last kSelectSeconds of committed material, leaving out the newest
    // ones so that a whole grain fits before the write head (a reversed grain may sit up to half a grain
    // later, see below, so it leaves out twice as many).
    const int64_t excluded = halfGrainSegments * (reverse ? 2 : 1);
    const int64_t windowSegments = (int64_t) std::lround (kSelectSeconds / kSegmentSeconds);
    const int64_t last = c.committedSegments - excluded;            // exclusive
    const int64_t first = std::max<int64_t> (halfGrainSegments, c.committedSegments - windowSegments);
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

    // A forward grain is centred on the chosen spot, so the loud part after it fills the second half of
    // the grain. A reversed grain plays that loud part backwards, ending at the chosen spot; it is moved
    // later by the length of the loud run so the loud part again fills the second half.
    int64_t loudRun = 0;
    if (reverse)
        for (int64_t s = chosen + 1; s < c.committedSegments && loudRun < halfGrainSegments; ++s, ++loudRun)
            if (c.energy[(size_t) (s % bufferSegments)] < threshold)
                break;
    voice->start = chosen * segmentLength + c.random.below (segmentLength) - grainLength / 2 + loudRun * segmentLength;

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
    voice->reverse = reverse;
    voice->active = true;
    ++grainsSpawned;
    grainsReversed += reverse ? 1 : 0;
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
        const int offset = g.reverse ? g.length - 1 - g.age : g.age;
        sum += c.buffer[(size_t) ((g.start + offset) % capacity)] * w * g.gain;
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
        // The spectral freeze always listens, so it is ready the moment Amount goes up
        float spectralOut[2] {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channelState[(size_t) ch];
            c.staging[(size_t) stagingFill] = channels[ch][i];
            spectralOut[std::min (ch, 1)] = c.spectral.processSample (channels[ch][i]);
        }

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

            // From 50 % to 100 % Amount the frozen sound crossfades from grains into the spectral freeze
            const float t = std::clamp ((freezeAmount[i] - 0.5f) * 2.0f, 0.0f, 1.0f);
            const float spectralShare = maxSpectralShare * t * t * (3.0f - 2.0f * t);
            const auto grainGain = (float) std::cos (spectralShare * kHalfPi);
            const auto spectralGain = (float) std::sin (spectralShare * kHalfPi);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = channelState[(size_t) ch];
                if (sampleCounter >= c.nextSpawn)
                {
                    spawnGrain (c, freezeAmount[i]);
                    c.nextSpawn += nextSpawnInterval (c);
                }
                float wet = renderGrains (c);
                if (spectralShare > 0.0f)
                    wet = wet * grainGain + spectralOut[std::min (ch, 1)] * spectralGain;
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
