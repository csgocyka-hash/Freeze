#pragma once
// Spectral freeze: keeps a running average of the input's spectrum (power and true frequency per bin),
// finds its partials and plays them on a bank of continuous sine oscillators. Attacks, decays, rhythm and
// grain texture disappear; pitch, harmonics and tone balance remain, as one steady note.
// One instance per channel. JUCE-free; everything is allocated in prepare().
#include "Fft.h"
#include "Random.h"
#include <complex>
#include <cstdint>
#include <vector>

namespace gf
{
class SpectralFreeze
{
public:
    void prepare (double sampleRate, uint64_t seed);
    void reset();

    // How long the frozen note takes to follow new input (seconds, time constant).
    void setSmoothingSeconds (double seconds);
    // While held, the spectrum is not updated.
    void setHold (bool shouldHold) { hold = shouldHold; }

    // Feeds one input sample and returns one output sample. Deterministic, block-size independent.
    float processSample (float input);

private:
    static constexpr int kMaxPartials = 40;

    struct Partial
    {
        std::complex<float> phasor { 1.0f, 0.0f }, rotor { 1.0f, 0.0f };
        float frequency = 0.0f;           // radians per sample
        float amplitude = 0.0f, target = 0.0f, step = 0.0f;
        bool active = false;
    };

    void analyseFrame();
    void updatePartials();

    Fft fft;
    Random random;
    uint64_t seed = 1;
    int size = 16384, hop = 4096, counter = 0, inputPos = 0;
    double sampleRate = 48000.0, smoothingSeconds = 1.0, smoothing = 0.1;
    bool hold = false, hasSpectrum = false, previousAnalysed = false;
    float targetRms = 0.0f;

    std::vector<float> window, input, power, lastPhase, frequency;
    std::vector<std::complex<float>> frame;
    std::vector<int> peaks;
    std::vector<float> peakPower;
    Partial partials[kMaxPartials];
};
} // namespace gf
