#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

std::vector<float> freezeDiffused (float amount, float diffusion, const std::vector<float>& in, uint64_t seed = 1)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setSeed (seed);
    e.setMaxDiffusion (diffusion);
    e.setFloorLevel (0.0f); // grains only
    std::vector<float> a (in.size(), amount);
    return runEngine (e, { in }, a, 512)[0];
}

// Worst second difference relative to the level: a click is a spike in it
double clickRatio (const std::vector<float>& x, size_t from)
{
    double worst = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        worst = std::max (worst, (double) std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]));
    return worst / rms (x.data() + from, x.size() - from);
}
} // namespace

TEST (diffuser_only_above_70_percent)
{
    auto in = pluckTrain (sr, 3.0);
    CHECK (freezeDiffused (0.65f, 1.0f, in) == freezeDiffused (0.65f, 0.0f, in));
    CHECK (freezeDiffused (0.9f, 1.0f, in) != freezeDiffused (0.9f, 0.0f, in));
}

TEST (diffuser_smears_grain_edges_and_keeps_level)
{
    // At full Amount every grain goes through the all-pass diffuser: what is left of grain edges and de-click
    // dips is smeared over ~45 ms (a lower click measure), and the level stays (all-passes keep power).
    auto in = pluckTrain (sr, 8.0);
    const auto from = (size_t) (2 * sr);
    double dryDb = 0.0, wetDb = 0.0;
    const double ratio = meanOverSeeds ([&] (uint64_t seed) {
        auto plain = freezeDiffused (1.0f, 0.0f, in, seed), diffused = freezeDiffused (1.0f, 1.0f, in, seed);
        dryDb += 20.0 * std::log10 (rms (plain.data() + from, plain.size() - from)) / 3.0;
        wetDb += 20.0 * std::log10 (rms (diffused.data() + from, diffused.size() - from)) / 3.0;
        return clickRatio (diffused, from) / clickRatio (plain, from);
    }, 3);
    std::printf ("    click measure diffused / plain %.2f, level change %+.2f dB\n", ratio, wetDb - dryDb);
    CHECK (ratio < 0.7);
    CHECK (std::abs (wetDb - dryDb) < 0.5);
}

TEST (diffuser_is_block_size_independent)
{
    auto in = pluckTrain (sr, 2.0);
    std::vector<float> a (in.size(), 1.0f);
    std::vector<float> outs[2];
    const int sizes[2] = { 1, 512 };
    for (int k = 0; k < 2; ++k)
    {
        gf::FreezeEngine e;
        e.prepare (sr, 512, 2);
        outs[k] = runEngine (e, { in, in }, a, sizes[k])[1];
    }
    CHECK (outs[0] == outs[1]);
}
