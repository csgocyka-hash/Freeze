#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/Random.h"
#include <cstdio>

namespace
{
std::vector<float> constant (size_t n, float v) { return std::vector<float> (n, v); }

std::vector<std::vector<float>> freezePlucks (double sr, int numChannels, double seconds = 5.0)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, numChannels);
    e.setSeed (1);
    auto p = pluckTrain (sr, seconds);
    std::vector<std::vector<float>> in ((size_t) numChannels, p);
    return runEngine (e, in, constant (p.size(), 1.0f), 512);
}
} // namespace

TEST (amount_zero_is_bit_identical)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 2);
    auto p = pluckTrain (48000, 3.0);
    auto out = runEngine (e, { p, p }, constant (p.size(), 0.0f), 512);
    CHECK (out[0] == p);
    CHECK (out[1] == p);
}

TEST (silence_in_silence_out)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto z = constant (96000, 0.0f);
    auto out = runEngine (e, { z }, constant (z.size(), 1.0f), 512);
    CHECK (out[0] == z);
}

TEST (dry_plucks_have_rhythm)
{
    CHECK (foldedRhythmFloor (pluckTrain (48000, 5.0), 48000) < 0.05); // sanity check of the measure
}

TEST (fills_gaps_between_plucks)
{
    checkGapsFilled (freezePlucks (48000, 1)[0], 48000);
}

TEST (fills_gaps_at_other_rates)
{
    for (double sr : { 44100.0, 96000.0 })
        checkGapsFilled (freezePlucks (sr, 1)[0], sr);
}

TEST (fills_gaps_mono_and_stereo)
{
    auto out = freezePlucks (48000, 2);
    checkGapsFilled (out[0], 48000);
    checkGapsFilled (out[1], 48000);
    CHECK (out[0] != out[1]); // channels decorrelated
}

TEST (sustains_after_input_stops)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto in = pluckTrain (48000, 2.0);
    in.resize (5 * 48000, 0.0f);
    auto out = runEngine (e, { in }, constant (in.size(), 1.0f), 512)[0];
    const double a = rms (out.data() + 48000, 48000);
    const double b = rms (out.data() + 2 * 48000, 3 * 48000);
    std::printf ("    playing %.4f, after stop %.4f\n", a, b);
    CHECK (b > a * 0.5);
    CHECK (b < a * 2.0);
}

TEST (wet_level_matches_input)
{
    auto p = pluckTrain (48000, 5.0);
    auto out = freezePlucks (48000, 1)[0];
    const double ref = soundingRms (p, 48000);
    const double wet = rms (out.data() + 48000, out.size() - 48000);
    const double db = 20.0 * std::log10 (wet / ref);
    std::printf ("    wet vs sounding input: %+.2f dB\n", db);
    CHECK (db > -4.0);
    CHECK (db < 4.0);
}

TEST (no_clicks_on_sine)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 4.0, 440, 0.5f);
    auto out = runEngine (e, { s }, constant (s.size(), 1.0f), 512)[0];
    const double maxSlope = kTwoPi * 440.0 / 48000.0 * 0.5;
    const double step = maxStep (out, 48000);
    std::printf ("    max step %.4f, bound %.4f\n", step, 4.0 * maxSlope);
    CHECK (step < 4.0 * maxSlope);
}

TEST (no_clicks_on_knob_sweep)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 4.0, 440, 0.5f);
    std::vector<float> amt (s.size(), 1.0f);
    for (size_t i = 0; i < 48000; ++i)
        amt[i] = (float) i / 48000.0f;
    auto out = runEngine (e, { s }, amt, 512)[0];
    const double maxSlope = kTwoPi * 440.0 / 48000.0 * 0.5;
    CHECK (maxStep (out, 0) < 4.0 * maxSlope);
}

TEST (wet_appears_after_long_bypass)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto p = pluckTrain (48000, 4.0);
    std::vector<float> amt (p.size(), 0.0f);
    std::fill (amt.begin() + 3 * 48000, amt.end(), 1.0f);
    auto out = runEngine (e, { p }, amt, 512)[0];
    const auto from = (size_t) (3.2 * 48000);
    CHECK (rms (out.data() + from, out.size() - from) > 0.1 * soundingRms (p, 48000));
}

TEST (deterministic_for_same_seed)
{
    auto p = pluckTrain (48000, 3.0);
    auto run = [&] (uint64_t seed) {
        gf::FreezeEngine e;
        e.prepare (48000, 512, 1);
        e.setSeed (seed);
        return runEngine (e, { p }, constant (p.size(), 1.0f), 512)[0];
    };
    CHECK (run (7) == run (7));
    CHECK (run (7) != run (8));
}

TEST (block_size_independent)
{
    auto p = pluckTrain (48000, 3.0);
    std::vector<std::vector<float>> results[3];
    const int sizes[3] = { 1, 64, 512 };
    for (int i = 0; i < 3; ++i)
    {
        gf::FreezeEngine e;
        e.prepare (48000, 512, 2);
        results[i] = runEngine (e, { p, p }, constant (p.size(), 1.0f), sizes[i]);
    }
    CHECK (results[0] == results[1]);
    CHECK (results[1] == results[2]);
}

TEST (stable_on_noise_and_full_scale)
{
    gf::Random r (3);
    std::vector<float> noise (60 * 48000);
    for (auto& x : noise)
        x = (float) (2.0 * r.uniform() - 1.0);
    std::vector<float> square (10 * 48000);
    for (size_t i = 0; i < square.size(); ++i)
        square[i] = (i / 240) % 2 == 0 ? 1.0f : -1.0f; // 100 Hz at 48 kHz

    for (auto* input : { &noise, &square })
    {
        gf::FreezeEngine e;
        e.prepare (48000, 512, 1);
        auto out = runEngine (e, { *input }, constant (input->size(), 1.0f), 512)[0];
        bool finite = true;
        float peak = 0.0f;
        for (float x : out)
        {
            finite = finite && std::isfinite (x);
            peak = std::max (peak, std::abs (x));
        }
        std::printf ("    peak %.3f\n", peak);
        CHECK (finite);
        CHECK (peak < 8.0f);
    }
}
