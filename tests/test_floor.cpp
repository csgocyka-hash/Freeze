#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/SpectralFloor.h"

namespace
{
constexpr double sr = 48000.0;

// Plucks of a 220 Hz saw-like tone (8 harmonics), switching to `later` Hz after `switchAt` seconds
std::vector<float> harmonicPlucks (double seconds, double switchAt = 1.0e9, double later = 330.0)
{
    std::vector<float> out ((size_t) (sr * seconds), 0.0f);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double time = (double) i / sr, t = std::fmod (time, 0.25), f = time < switchAt ? 220.0 : later;
        if (t < 0.2)
            for (int h = 1; h <= 8; ++h)
                out[i] += (float) (0.3 / h * std::exp (-t / 0.06) * std::sin (kTwoPi * f * h * t));
    }
    return out;
}

std::vector<float> render (const std::vector<float>& in, float floorLevel, float amount = 1.0f, double grain = 0.12,
                           uint64_t seed = 1)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setSeed (seed);
    e.setGrainSeconds (grain);
    e.setFloorLevel (floorLevel);
    std::vector<float> a (in.size(), amount);
    return runEngine (e, { in }, a, 512)[0];
}

// The floor alone: the grains are the same with and without it, so the difference is the floor
std::vector<float> floorAlone (const std::vector<float>& in, float level = 0.25f)
{
    auto with = render (in, level), without = render (in, 0.0f);
    for (size_t i = 0; i < with.size(); ++i)
        with[i] -= without[i];
    return with;
}

// Strongest tone level within +-10 Hz of f, searched finely: the floor's partials are steady sines that
// a coarse search could miss between its steps
double fineBandLevel (const std::vector<float>& x, size_t from, size_t to, double f)
{
    double best = 0.0;
    for (double g = f - 10.0; g <= f + 10.0; g += 0.05)
        best = std::max (best, toneLevel (x, from, to, g, sr));
    return best;
}

double worstSecondDifference (const std::vector<float>& x, size_t from)
{
    double worst = 0.0;
    for (size_t i = std::max<size_t> (from, 2); i < x.size(); ++i)
        worst = std::max (worst, (double) std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]));
    return worst;
}
} // namespace

TEST (floor_loop_repeats_without_clicks)
{
    // A loop made from a random-phase spectrum is periodic: playing it round and round has no seam.
    // The loop is normalised to an RMS of 1. Fed a steady two-partial tone it plays steadily, also where it
    // wraps round and while one loop crossfades into the next.
    gf::SpectralFloor f;
    f.prepare (sr);
    f.reset (1);
    f.setIntervalSeconds (0.5);
    std::vector<float> out ((size_t) (6 * sr));
    int64_t made = 0;
    for (size_t n = 0; n < out.size(); ++n)
    {
        if (const int frame = f.pendingFrame(); frame >= 0)
        {
            float* x = f.frameInput();
            for (int i = 0; i < f.frameSize(); ++i)
                x[i] = (float) (0.3 * std::sin (kTwoPi * 440.0 * (double) (i + (int64_t) n) / sr) + 0.1 * std::sin (kTwoPi * 1320.0 * (double) (i + (int64_t) n) / sr));
            f.addFrame();
            made += frame == 0 ? 1 : 0;
        }
        out[n] = f.processSample();
    }
    const auto from = (size_t) (2 * sr);
    std::vector<float> tail (out.begin() + (long) from, out.end());
    const double level = rms (tail.data(), tail.size()), worst = worstSecondDifference (tail, 0);
    const int n = f.frameSize();
    double atSeam = 0.0;
    for (size_t seam = (size_t) n; seam + 1 < out.size(); seam += (size_t) n)
        if (seam > from)
            atSeam = std::max ({ atSeam, (double) std::abs (out[seam] - 2.0f * out[seam - 1] + out[seam - 2]),
                                 (double) std::abs (out[seam + 1] - 2.0f * out[seam] + out[seam - 1]) });
    std::printf ("    loops made %lld, level %.3f, envelope variation %.3f, worst second difference %.4f, at seams %.4f\n",
                 (long long) made, level, envelopeVariation (tail, 0), worst, atSeam);
    CHECK (made >= 8);
    CHECK_NEAR (level, 1.0, 0.1);
    CHECK (envelopeVariation (tail, 0) < 0.1); // steady: no beating partials
    CHECK (worst < 0.08 * level);              // the 1320 Hz partial alone gives about w^2 = 0.03 of the level
    CHECK (atSeam <= worst);
}

TEST (floor_is_quiet_full_range_and_keeps_pitch)
{
    // A 25 % floor under the grains, with the notes' fundamental (not high-passed) and their pitch
    auto in = harmonicPlucks (6.0);
    auto layer = floorAlone (in), grains = render (in, 0.0f);
    const size_t from = (size_t) (2 * sr), to = layer.size();
    const double ratio = rms (layer.data() + from, to - from) / rms (grains.data() + from, to - from);
    const double at220 = fineBandLevel (layer, from, to, 220.0), at1320 = fineBandLevel (layer, from, to, 1320.0);
    const double off = std::max (fineBandLevel (layer, from, to, 165.0), fineBandLevel (layer, from, to, 290.0));
    std::printf ("    floor / grains %.3f; floor at 220 Hz %.5f, 1320 Hz %.5f, 165 / 290 Hz %.5f\n", ratio, at220, at1320, off);
    CHECK (ratio > 0.2 && ratio < 0.3);
    CHECK (at220 > at1320);
    CHECK (at220 > off * 10.0);
}

TEST (floor_has_no_clicks)
{
    auto layer = floorAlone (sine (sr, 6.0, 440.0, 0.5f));
    const size_t from = (size_t) sr;
    const double level = rms (layer.data() + from, layer.size() - from), worst = worstSecondDifference (layer, from);
    std::printf ("    floor level %.4f, worst second difference %.5f (%.4f of the level)\n", level, worst, worst / level);
    CHECK (level > 0.01);
    CHECK (worst < 0.02 * level); // a 440 Hz tone gives about w^2 = 0.0033 of its peak
}

TEST (floor_follows_new_notes)
{
    auto layer = floorAlone (harmonicPlucks (8.0, 3.0));
    const size_t from = (size_t) (6 * sr), to = layer.size();
    const double old = fineBandLevel (layer, from, to, 220.0), fresh = fineBandLevel (layer, from, to, 330.0);
    std::printf ("    3 s after the change: 220 Hz %.5f, 330 Hz %.5f\n", old, fresh);
    CHECK (fresh > old * 5.0);
}

TEST (floor_fills_the_clouds_dips)
{
    // Long grains of one tone at random phases sometimes cancel for a moment (10 ms windows 25-40 dB down).
    // The floor is steady, so with it the quietest moments are clearly less deep.
    auto in = pluckTrain (sr, 5.0);
    const double grainsOnly = meanOverSeeds ([&] (uint64_t seed) { return 20.0 * std::log10 (minWindowRatio (render (in, 0.0f, 1.0f, 0.5, seed), sr)); }, 3);
    const double withFloor = meanOverSeeds ([&] (uint64_t seed) { return 20.0 * std::log10 (minWindowRatio (render (in, 0.25f, 1.0f, 0.5, seed), sr)); }, 3);
    std::printf ("    quietest 10 ms window: grains only %.1f dB, with floor %.1f dB\n", grainsOnly, withFloor);
    CHECK (withFloor > grainsOnly + 3.0);
}

TEST (floor_is_off_below_half_amount)
{
    auto in = pluckTrain (sr, 3.0);
    CHECK (render (in, 0.25f, 0.45f) == render (in, 0.0f, 0.45f));
}
