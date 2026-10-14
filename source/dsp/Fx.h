#pragma once
// Reverb + delay for the frozen signal. JUCE-free; buffers are allocated in prepare().
#include <vector>

namespace gf
{
struct FxCharacter
{
    float reverbSize = 0.5f, reverbDamp = 0.3f, reverbLevel = 1.0f;
    float delaySeconds = 0.3f, delayFeedback = 0.0f, delayLevel = 0.0f;
};

class FxChain
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setCharacter (const FxCharacter& character);

    // Returns only the effect signal (reverb + delay at their levels), no dry.
    float processSample (int channel, float in);

private:
    struct Comb
    {
        std::vector<float> buffer;
        size_t pos = 0;
        float store = 0.0f;
    };

    struct Allpass
    {
        std::vector<float> buffer;
        size_t pos = 0;
    };

    struct Channel
    {
        Comb combs[8];
        Allpass allpasses[4];
        std::vector<float> delay;
        size_t delayWrite = 0;
        float delayLowpass = 0.0f;
        double delayCurrent = 0.0; // samples, glides towards the target
    };

    double sampleRate = 48000.0;
    FxCharacter fx;
    float combFeedback = 0.84f, damp1 = 0.12f, damp2 = 0.88f, sizeGain = 1.0f;
    double delayTarget = 0.0, delayGlide = 0.0;
    float loopLowpass = 0.4f;
    std::vector<Channel> channels;
};
} // namespace gf
