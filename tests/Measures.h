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

inline double maxStep (const std::vector<float>& x, size_t from)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i)
        m = std::max (m, (double) std::abs (x[i] - x[i - 1]));
    return m;
}

