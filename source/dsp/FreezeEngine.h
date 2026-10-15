#pragma once
// Granular freeze: records the recent sounding input and plays it back as a cloud of overlapping grains.
// JUCE-free and real-time safe: everything is allocated in prepare().
#include "Fx.h"
#include "Random.h"
#include "SpectralFreeze.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace gf
{
constexpr double kBufferSeconds = 4.0;
constexpr double kSegmentSeconds = 0.010;
constexpr float kSilenceRms = 0.001f; // -60 dBFS
constexpr double kGrainSeconds = 0.120; // default grain length
constexpr double kDensity = 6.0;         // default overlap: spacing = grain length / density
constexpr double kSpawnJitter = 0.25;
// Longest gap between grain starts. Long grains on short plucks would otherwise replay whole
// plucks with near-silence between them, so long grains overlap more instead.
constexpr double kMaxSpacingSeconds = 0.040;
constexpr int kMaxGrains = 48;
constexpr int kWindowTableSize = 4096;
constexpr double kSelectSeconds = 1.5;
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

    // Share of grains played backwards at full Amount (scaled by Amount below that). Default 0.5.
    void setMaxReverseShare (float share);

    // How far the cloud turns smooth near full Amount (0..1, default 1). From 50 % to 100 % Amount grains
    // get up to twice as long, about 16 overlap, and they read from a copy of the input whose note
    // envelopes (attack and decay) are flattened, so no pluck is left in them.
    void setMaxSmoothness (float amount);

    // Level of the high-passed spectral freeze layered in parallel with the grains near full Amount,
    // relative to the grains (default 0.2 = 20 %). It adds an airy, steady shimmer on top of the cloud.
    void setSpectralLevel (float level);

    // Grains spawned since reset() (diagnostic).
    int64_t grainsSpawnedCount() const { return grainsSpawned; }

    // Fraction of grains spawned since reset() that play backwards (diagnostic).
    double reversedShare() const;

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
        bool reverse = false;
        float flatMix = 0.0f; // how much the grain reads from the flattened copy
        bool active = false;
    };

    struct Channel
    {
        std::vector<float> staging;   // current 10 ms segment being collected
        std::vector<float> buffer;    // circular capture buffer, whole segments
        std::vector<float> flat;      // the same audio with its note envelopes flattened (no attack or decay)
        float levelReference = 0.0f;  // slow average of the sounding level, what `flat` is scaled to
        SpectralFreeze spectral;
        float hpState[2][4] {}; // two cascaded high-pass stages (x1, x2, y1, y2) for the spectral layer
        std::vector<float> energy;    // RMS per buffer segment
        int64_t committedSegments = 0;
        Grain grains[kMaxGrains];
        Random random;
        float reverseAccumulator = 0.0f; // spreads reversed grains evenly between forward ones
        int64_t nextSpawn = 0;
    };

    void commitSegment (Channel& c);
    void spawnGrain (Channel& c, float amount);
    float renderGrains (Channel& c);
    int nextSpawnInterval (Channel& c);
    double spacingSeconds() const;
    double currentGrainSeconds() const;
    void flattenSegment (Channel& c, int64_t segment);

    double sampleRate = 48000.0;
    int segmentLength = 480;
    int bufferSegments = 200;
    double grainSeconds = kGrainSeconds;
    double density = kDensity;
    bool bypassed = false;
    bool hold = false;
    float maxReverseShare = 0.5f;
    float maxSmoothness = 1.0f;
    float smoothness = 0.0f; // current, from Amount
    float spectralLevel = 0.2f;
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f; // 800 Hz high-pass, used twice (24 dB/oct)
    float highPass (Channel& c, float x);
    int64_t grainsSpawned = 0, grainsReversed = 0;
    FxChain fx;
    int stagingFill = 0;
    int64_t sampleCounter = 0;
    std::vector<float> window;
    uint64_t seed = 1;
    std::vector<Channel> channelState;
};
} // namespace gf
