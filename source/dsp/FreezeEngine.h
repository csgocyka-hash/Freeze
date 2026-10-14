#pragma once
// Granular freeze: records the recent sounding input and plays it back as a cloud of overlapping grains.
// JUCE-free and real-time safe: everything is allocated in prepare().
#include "Random.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace gf
{
constexpr double kBufferSeconds = 2.0;
constexpr double kSegmentSeconds = 0.010;
constexpr float kSilenceRms = 0.001f; // -60 dBFS
constexpr double kGrainSeconds = 0.120;
constexpr double kSpawnSeconds = 0.020;
constexpr double kSpawnJitter = 0.25;
constexpr int kMaxGrains = 16;
constexpr double kSelectSeconds = 1.0;
constexpr float kSelectThreshold = 0.25f;
// Power-sum normalisation for uncorrelated Hann grains at (grain / spawn) overlap.
inline const float kWetGain = (float) (1.0 / std::sqrt ((kGrainSeconds / kSpawnSeconds) * 0.375));

class FreezeEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSeed (uint64_t newSeed);

    // In place. freezeAmount holds the smoothed knob value (0..1) for each sample.
    void process (float* const* channels, int numChannels, int numSamples, const float* freezeAmount);

    // Total samples committed to the capture buffer of a channel since reset().
    int64_t recordedSamples (int channel) const;

private:
    struct Grain
    {
        int64_t start = 0; // absolute index into the committed material
        int age = 0;       // samples played so far
        bool active = false;
    };

    struct Channel
    {
        std::vector<float> staging;   // current 10 ms segment being collected
        std::vector<float> buffer;    // circular capture buffer, whole segments
        std::vector<float> energy;    // RMS per buffer segment
        int64_t committedSegments = 0;
        Grain grains[kMaxGrains];
        Random random;
        int64_t nextSpawn = 0;
    };

    void commitSegment (Channel& c);
    void spawnGrain (Channel& c);
    float renderGrains (Channel& c);
    int nextSpawnInterval (Channel& c);

    double sampleRate = 48000.0;
    int segmentLength = 480;
    int bufferSegments = 200;
    int grainLength = 5760;
    int stagingFill = 0;
    int64_t sampleCounter = 0;
    std::vector<float> window;
    uint64_t seed = 1;
    std::vector<Channel> channelState;
};
} // namespace gf
