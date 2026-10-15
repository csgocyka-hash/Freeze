#pragma once
// Granular freeze: records the recent sounding input and plays it back as a cloud of overlapping grains.
// JUCE-free and real-time safe: everything is allocated in prepare().
#include "Fx.h"
#include "Random.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace gf
{
constexpr double kBufferSeconds = 2.0;
constexpr double kSegmentSeconds = 0.010;
constexpr float kSilenceRms = 0.001f; // -60 dBFS
constexpr double kGrainSeconds = 0.120; // default grain length
constexpr double kDensity = 6.0;         // default overlap: spacing = grain length / density
constexpr double kSpawnJitter = 0.25;
// Longest gap between grain starts. Long grains on short plucks would otherwise replay whole
// plucks with near-silence between them, so long grains overlap more instead.
constexpr double kMaxSpacingSeconds = 0.040;
constexpr int kMaxGrains = 32;
constexpr int kWindowTableSize = 4096;
constexpr double kSelectSeconds = 1.0;
constexpr float kSelectThreshold = 0.25f;

class FreezeEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSeed (uint64_t newSeed);

    // New grains use these; grains already playing keep their length.
    void setGrainSeconds (double seconds);
    void setDensity (double grainsOverlapping);
    void setFxCharacter (const FxCharacter& character);

    // Hold: stop recording new input, so the grains keep playing the sound captured so far.
    void setHold (bool shouldHold);

    // In place. freezeAmount and fxAmount hold smoothed knob values (0..1) per sample; fxAmount may be null.
    void process (float* const* channels, int numChannels, int numSamples, const float* freezeAmount,
                  const float* fxAmount = nullptr);

    // Total samples committed to the capture buffer of a channel since reset().
    int64_t recordedSamples (int channel) const;

private:
    struct Grain
    {
        int64_t start = 0; // absolute index into the committed material
        int age = 0;       // samples played so far
        int length = 0;
        float gain = 1.0f; // brings the grain to the recent input level
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
    double spacingSeconds() const;

    double sampleRate = 48000.0;
    int segmentLength = 480;
    int bufferSegments = 200;
    double grainSeconds = kGrainSeconds;
    double density = kDensity;
    bool bypassed = false;
    bool hold = false;
    FxChain fx;
    int stagingFill = 0;
    int64_t sampleCounter = 0;
    std::vector<float> window;
    uint64_t seed = 1;
    std::vector<Channel> channelState;
};
} // namespace gf
