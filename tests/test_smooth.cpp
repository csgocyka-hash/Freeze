#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

std::vector<float> freezeAt (float amount, std::vector<float> input, float smoothness = 1.0f, gf::FreezeEngine* engine = nullptr,
                             uint64_t seed = 1)
{
    gf::FreezeEngine local;
    auto& e = engine != nullptr ? *engine : local;
    e.prepare (sr, 512, 1);
    e.setSeed (seed);
    e.setMaxSmoothness (smoothness);
    e.setFloorLevel (0.0f); // grains only; the floor has its own tests (test_floor.cpp)
    std::vector<float> a (input.size(), amount);
    return runEngine (e, { std::move (input) }, a, 512)[0];
}
} // namespace

TEST (full_amount_has_no_pluck_left)
{
    auto plain = freezeAt (1.0f, pluckTrain (sr, 8.0), 0.0f);
    auto smooth = freezeAt (1.0f, pluckTrain (sr, 8.0));
    const double plainVar = envelopeVariation (plain, (size_t) (2 * sr)), smoothVar = envelopeVariation (smooth, (size_t) (2 * sr));
    const double plainSkew = onsetSkew (std::vector<float> (plain.begin() + (long) sr, plain.end()));
    const double smoothSkew = onsetSkew (std::vector<float> (smooth.begin() + (long) sr, smooth.end()));
    std::printf ("    envelope variation %.3f -> %.3f, onset skew %.2f -> %.2f\n", plainVar, smoothVar, plainSkew, smoothSkew);
    // (The envelope variation is reported only: on a pure test tone it comes from grains interfering with
    // each other, not from the plucks.)
    CHECK (std::abs (smoothSkew) < plainSkew * 0.4);
}

TEST (full_amount_uses_longer_denser_grains)
{
    gf::FreezeEngine plain, smooth;
    freezeAt (1.0f, pluckTrain (sr, 4.0), 0.0f, &plain);
    freezeAt (1.0f, pluckTrain (sr, 4.0), 1.0f, &smooth);
    std::printf ("    grains spawned: %lld -> %lld\n", (long long) plain.grainsSpawnedCount(), (long long) smooth.grainsSpawnedCount());
    // Twice as long grains at ~16 instead of 6 overlapping -> about 16/6/2 = 1.33x as many spawns
    CHECK (smooth.grainsSpawnedCount() > plain.grainsSpawnedCount() * 1.15);
}

TEST (full_amount_keeps_pitch_and_level)
{
    // (Averaged over seeds: on one seed the off-pitch levels of a random cloud scatter by several dB.)
    auto in = pluckTrain (sr, 8.0, 220.0);
    const size_t from = (size_t) (2 * sr), to = in.size();
    double db = 0.0;
    const double pitchDb = meanOverSeeds ([&] (uint64_t seed) {
        auto out = freezeAt (1.0f, in, 1.0f, nullptr, seed);
        const double off = std::max (bandLevel (out, from, to, 165.0, sr), bandLevel (out, from, to, 290.0, sr));
        db += 20.0 * std::log10 (rms (out.data() + from, to - from) / soundingRms (in, sr)) / 5.0;
        return 20.0 * std::log10 (bandLevel (out, from, to, 220.0, sr) / off);
    });
    std::printf ("    220 Hz %.1f dB above 165 / 290 Hz, level %+.2f dB\n", pitchDb, db);
    CHECK (pitchDb > 20.0);
    CHECK (db > -4.0 && db < 4.0);
}

TEST (below_half_amount_is_unchanged_by_smoothing)
{
    auto in = pluckTrain (sr, 3.0);
    CHECK (freezeAt (0.45f, in) == freezeAt (0.45f, in, 0.0f));
}

TEST (smooth_cloud_has_no_clicks)
{
    auto out = freezeAt (1.0f, sine (sr, 4.0, 440.0, 0.5f));
    CHECK (maxStep (out, (size_t) sr) < 4.0 * kTwoPi * 440.0 / sr * 0.5);
}

TEST (smooth_cloud_on_plucks_has_no_clicks)
{
    // A click is a sudden jump: it shows up as a spike in the second difference, which for a smooth
    // 220 Hz cloud stays tiny (about w^2 = 0.0008 of the signal).
    auto out = freezeAt (1.0f, pluckTrain (sr, 8.0));
    const size_t from = (size_t) (2 * sr);
    double worst = 0.0;
    for (size_t i = from; i < out.size(); ++i)
        worst = std::max (worst, (double) std::abs (out[i] - 2.0f * out[i - 1] + out[i - 2]));
    const double level = rms (out.data() + from, out.size() - from);
    std::printf ("    worst jump %.5f, level %.4f, ratio %.4f\n", worst, level, worst / level);
    CHECK (worst < 0.02 * level);
}

TEST (full_amount_overlaps_24_grains)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setGrainSeconds (0.25); // x2 at full Amount = 0.5 s, so 24 overlapping means a spawn every ~21 ms
    auto in = pluckTrain (sr, 4.0);
    std::vector<float> a (in.size(), 1.0f);
    runEngine (e, { in }, a, 512);
    const double perSecond = (double) e.grainsSpawnedCount() / 4.0;
    std::printf ("    grains per second %.1f (expect ~48)\n", perSecond);
    CHECK (perSecond > 40.0 && perSecond < 56.0);
}

TEST (smooth_grains_read_apart_from_each_other)
{
    // Two grains reading almost the same spot at the same time form a comb filter (phasing). In smooth mode
    // a new grain should start reading at least 40 ms away from every playing grain of the same direction.
    // (With ~12 grains per direction in 1.5 s of material that is not always possible; picked at random,
    // about half of them start too close.)
    gf::FreezeEngine plain, smooth;
    freezeAt (1.0f, pluckTrain (sr, 8.0), 0.0f, &plain);
    freezeAt (1.0f, pluckTrain (sr, 8.0), 1.0f, &smooth);
    std::printf ("    grains starting within 40 ms of a playing grain: plain %.3f, smooth %.3f\n", plain.closeReadShare(),
                 smooth.closeReadShare());
    CHECK (smooth.closeReadShare() < 0.2);
}
