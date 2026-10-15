#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
// Plucks for 2 s, then hold switches to `holdOn` and a loud 880 Hz tone plays for 3 s.
std::vector<float> runHoldScenario (bool holdOn)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto in = pluckTrain (48000, 2.0, 220.0);
    const auto tone = sine (48000, 3.0, 880.0, 0.5f);
    const size_t switchAt = in.size();
    in.insert (in.end(), tone.begin(), tone.end());
    std::vector<float> amount (in.size(), 1.0f);
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        e.setHold (holdOn && pos >= switchAt);
        float* chunk = in.data() + pos;
        e.process (&chunk, 1, (int) std::min<size_t> (512, in.size() - pos), amount.data() + pos);
    }
    return in;
}
} // namespace

TEST (hold_ignores_new_input)
{
    auto out = runHoldScenario (true);
    const size_t from = 3 * 48000, to = 5 * 48000; // well after the tone started
    const double old = toneLevel (out, from, to, 220.0, 48000), fresh = toneLevel (out, from, to, 880.0, 48000);
    std::printf ("    held: 220 Hz %.5f, 880 Hz %.5f\n", old, fresh);
    CHECK (old > fresh * 10.0);
}

TEST (without_hold_the_freeze_follows_new_input)
{
    auto out = runHoldScenario (false);
    const size_t from = 3 * 48000, to = 5 * 48000;
    const double old = toneLevel (out, from, to, 220.0, 48000), fresh = toneLevel (out, from, to, 880.0, 48000);
    std::printf ("    not held: 220 Hz %.5f, 880 Hz %.5f\n", old, fresh);
    CHECK (fresh > old * 10.0);
}

TEST (hold_stops_recording)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto p = pluckTrain (48000, 1.0);
    std::vector<float> amount (p.size(), 1.0f);
    runEngine (e, { p }, amount, 512);
    const auto before = e.recordedSamples (0);
    e.setHold (true);
    runEngine (e, { p }, amount, 512);
    CHECK (e.recordedSamples (0) == before);
    e.setHold (false);
    runEngine (e, { p }, amount, 512);
    CHECK (e.recordedSamples (0) > before);
}

TEST (hold_sustains_indefinitely)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto in = pluckTrain (48000, 2.0);
    in.resize (12 * 48000, 0.0f);
    std::vector<float> amount (in.size(), 1.0f);
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        e.setHold (pos >= 2 * 48000);
        float* chunk = in.data() + pos;
        e.process (&chunk, 1, (int) std::min<size_t> (512, in.size() - pos), amount.data() + pos);
    }
    const double early = rms (in.data() + 48000, 48000), late = rms (in.data() + 11 * 48000, 48000);
    std::printf ("    playing %.4f, 9 s into hold %.4f\n", early, late);
    CHECK (late > early * 0.5);
    CHECK (late < early * 2.0);
}

TEST (hold_toggle_has_no_clicks)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 5.0, 440, 0.5f);
    std::vector<float> amount (s.size(), 1.0f);
    for (size_t pos = 0; pos < s.size(); pos += 512)
    {
        e.setHold (pos >= 2 * 48000 && pos < 3 * 48000);
        float* chunk = s.data() + pos;
        e.process (&chunk, 1, (int) std::min<size_t> (512, s.size() - pos), amount.data() + pos);
    }
    const double maxSlope = kTwoPi * 440.0 / 48000.0 * 0.5;
    CHECK (maxStep (s, 48000) < 4.0 * maxSlope);
}
