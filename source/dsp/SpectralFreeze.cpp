#include "SpectralFreeze.h"
#include <algorithm>

namespace gf
{
namespace
{
constexpr double kFrameSeconds = 0.3; // long frames resolve close pitches and average over several notes
constexpr float kSilenceRms = 0.001f; // -60 dBFS: silent frames don't change the spectrum
constexpr double kTwoPi = 6.283185307179586;

float wrapPhase (float x)
{
    return x - (float) kTwoPi * std::round (x / (float) kTwoPi);
}
} // namespace

void SpectralFreeze::prepare (double newSampleRate, uint64_t newSeed)
{
    sampleRate = newSampleRate;
    seed = newSeed;
    size = 1;
    while (size < (int) (kFrameSeconds * sampleRate))
        size <<= 1;
    hop = size / 4;
    fft.prepare (size);

    window.resize ((size_t) size);
    for (int i = 0; i < size; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / size));

    input.assign ((size_t) size, 0.0f);
    power.assign ((size_t) size / 2 + 1, 0.0f);
    lastPhase.assign (power.size(), 0.0f);
    frequency.assign (power.size(), 0.0f);
    frame.assign ((size_t) size, {});
    peaks.reserve (power.size());
    peakPower.reserve (power.size());
    setSmoothingSeconds (smoothingSeconds);
    reset();
}

void SpectralFreeze::reset()
{
    std::fill (input.begin(), input.end(), 0.0f);
    std::fill (power.begin(), power.end(), 0.0f);
    std::fill (lastPhase.begin(), lastPhase.end(), 0.0f);
    for (size_t k = 0; k < frequency.size(); ++k)
        frequency[k] = (float) (kTwoPi * (double) k / size);
    random.seed (seed);
    for (auto& p : partials)
        p = Partial {};
    counter = inputPos = 0;
    hasSpectrum = previousAnalysed = false;
    targetRms = 0.0f;
}

void SpectralFreeze::setSmoothingSeconds (double seconds)
{
    smoothingSeconds = std::max (0.05, seconds);
    smoothing = 1.0 - std::exp (-(double) hop / (smoothingSeconds * sampleRate));
}

float SpectralFreeze::processSample (float in)
{
    input[(size_t) inputPos] = in;
    inputPos = (inputPos + 1) % size;

    if (++counter == hop)
    {
        counter = 0;
        analyseFrame();
        updatePartials();
    }

    float out = 0.0f;
    for (auto& p : partials)
    {
        if (! p.active)
            continue;
        out += p.amplitude * p.phasor.imag();
        p.phasor *= p.rotor;
        p.amplitude += p.step;
    }
    return out;
}

void SpectralFreeze::analyseFrame()
{
    if (hold)
    {
        previousAnalysed = false;
        return;
    }

    // Last `size` input samples, oldest first, windowed
    double sum = 0.0;
    for (int i = 0; i < size; ++i)
    {
        const float x = input[(size_t) ((inputPos + i) % size)];
        sum += (double) x * x;
        frame[(size_t) i] = { x * window[(size_t) i], 0.0f };
    }
    const auto frameRms = (float) std::sqrt (sum / size);
    if (frameRms < kSilenceRms)
    {
        previousAnalysed = false;
        return; // silence hold, like the grain capture
    }

    fft.perform (frame.data(), false);

    // First sounding frame sets the spectrum directly; later frames are averaged in
    const float a = hasSpectrum ? (float) smoothing : 1.0f;
    for (size_t k = 0; k < power.size(); ++k)
    {
        power[k] += a * (std::norm (frame[k]) - power[k]);

        // True frequency of this bin from the phase change since the previous frame
        const float phase = std::arg (frame[k]);
        if (previousAnalysed)
        {
            const float binFrequency = (float) (kTwoPi * (double) k / size);
            const float deviation = wrapPhase (phase - lastPhase[k] - binFrequency * (float) hop);
            frequency[k] += a * (binFrequency + deviation / (float) hop - frequency[k]);
        }
        lastPhase[k] = phase;
    }
    targetRms += a * (frameRms - targetRms);
    previousAnalysed = true;
    hasSpectrum = true;
}

void SpectralFreeze::updatePartials()
{
    if (! hasSpectrum)
        return;

    // Peaks, strongest first. A peak closer than about a quarter-tone to a stronger one is merged into it
    // (its region's power is added), so a smeared or repeated note becomes one partial.
    const int half = size / 2;
    peaks.clear();
    for (int k = 1; k < half; ++k)
        if (power[(size_t) k] > power[(size_t) k - 1] && power[(size_t) k] >= power[(size_t) k + 1])
            peaks.push_back (k);
    std::sort (peaks.begin(), peaks.end(), [this] (int x, int y) { return power[(size_t) x] > power[(size_t) y]; });

    size_t kept = 0;
    peakPower.clear();
    for (int k : peaks)
    {
        const int spacing = std::max (6, (int) (0.015f * (float) k));
        size_t owner = kept;
        for (size_t j = 0; j < kept; ++j)
            if (std::abs (peaks[j] - k) < spacing)
                owner = j;
        // Power of the peak's own neighbourhood (main lobe)
        float regionPower = 0.0f;
        for (int b = std::max (1, k - 2); b <= std::min (half - 1, k + 2); ++b)
            regionPower += power[(size_t) b];
        if (owner < kept)
            peakPower[owner] += regionPower;
        else if (kept < (size_t) kMaxPartials)
        {
            peaks[kept++] = k;
            peakPower.push_back (regionPower);
        }
    }

    // Amplitudes in proportion to sqrt(power), scaled so the note has the input's level
    double totalPower = 0.0;
    for (size_t i = 0; i < kept; ++i)
        totalPower += peakPower[i];
    const double scale = totalPower > 0.0 ? targetRms * std::sqrt (2.0 / totalPower) : 0.0;

    // Match to the running oscillators by frequency so their phase continues; fade the rest out
    bool used[kMaxPartials] {};
    for (size_t i = 0; i < kept; ++i)
    {
        const float f = frequency[(size_t) peaks[i]];
        const float target = (float) (scale * std::sqrt ((double) peakPower[i]));
        int best = -1;
        for (int j = 0; j < kMaxPartials; ++j)
            if (partials[j].active && ! used[j] && std::abs (partials[j].frequency - f) < 0.03f * f
                && (best < 0 || std::abs (partials[j].frequency - f) < std::abs (partials[best].frequency - f)))
                best = j;
        if (best < 0)
            for (int j = 0; j < kMaxPartials && best < 0; ++j)
                if (! partials[j].active && ! used[j])
                {
                    best = j;
                    partials[j].amplitude = 0.0f;
                    partials[j].phasor = std::polar (1.0f, (float) (kTwoPi * random.uniform()));
                }
        if (best < 0)
            continue;
        auto& p = partials[best];
        used[best] = true;
        p.active = true;
        p.frequency = f;
        p.rotor = std::polar (1.0f, f);
        p.target = target;
    }
    for (int j = 0; j < kMaxPartials; ++j)
    {
        auto& p = partials[j];
        if (! used[j])
            p.target = 0.0f;
        p.phasor /= std::abs (p.phasor); // keep the oscillator on the unit circle
        p.step = (p.target - p.amplitude) / (float) hop;
        if (! used[j] && p.amplitude <= 0.0f)
            p.active = false;
    }
}
} // namespace gf
