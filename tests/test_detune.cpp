#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

// The frozen sound alone: full Amount (no dry signal), shimmer layer off so only grains are measured
std::vector<float> freezeSine (float detune, int blockSize = 512, float amount = 1.0f, uint64_t seed = 1)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setSeed (seed);
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
    const size_t from = (size_t) sr, to = (size_t) (4 * sr);
    auto spread = [&] (const std::vector<float>& x) {
        // +-30 cents around 440 Hz vs the exact pitch
        return (toneLevel (x, from, to, 432.5, sr) + toneLevel (x, from, to, 447.7, sr)) / toneLevel (x, from, to, 440.0, sr);
    };
    // Averaged over seeds (in dB): narrow-band levels of a random cloud scatter a lot on one seed
    double outsideDb = 0.0;
    const double gainDb = meanOverSeeds ([&] (uint64_t seed) {
        auto plain = freezeSine (0.0f, 512, 1.0f, seed), detuned = freezeSine (1.0f, 512, 1.0f, seed);
        outsideDb += 20.0 * std::log10 (std::max (toneLevel (detuned, from, to, 470.0, sr), toneLevel (detuned, from, to, 412.0, sr))
                                        / std::max (toneLevel (detuned, from, to, 440.0, sr), toneLevel (detuned, from, to, 447.7, sr)))
                     / 5.0;
        return 20.0 * std::log10 (spread (detuned) / spread (plain));
    });
    std::printf ("    +-30 cent energy vs 440 Hz: detuned %.1f dB above plain; beyond 100 cents %.1f dB\n", gainDb, outsideDb);
    // (Grains fading in and out add small sidebands of their own, so plain is not zero.)
    CHECK (gainDb > 20.0 * std::log10 (5.0));
    CHECK (outsideDb < 20.0 * std::log10 (0.15));
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
