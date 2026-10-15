#pragma once
// Tone stage for the frozen sound: low cut, low and high shelves, soft saturation, chorus and stereo width.
// Each preset sets its own character. Stages at their neutral values are skipped, so a neutral character
// leaves the audio bit-identical. JUCE-free; everything is allocated in prepare().
#include <vector>

namespace gf
{
struct ToneCharacter
{
    float lowCutHz = 0.0f;                         // 0 = off
    float lowShelfHz = 200.0f, lowShelfDb = 0.0f;
    float highShelfHz = 8000.0f, highShelfDb = 0.0f;
    float saturation = 0.0f;                       // 0..1 mix of a soft tanh curve
    float chorusMix = 0.0f, chorusRate = 0.2f;     // mix 0..1, rate in Hz
    float width = 1.0f;                            // stereo side gain (1 = unchanged)
};

class Tone
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setCharacter (const ToneCharacter&);
    void process (float* const* channels, int numChannels, int numSamples);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    struct Channel
    {
        float lowCut[4] {}, lowShelf[4] {}, highShelf[4] {};
        std::vector<float> delay;
        int write = 0;
    };

    static float run (const Biquad&, float x, float* state);

    double sampleRate = 48000.0;
    ToneCharacter tone;
    Biquad lowCut, lowShelf, highShelf;
    std::vector<Channel> channels;
    double chorusPhase = 0.0;
};
} // namespace gf
