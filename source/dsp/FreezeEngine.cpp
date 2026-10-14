#include "FreezeEngine.h"
#include <algorithm>
#include <cmath>

namespace gf
{
void FreezeEngine::prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels)
{
    sampleRate = newSampleRate;
    segmentLength = std::max (1, (int) std::lround (kSegmentSeconds * sampleRate));
    bufferSegments = (int) std::ceil (kBufferSeconds / kSegmentSeconds);

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
    for (auto& c : channelState)
    {
        std::fill (c.staging.begin(), c.staging.end(), 0.0f);
        std::fill (c.buffer.begin(), c.buffer.end(), 0.0f);
        std::fill (c.energy.begin(), c.energy.end(), 0.0f);
        c.committedSegments = 0;
    }
}

void FreezeEngine::setSeed (uint64_t newSeed)
{
    seed = newSeed;
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

void FreezeEngine::process (float* const* channels, int numChannels, int numSamples, const float* /*freezeAmount*/)
{
    numChannels = std::min (numChannels, (int) channelState.size());

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
    }
}
} // namespace gf
