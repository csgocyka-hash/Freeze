#include "SpectralFloor.h"
#include <algorithm>
#include <cmath>

namespace gf
{
namespace
{
constexpr double kFrameSeconds = 0.6; // 32768 points at 48 kHz: a gathered partial is at most 0.7 Hz off
constexpr double kLowestHz = 30.0;    // nothing below (no DC or rumble)
constexpr double kLobeHz = 8.0;       // a partial's energy is gathered from +-8 Hz (short notes smear it that far)
constexpr float kPeakRatio = 30.0f;   // a partial stands 15 dB above the spectrum twice as far away
constexpr float kSilenceRms = 0.001f; // -60 dBFS
constexpr double kTwoPi = 6.283185307179586;
constexpr float kHalfPi = 1.5707963f;
} // namespace

void SpectralFloor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    size = 1;
    while (size < (int) (kFrameSeconds * sampleRate))
        size <<= 1;
    half = size / 2;
    fft.prepare (size);
    window.resize ((size_t) size);
    for (int i = 0; i < size; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / size));
    input.assign ((size_t) size, 0.0f);
    power.assign ((size_t) half + 1, 0.0f);
    phase.assign ((size_t) half + 1, 0.0f);
    peak.assign ((size_t) half + 1, 0);
    wasPeak.assign ((size_t) half + 1, 0);
    for (int i = 0; i < 2; ++i)
    {
        tonal[i].assign ((size_t) size, 0.0f);
        noise[i].assign ((size_t) size, 0.0f);
    }
    spectrum.assign ((size_t) size, {});
    lowestBin = std::max (1, (int) std::ceil (kLowestHz * size / sampleRate));
    lobe = std::max (2, (int) std::lround (kLobeHz * size / sampleRate));
    retry = std::max (1, (int) std::lround (0.05 * sampleRate));
    step = std::max (1, (int) std::lround (0.02 * sampleRate)); // one transform per 20 ms while a loop is made
    setIntervalSeconds ((double) interval / sampleRate);
    reset (1);
}

void SpectralFloor::reset (uint64_t seed)
{
    random.seed (seed);
    clear();
}

void SpectralFloor::clear()
{
    for (int i = 0; i < 2; ++i)
    {
        std::fill (tonal[i].begin(), tonal[i].end(), 0.0f);
        std::fill (noise[i].begin(), noise[i].end(), 0.0f);
    }
    std::fill (wasPeak.begin(), wasPeak.end(), 0);
    hasLoop = false;
    position = current = 0;
    fadeIn = fadeLength = 1;
    sinceLoop = retry;
    job = -1;
    jobWait = 0;
}

void SpectralFloor::setIntervalSeconds (double seconds)
{
    interval = std::max (step * (kFrames + 2), (int) std::lround (seconds * sampleRate));
}

void SpectralFloor::startJob()
{
    std::fill (power.begin(), power.end(), 0.0f);
    sumSquares = 0.0;
    job = 0;
    jobWait = 0;
}

void SpectralFloor::addFrame()
{
    for (int i = 0; i < size; ++i)
    {
        const float x = input[(size_t) i];
        sumSquares += (double) x * x;
        spectrum[(size_t) i] = { x * window[(size_t) i], 0.0f };
    }
    fft.perform (spectrum.data(), false);
    for (int k = 0; k <= half; ++k)
        power[(size_t) k] += std::norm (spectrum[(size_t) k]);
    ++job;
    jobWait = step;
}

