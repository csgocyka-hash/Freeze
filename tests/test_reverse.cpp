#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
std::vector<float> freezeWithReverse (float amount, float maxReverseShare, gf::FreezeEngine* engineOut = nullptr)
{
    gf::FreezeEngine local;
    auto& e = engineOut != nullptr ? *engineOut : local;
    e.prepare (48000, 512, 1);
    e.setMaxReverseShare (maxReverseShare);
    e.setMaxSpectralShare (0.0f); // measure the grain cloud itself
    auto p = pluckTrain (48000, 8.0);
    std::vector<float> a (p.size(), amount);
    return runEngine (e, { p }, a, 512)[0];
}

} // namespace

TEST (half_the_grains_reverse_at_full_amount)
{
    gf::FreezeEngine e;
    freezeWithReverse (1.0f, 0.5f, &e);
    std::printf ("    reversed share %.3f\n", e.reversedShare());
    CHECK_NEAR (e.reversedShare(), 0.5, 0.1);
}

TEST (reverse_share_follows_amount)
{
    gf::FreezeEngine low, mid;
    freezeWithReverse (0.1f, 0.5f, &low);
    freezeWithReverse (0.5f, 0.5f, &mid);
    std::printf ("    amount 10 %%: %.3f, amount 50 %%: %.3f\n", low.reversedShare(), mid.reversedShare());
    CHECK_NEAR (low.reversedShare(), 0.05, 0.05);
    CHECK_NEAR (mid.reversedShare(), 0.25, 0.08);
}

TEST (reverse_grains_smooth_the_freeze)
{
    const double forwardOnly = onsetSkew (freezeWithReverse (1.0f, 0.0f));
    const double mixed = onsetSkew (freezeWithReverse (1.0f, 0.5f));
    std::printf ("    onset skew: forward only %.2f, with reverse %.2f\n", forwardOnly, mixed);
    CHECK (mixed < forwardOnly * 0.6);
}

TEST (reverse_keeps_level_and_has_no_clicks)
{
    auto p = pluckTrain (48000, 8.0);
    const double db = 20.0 * std::log10 (rms (freezeWithReverse (1.0f, 0.5f).data() + 48000, p.size() - 48000)
                                         / soundingRms (p, 48000));
    std::printf ("    level with reverse: %+.2f dB\n", db);
    CHECK (db > -4.0 && db < 4.0);

    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 4.0, 440, 0.5f);
    std::vector<float> a (s.size(), 1.0f);
    auto out = runEngine (e, { s }, a, 512)[0];
    CHECK (maxStep (out, 48000) < 4.0 * kTwoPi * 440.0 / 48000.0 * 0.5);
}
