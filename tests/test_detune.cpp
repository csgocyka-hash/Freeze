#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

// The frozen sound alone: full Amount (no dry signal), shimmer layer off so only grains are measured
std::vector<float> freezeSine (float detune, int blockSize = 512, float amount = 1.0f)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setDetune (detune);
    e.setSpectralLevel (0.0f);
    auto s = sine (sr, 4.0, 440.0, 0.5f);
    std::vector<float> a (s.size(), amount);
    return runEngine (e, { s }, a, blockSize)[0];
}
} // namespace

TEST (detune_zero_changes_nothing)
{
    auto s = sine (sr, 2.0, 440.0, 0.5f);
    std::vector<float> a (s.size(), 0.45f);
    gf::FreezeEngine plain, zero;
    plain.prepare (sr, 512, 1);
    zero.prepare (sr, 512, 1);
    zero.setDetune (0.0f);
    CHECK (runEngine (plain, { s }, a, 512)[0] == runEngine (zero, { s }, a, 512)[0]);
}

TEST (detune_spreads_grains_up_to_50_cents)
{
    auto plain = freezeSine (0.0f), detuned = freezeSine (1.0f);
    const size_t from = (size_t) sr, to = plain.size();
    auto spread = [&] (const std::vector<float>& x) {
        // +-30 cents around 440 Hz vs the exact pitch
        return (toneLevel (x, from, to, 432.5, sr) + toneLevel (x, from, to, 447.7, sr)) / toneLevel (x, from, to, 440.0, sr);
    };
    const double outside = std::max (toneLevel (detuned, from, to, 470.0, sr), toneLevel (detuned, from, to, 412.0, sr))
                           / std::max (toneLevel (detuned, from, to, 440.0, sr), toneLevel (detuned, from, to, 447.7, sr));
    std::printf ("    +-30 cent energy vs 440 Hz: plain %.3f, detuned %.3f; beyond 100 cents %.3f\n", spread (plain), spread (detuned), outside);
    // (Grains fading in and out add small sidebands of their own, so plain is not zero.)
    CHECK (spread (detuned) > spread (plain) * 5.0);
    CHECK (outside < 0.15);
}

TEST (detune_keeps_level_and_has_no_clicks)
{
    auto plain = freezeSine (0.0f), detuned = freezeSine (1.0f);
    const size_t from = (size_t) sr;
    const double db = 20.0 * std::log10 (rms (detuned.data() + from, detuned.size() - from) / rms (plain.data() + from, plain.size() - from));
    std::printf ("    level change %+.2f dB\n", db);
    CHECK (std::abs (db) < 1.5);
    CHECK (maxStep (detuned, from) < 4.0 * kTwoPi * 440.0 / sr * 0.5 * 1.03);
}

TEST (detune_is_block_size_independent)
{
    CHECK (freezeSine (0.7f, 1) == freezeSine (0.7f, 64));
    CHECK (freezeSine (0.7f, 64, 0.45f) == freezeSine (0.7f, 512, 0.45f));
}
