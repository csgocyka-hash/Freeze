#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

std::vector<float> freezeAt (float amount, std::vector<float> input, float smoothness = 1.0f, gf::FreezeEngine* engine = nullptr)
{
    gf::FreezeEngine local;
    auto& e = engine != nullptr ? *engine : local;
    e.prepare (sr, 512, 1);
    e.setMaxSmoothness (smoothness);
    e.setSpectralLevel (0.0f); // grains only; the spectral layer has its own tests
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
    auto in = pluckTrain (sr, 8.0, 220.0);
    auto out = freezeAt (1.0f, in);
    const size_t from = (size_t) (2 * sr), to = out.size();
    const double at220 = bandLevel (out, from, to, 220.0, sr);
    const double off = std::max ({ bandLevel (out, from, to, 165.0, sr), bandLevel (out, from, to, 290.0, sr) });
    const double db = 20.0 * std::log10 (rms (out.data() + from, to - from) / soundingRms (in, sr));
    std::printf ("    220 Hz %.4f vs %.4f, level %+.2f dB\n", at220, off, db);
    CHECK (at220 > off * 10.0);
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

namespace
{
// Plucks of a 220 Hz saw-like tone (8 harmonics), so a high-passed layer has something to keep
std::vector<float> richPlucks (double seconds)
{
    std::vector<float> out ((size_t) (sr * seconds), 0.0f);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = std::fmod ((double) i / sr, 0.25);
        if (t < 0.2)
            for (int h = 1; h <= 8; ++h)
                out[i] += (float) (0.3 / h * std::exp (-t / 0.06) * std::sin (kTwoPi * 220.0 * h * t));
    }
    return out;
}

std::vector<float> renderWithSpectral (float level, const std::vector<float>& in)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setSpectralLevel (level);
    std::vector<float> a (in.size(), 1.0f);
    return runEngine (e, { in }, a, 512)[0];
}
} // namespace

TEST (spectral_layer_is_quiet_and_high_passed)
{
    auto in = richPlucks (6.0);
    auto grains = renderWithSpectral (0.0f, in);
    auto both = renderWithSpectral (0.2f, in);
    std::vector<float> layer (both.size());
    for (size_t i = 0; i < both.size(); ++i)
        layer[i] = both[i] - grains[i]; // grains are identical, so this is the spectral layer alone
    const size_t from = (size_t) (2 * sr), to = layer.size();
    const double ratio = rms (layer.data() + from, to - from) / rms (grains.data() + from, to - from);
    const double low = bandLevel (layer, from, to, 220.0, sr), high = bandLevel (layer, from, to, 1320.0, sr);
    std::printf ("    layer / grains %.3f, layer at 220 Hz %.5f vs 1320 Hz %.5f\n", ratio, low, high);
    CHECK (ratio > 0.02 && ratio < 0.25);
    CHECK (low < high * 0.5);
}

TEST (spectral_layer_is_off_below_half_amount)
{
    auto in = pluckTrain (sr, 3.0);
    std::vector<float> a (in.size(), 0.45f);
    gf::FreezeEngine withLayer, without;
    withLayer.prepare (sr, 512, 1);
    without.prepare (sr, 512, 1);
    without.setSpectralLevel (0.0f);
    CHECK (runEngine (withLayer, { in }, a, 512)[0] == runEngine (without, { in }, a, 512)[0]);
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
