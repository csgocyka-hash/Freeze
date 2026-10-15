#pragma once
// Level and smoothness measurements shared by the engine tests.
#include "Check.h"
#include "Signals.h"
#include <algorithm>
#include <cstdio>

// RMS over the input's sounding 10 ms segments (the level the freeze should match).
inline double soundingRms (const std::vector<float>& x, double sr)
{
    const auto seg = (size_t) std::lround (0.010 * sr);
    double sum = 0.0;
    size_t count = 0;
    for (size_t pos = 0; pos + seg <= x.size(); pos += seg)
        if (rms (x.data() + pos, seg) >= 0.001)
        {
            for (size_t i = 0; i < seg; ++i)
                sum += (double) x[pos + i] * x[pos + i];
            count += seg;
        }
    return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
}

// 10 ms window RMS after the first second, relative to the overall RMS after the first second.
inline std::vector<double> windowRatios (const std::vector<float>& out, double sr)
{
    const auto start = (size_t) sr;
    const auto win = (size_t) std::lround (0.010 * sr);
    const double overall = rms (out.data() + start, out.size() - start);
    std::vector<double> ratios;
    for (size_t pos = start; pos + win <= out.size(); pos += win)
        ratios.push_back (rms (out.data() + pos, win) / overall);
    return ratios;
}

inline double minWindowRatio (const std::vector<float>& out, double sr)
{
    auto r = windowRatios (out, sr);
    return *std::min_element (r.begin(), r.end());
}

// Residual pluck rhythm: window levels folded by the 250 ms pluck period (25 bins).
// Returns the quietest bin relative to the mean bin: 0 for the dry plucks, near 1 when the rhythm is gone.
inline double foldedRhythmFloor (const std::vector<float>& out, double sr)
{
    auto r = windowRatios (out, sr);
    double bins[25] = {};
    int counts[25] = {};
    for (size_t k = 0; k < r.size(); ++k)
    {
        bins[k % 25] += r[k];
        ++counts[k % 25];
    }
    double mean = 0.0, lowest = 1e9;
    for (int b = 0; b < 25; ++b)
        mean += (bins[b] /= counts[b]) / 25.0;
    for (int b = 0; b < 25; ++b)
        lowest = std::min (lowest, bins[b] / mean);
    return lowest;
}

// Gaps are filled when the pluck rhythm has gone and no 10 ms window drops below -30 dB.
// (Single windows can dip near -20 dB: granulating a steady tone makes its level wander at random.)
inline void checkGapsFilled (const std::vector<float>& out, double sr)
{
    const double floor = foldedRhythmFloor (out, sr);
    const double minRatio = minWindowRatio (out, sr);
    std::printf ("    %.0f Hz: rhythm floor %.2f, min window %.1f dB\n", sr, floor, 20.0 * std::log10 (minRatio));
    CHECK (floor > 0.6);
    CHECK (minRatio > 0.0316);
}

// The grain cloud is random, so values measured on it scatter from seed to seed (grains of one tone at
// random phases can cancel each other for a moment). Statistical checks average over a few seeds.
template <typename Measure>
inline double meanOverSeeds (Measure measure, int seeds = 5)
{
    double sum = 0.0;
    for (int s = 1; s <= seeds; ++s)
        sum += measure ((uint64_t) s);
    return sum / seeds;
}

// checkGapsFilled on the median of a few seeds
template <typename Render>
inline void checkGapsFilledOverSeeds (Render render, double sr, int seeds = 3)
{
    std::vector<double> floors, minima;
    for (int s = 1; s <= seeds; ++s)
    {
        const auto out = render ((uint64_t) s);
        floors.push_back (foldedRhythmFloor (out, sr));
        minima.push_back (minWindowRatio (out, sr));
    }
    std::sort (floors.begin(), floors.end());
    std::sort (minima.begin(), minima.end());
    const double floor = floors[floors.size() / 2], minRatio = minima[minima.size() / 2];
    std::printf ("    %.0f Hz: rhythm floor %.2f, min window %.1f dB (medians of %d seeds)\n", sr, floor, 20.0 * std::log10 (minRatio), seeds);
    CHECK (floor > 0.6);
    CHECK (minRatio > 0.0316);
}

inline double maxStep (const std::vector<float>& x, size_t from)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i)
        m = std::max (m, (double) std::abs (x[i] - x[i - 1]));
    return m;
}


// Direction of the sound: skewness of the 2 ms envelope's changes after the first second. Plucks rise
// sharply and decay slowly (large positive skew); a directionless wash has skew near 0.
inline double onsetSkew (const std::vector<float>& x)
{
    std::vector<double> env, d;
    for (size_t pos = 48000; pos + 96 <= x.size(); pos += 96)
        env.push_back (rms (x.data() + pos, 96));
    for (size_t i = 1; i < env.size(); ++i)
        d.push_back (env[i] - env[i - 1]);
    double mean = 0.0, m2 = 0.0, m3 = 0.0;
    for (double v : d) mean += v / (double) d.size();
    for (double v : d)
    {
        m2 += (v - mean) * (v - mean) / (double) d.size();
        m3 += (v - mean) * (v - mean) * (v - mean) / (double) d.size();
    }
    return m3 / std::pow (m2, 1.5);
}

// Envelope fluctuation: standard deviation / mean of 10 ms RMS windows from `fromSample` on
inline double envelopeVariation (const std::vector<float>& x, size_t fromSample)
{
    std::vector<double> env;
    for (size_t pos = fromSample; pos + 480 <= x.size(); pos += 480)
        env.push_back (rms (x.data() + pos, 480));
    double mean = 0.0, var = 0.0;
    for (double v : env) mean += v / (double) env.size();
    for (double v : env) var += (v - mean) * (v - mean) / (double) env.size();
    return std::sqrt (var) / mean;
}

// Level of x at frequency f (Goertzel), normalised by length
inline double toneLevel (const std::vector<float>& x, size_t from, size_t to, double f, double sr)
{
    const double w = kTwoPi * f / sr, coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        const double s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) / (double) (to - from);
}

// Strongest tone level within +-10 Hz of f (the rebuilt tone can sit a little off the exact frequency)
inline double bandLevel (const std::vector<float>& x, size_t from, size_t to, double f, double sr)
{
    double best = 0.0;
    for (double g = f - 10.0; g <= f + 10.0; g += 0.5)
        best = std::max (best, toneLevel (x, from, to, g, sr));
    return best;
}
