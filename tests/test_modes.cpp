#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

struct Setup
{
    gf::EngineMode mode = gf::EngineMode::normal;
    float octave = 0.0f;
    double rhythmBeats = 1.0;
    bool pulseUnlinked = false;
    float pulseTime = 0.0f;
    bool invert = false;
};

std::vector<float> render (const Setup& setup, std::vector<float> in, float amount, gf::FreezeEngine* out = nullptr)
{
    gf::FreezeEngine local;
    auto& e = out != nullptr ? *out : local;
    e.prepare (sr, 512, 1);
    e.setFloorLevel (0.0f);
    e.setTempo (120.0);
    e.setMode (setup.mode);
    e.setOctaveShare (setup.octave);
    e.setRhythmBeats (setup.rhythmBeats);
    e.setPulse (setup.pulseUnlinked, setup.pulseTime);
    e.setFollowInvert (setup.invert);
    std::vector<float> a (in.size(), amount);
    return runEngine (e, { std::move (in) }, a, 512)[0];
}
} // namespace

TEST (normal_mode_with_new_settings_off_is_unchanged)
{
    auto in = pluckTrain (sr, 3.0);
    gf::FreezeEngine plain;
    plain.prepare (sr, 512, 1);
    plain.setFloorLevel (0.0f);
    std::vector<float> a (in.size(), 0.8f);
    CHECK (render ({}, in, 0.8f) == runEngine (plain, { in }, a, 512)[0]);
}

TEST (octave_grains_add_the_octave_above)
{
    auto plain = render ({}, sine (sr, 4.0, 220.0, 0.5f), 1.0f);
    Setup octave;
    octave.octave = 0.35f;
    auto up = render (octave, sine (sr, 4.0, 220.0, 0.5f), 1.0f);
    const size_t from = (size_t) sr, to = up.size();
    const double before = bandLevel (plain, from, to, 440.0, sr) / bandLevel (plain, from, to, 220.0, sr);
    const double after = bandLevel (up, from, to, 440.0, sr) / bandLevel (up, from, to, 220.0, sr);
    std::printf ("    440 Hz vs 220 Hz: plain %.3f, octave %.3f\n", before, after);
    CHECK (after > 0.3);
    CHECK (after > before * 5.0);
}

TEST (rhythm_steps_on_the_division_without_clicks)
{
    Setup rhythm;
    rhythm.mode = gf::EngineMode::rhythm;
    rhythm.rhythmBeats = 1.0; // 1/4 at 120 BPM = every 0.5 s
    gf::FreezeEngine e;
    auto out = render (rhythm, sine (sr, 4.0, 440.0, 0.5f), 1.0f, &e);
    std::printf ("    steps in 4 s: %lld (expect 8)\n", (long long) e.rhythmSteps());
    CHECK (e.rhythmSteps() >= 7 && e.rhythmSteps() <= 9);
    CHECK (maxStep (out, (size_t) sr) < 4.0 * kTwoPi * 440.0 / sr * 0.5 * 1.5);
}

TEST (pulse_follows_amount_and_is_static_at_full)
{
    CHECK_NEAR (gf::FreezeEngine::pulseBeats (0.0f), 2.0, 1e-6);       // 1/2 note
    CHECK_NEAR (gf::FreezeEngine::pulseBeats (1.0f), 1.0 / 16.0, 1e-6); // 1/64 note
    Setup pulse;
    pulse.mode = gf::EngineMode::pulse;
    auto in = sine (sr, 4.0, 440.0, 0.5f);
    auto slow = render (pulse, in, 0.3f), full = render (pulse, in, 1.0f), plain = render ({}, in, 1.0f);
    // At 30 % the level swings; at 100 % the pulse is gone
    std::vector<double> env;
    for (size_t pos = (size_t) sr; pos + 480 <= slow.size(); pos += 480)
        env.push_back (rms (slow.data() + pos, 480));
    const double swing = *std::max_element (env.begin(), env.end()) / std::max (1e-9, *std::min_element (env.begin(), env.end()));
    std::printf ("    level swing at 30 %%: %.1fx\n", swing);
    CHECK (swing > 2.0);
    CHECK (full == plain);
}

TEST (pulse_unlinked_uses_its_own_time)
{
    Setup pulse;
    pulse.mode = gf::EngineMode::pulse;
    pulse.pulseUnlinked = true;
    pulse.pulseTime = 0.2f;
    auto in = sine (sr, 3.0, 440.0, 0.5f);
    auto atLow = render (pulse, in, 0.6f), atHigh = render (pulse, in, 1.0f);
    CHECK (atHigh != render ({}, in, 1.0f)); // still pulsing at full Amount when unlinked
    CHECK (atLow != atHigh);
}

TEST (follow_plays_only_with_the_input_or_inverted_only_without)
{
    auto in = pluckTrain (sr, 2.0, 220.0, 4.0, 0.5f, 0.15);
    in.resize ((size_t) (4 * sr), 0.0f); // 2 s of playing, 2 s of silence
    Setup follow;
    follow.mode = gf::EngineMode::follow;
    auto normal = render (follow, in, 1.0f);
    follow.invert = true;
    auto inverted = render (follow, in, 1.0f);
    const auto playing = (size_t) (1.0 * sr), silent = (size_t) (3.0 * sr);
    const double nPlay = rms (normal.data() + playing, 24000), nQuiet = rms (normal.data() + silent, 24000);
    const double iPlay = rms (inverted.data() + playing, 24000), iQuiet = rms (inverted.data() + silent, 24000);
    std::printf ("    follow: playing %.4f, silent %.4f; inverted: playing %.4f, silent %.4f\n", nPlay, nQuiet, iPlay, iQuiet);
    CHECK (nQuiet < nPlay * 0.05);
    CHECK (iQuiet > iPlay * 2.0);
}

TEST (modes_are_block_size_independent)
{
    for (auto mode : { gf::EngineMode::rhythm, gf::EngineMode::pulse, gf::EngineMode::follow })
    {
        Setup s;
        s.mode = mode;
        s.octave = 0.3f;
        auto in = pluckTrain (sr, 2.0);
        std::vector<float> a (in.size(), 0.9f);
        gf::FreezeEngine x, y;
        for (auto* e : { &x, &y })
        {
            e->prepare (sr, 512, 1);
            e->setTempo (120.0);
            e->setMode (mode);
            e->setOctaveShare (0.3f);
        }
        CHECK (runEngine (x, { in }, a, 1)[0] == runEngine (y, { in }, a, 512)[0]);
    }
}
