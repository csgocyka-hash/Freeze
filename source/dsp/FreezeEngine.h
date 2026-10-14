#pragma once
// Granular freeze: records the recent sounding input and (Task 3) plays it back as a grain cloud.
// JUCE-free and real-time safe: everything is allocated in prepare().
#include <cstdint>
#include <vector>

namespace gf
{
constexpr double kBufferSeconds = 2.0;
constexpr double kSegmentSeconds = 0.010;
constexpr float kSilenceRms = 0.001f; // -60 dBFS

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
    struct Channel
    {
        std::vector<float> staging;   // current 10 ms segment being collected
        std::vector<float> buffer;    // circular capture buffer, whole segments
        std::vector<float> energy;    // RMS per buffer segment
        int64_t committedSegments = 0;
    };

    void commitSegment (Channel& c);

    double sampleRate = 48000.0;
    int segmentLength = 480;
    int bufferSegments = 200;
    int stagingFill = 0;
    uint64_t seed = 1;
    std::vector<Channel> channelState;
};
} // namespace gf
