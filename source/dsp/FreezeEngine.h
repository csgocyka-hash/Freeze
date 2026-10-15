#pragma once
// Granular freeze: records the recent sounding input and plays it back as a cloud of overlapping grains.
// JUCE-free and real-time safe: everything is allocated in prepare().
#include "Fx.h"
#include "Random.h"
#include "SpectralFloor.h"
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
constexpr int kMaxGrains = 64;
constexpr int kWindowTableSize = 4096;
constexpr double kSelectSeconds = 1.5;
constexpr float kSelectThreshold = 0.25f;
// Smooth mode freezes the notes' bodies, not their attacks: after each note start the flattened copy stays
// silent for kAttackSkipSeconds and fades back in over kAttackFadeSeconds.
constexpr double kAttackSkipSeconds = 0.010;
constexpr double kAttackFadeSeconds = 0.020;
// Smooth mode keeps the read positions of grains playing at the same time this far apart (two grains reading
// within ~50 ms of each other comb-filter, which sounds phasey), trying up to kSpacingTries positions.
constexpr double kMinReadSpacingSeconds = 0.040;
constexpr int kSpacingTries = 16;
// From kDiffusionStart Amount on, a growing share of new grains (all at full Amount) plays through a
// diffuser: four Schroeder all-passes (gain kDiffuserGain, mutually prime delays of about 5-19 ms) that
// smear what is left of grain edges and de-click dips. Whole grains are routed, so the cloud is never mixed
// with its own all-passed copy (which would comb-filter while the diffuser fades in).
constexpr float kDiffusionStart = 0.7f;
constexpr float kDiffuserGain = 0.6f;
constexpr int kDiffuserStages = 4;

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
    // get up to twice as long, about 6 overlap (setSmoothOverlap), and they read from a copy of the input whose note
    // envelopes (attack and decay) are flattened and whose note starts are faded in, so no pluck is left.
    void setMaxSmoothness (float amount);

    // How many grains overlap at full Amount (default 6; short-grain presets use 8)
    void setSmoothOverlap (double grainsOverlapping);

    // Level of the random-phase spectral floor under the grains in smooth mode (faded in with smoothness),
    // relative to the grains (default 0.25 = 25 %). It is a steady, seamless version of the material
    // (see SpectralFloor) that glues the grains together and fills the moments where they cancel out.
    void setFloorLevel (float level);

    // Grains spawned since reset() (diagnostic).
    int64_t grainsSpawnedCount() const { return grainsSpawned; }

    // Fraction of grains spawned since reset() that play backwards (diagnostic).
    double reversedShare() const;

    // Fraction of grains spawned since reset() that started reading within kMinReadSpacingSeconds of where a
    // playing grain of the same direction reads (diagnostic: such pairs comb-filter).
    double closeReadShare() const;

    // Share of grains sent through the diffuser at full Amount (0..1, default 1; from 70 % Amount up).
    void setMaxDiffusion (float amount);

    // Detune (0..1): each new grain is pitched randomly up or down by up to 50 cents x detune.
    void setDetune (float amount);

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
        double rate = 1.0;    // playback speed (detune); 1 = original pitch
        bool diffuse = false; // plays through the diffuser
        bool active = false;
    };

    struct Channel
    {
        std::vector<float> staging;   // current 10 ms segment being collected
        std::vector<float> buffer;    // circular capture buffer, whole segments
        std::vector<float> flat;      // the same audio with its note envelopes flattened (no attack or decay)
        float levelReference = 0.0f;  // slow average of the sounding level, what `flat` is scaled to
        std::vector<int64_t> segmentTime; // when each buffer segment was recorded (in segments), to find joins
        float flatEnvelope = 0.0f;    // peak envelope used for flattening (instant attack, 40 ms release)
        int sinceDip = 1 << 30;       // samples since the last note start or join (attack gap and fade-in)
        int pendingDip = -1;          // a note start found just past the end of the last flattened segment
        SpectralFloor floor;
        bool floorStale = true;  // the floor was silent (smooth mode off) and starts afresh when it returns
        float grainPower = 0.0f; // slow mean square of the grains, which the floor is scaled to
        std::vector<float> energy;    // RMS per buffer segment
        int64_t committedSegments = 0;
        Grain grains[kMaxGrains];
        Random random;
        float reverseAccumulator = 0.0f; // spreads reversed grains evenly between forward ones
        float diffuseAccumulator = 0.0f; // spreads diffused grains the same way
        std::vector<float> diffuser[kDiffuserStages]; // all-pass delay lines
        size_t diffuserPos[kDiffuserStages] {};
        int64_t nextSpawn = 0;
    };

    void commitSegment (Channel& c, int64_t segmentTime);
    void spawnGrain (Channel& c, float amount);
    float renderGrains (Channel& c, float& diffused);
    float diffuse (Channel& c, float x);
    static float sourceAt (const Channel& c, int64_t index, float flatMix);
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
    double smoothOverlap = 6.0;
    float maxDiffusion = 1.0f;
    float smoothness = 0.0f; // current, from Amount
    float floorLevel = 0.25f;
    float detune = 0.0f;
    void fillFloorFrame (Channel& c, int frame);
    float grainPowerCoeff = 0.0f;
    int64_t grainsSpawned = 0, grainsReversed = 0, grainsClose = 0;
    double readDistance (const Channel& c, int64_t readIndex, bool reverse, const Grain* ignore) const;
    FxChain fx;
    int stagingFill = 0;
    int64_t sampleCounter = 0;
    std::vector<float> window;
    uint64_t seed = 1;
    std::vector<Channel> channelState;
};
} // namespace gf