void SpectralFloor::buildLoop()
{
    sinceLoop = 0;
    if (std::sqrt (sumSquares / ((double) kFrames * size)) < kSilenceRms)
        return;

    // Partials: clear peaks (the highest bin within +-lobe, 15 dB above the spectrum 2 x lobe away), each
    // gathered with its lobe into the bin nearest its centre. `input` is free now and holds their power.
    auto& gathered = input;
    std::fill (gathered.begin(), gathered.end(), 0.0f);
    std::fill (peak.begin(), peak.end(), 0);
    for (int k = std::max (lowestBin, 2 * lobe); k < half - 2 * lobe; ++k)
    {
        const float p = power[(size_t) k];
        if (! (p > kPeakRatio * std::max (power[(size_t) (k - 2 * lobe)], power[(size_t) (k + 2 * lobe)])))
            continue;
        bool highest = true;
        for (int j = k - lobe; j <= k + lobe && highest; ++j)
            highest = j == k || power[(size_t) j] < p || (power[(size_t) j] == p && j > k);
        if (! highest)
            continue;
        double energy = 0.0, moment = 0.0;
        for (int j = k - lobe; j <= k + lobe; ++j)
            if (power[(size_t) j] > 0.0f)
            {
                energy += power[(size_t) j];
                moment += (double) j * power[(size_t) j];
            }
        const int centre = (int) std::lround (moment / energy);
        gathered[(size_t) centre] += (float) energy;
        for (int j = k - lobe; j <= k + lobe; ++j)
            power[(size_t) j] = -1.0f; // taken
        k += lobe;
    }

    // Spectrum: partials (steady phase, kept from the last loop) as the real signal, the rest (random
    // phases) as the imaginary one, so one inverse transform makes both
    std::fill (spectrum.begin(), spectrum.end(), std::complex<float> {});
    for (int k = lowestBin; k < half; ++k)
    {
        std::complex<float> t {}, n {};
        if (gathered[(size_t) k] > 0.0f)
        {
            if (! wasPeak[(size_t) k])
                phase[(size_t) k] = (float) (kTwoPi * random.uniform());
            t = std::polar (std::sqrt (gathered[(size_t) k]), phase[(size_t) k]);
            peak[(size_t) k] = 1;
        }
        if (power[(size_t) k] > 0.0f)
            n = std::polar (std::sqrt (power[(size_t) k]), (float) (kTwoPi * random.uniform()));
        spectrum[(size_t) k] = t + std::complex<float> (0.0f, 1.0f) * n;
        spectrum[(size_t) (size - k)] = std::conj (t) + std::complex<float> (0.0f, 1.0f) * std::conj (n);
    }
    fft.perform (spectrum.data(), true);

    double squares = 0.0;
    for (const auto& v : spectrum)
        squares += (double) std::norm (v);
    if (squares <= 0.0)
        return;
    const auto gain = (float) (1.0 / std::sqrt (squares / size));

    // Crossfade in over the interval from the current loop, or the first time in 100 ms from silence (the
    // loops are cleared)
    const int next = 1 - current;
    for (int i = 0; i < size; ++i)
    {
        tonal[next][(size_t) i] = spectrum[(size_t) i].real() * gain;
        noise[next][(size_t) i] = spectrum[(size_t) i].imag() * gain;
    }
    std::swap (peak, wasPeak);
    fadeIn = 0;
    fadeLength = hasLoop ? interval : std::max (1, (int) std::lround (0.1 * sampleRate));
    hasLoop = true;
}

float SpectralFloor::processSample()
{
    if (job < 0 && fadeIn >= fadeLength && sinceLoop >= (hasLoop ? interval : retry))
        startJob();
    if (job >= 0)
    {
        if (jobWait > 0)
            --jobWait;
        else if (job == kFrames)
        {
            buildLoop();
            job = -1;
        }
    }
    ++sinceLoop;

    if (! hasLoop)
        return 0.0f;
    const auto p = (size_t) position;
    position = (position + 1) % size;
    if (fadeIn >= fadeLength)
        return tonal[current][p] + noise[current][p];

    // Partials keep their phase, so they crossfade linearly; the noise parts are independent, so at equal power
    const int next = 1 - current;
    const float x = (float) fadeIn / (float) fadeLength;
    const float out = tonal[current][p] + (tonal[next][p] - tonal[current][p]) * x
                      + noise[current][p] * std::cos (kHalfPi * x) + noise[next][p] * std::sin (kHalfPi * x);
    if (++fadeIn == fadeLength)
        current = next;
    return out;
}
} // namespace gf
