#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

// 220 Hz plucks (4 harmonics, tau 80 ms) that start with a short 3.5 kHz "pick" burst (tau 5 ms)
std::vector<float> pickedPlucks (double seconds)
{
    std::vector<float> out ((size_t) (sr * seconds), 0.0f);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = std::fmod ((double) i / sr, 0.25);
        if (t < 0.2)
        {
            double v = 0.3 * std::exp (-t / 0.005) * std::sin (kTwoPi * 3500.0 * t);
            for (int h = 1; h <= 4; ++h)
                v += 0.25 / h * std::exp (-t / 0.08) * std::sin (kTwoPi * 220.0 * h * t);
            out[i] = (float) v;
        }
    }
    return out;
}

std::vector<float> freezeFull (const std::vector<float>& in, float smoothness)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setMaxSmoothness (smoothness); // (the floor under the smooth cloud reads the same copy, so it counts too)
    std::vector<float> a (in.size(), 1.0f);
    return runEngine (e, { in }, a, 512)[0];
}

double attackShare (const std::vector<float>& x)
{
    const size_t from = (size_t) (2 * sr), to = x.size();
    return bandLevel (x, from, to, 3500.0, sr) / bandLevel (x, from, to, 220.0, sr);
}
} // namespace

TEST (smooth_mode_freezes_the_body_not_the_attack)
{
    // The pick burst is the plucks' attack. The plain cloud replays it; the smooth cloud should hold only the
    // notes' bodies (at least 20 dB less pick than the plain cloud) and keep their pitch and level.
    auto in = pickedPlucks (8.0);
    auto plain = freezeFull (in, 0.0f), smooth = freezeFull (in, 1.0f);
    const size_t from = (size_t) (2 * sr);
    const double db = 20.0 * std::log10 (rms (smooth.data() + from, smooth.size() - from) / soundingRms (in, sr));
    std::printf ("    pick / body: plain %.4f, smooth %.4f; smooth level %+.2f dB\n", attackShare (plain), attackShare (smooth), db);
    CHECK (attackShare (smooth) < attackShare (plain) * 0.1);
    CHECK (db > -4.0 && db < 4.0);
}
