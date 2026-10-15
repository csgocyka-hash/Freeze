#include "Check.h"
#include "Measures.h"
#include "Signals.h"

namespace
{
constexpr double sr = 48000.0;

std::vector<float> freezeAt (float amount, std::vector<float> input, float spectralShare = 1.0f)
{
    gf::FreezeEngine e;
    e.prepare (sr, 512, 1);
    e.setMaxSpectralShare (spectralShare);
    std::vector<float> a (input.size(), amount);
    return runEngine (e, { std::move (input) }, a, 512)[0];
}
} // namespace

TEST (full_amount_is_one_steady_note)
{
    auto out = freezeAt (1.0f, pluckTrain (sr, 8.0));
    const double variation = envelopeVariation (out, (size_t) (2 * sr));
    const double skew = onsetSkew (std::vector<float> (out.begin() + (long) sr, out.end()));
    std::printf ("    envelope variation %.3f, onset skew %.2f\n", variation, skew);
    CHECK (variation < 0.15);
    CHECK (std::abs (skew) < 0.6);
}

TEST (full_amount_keeps_the_pitch)
{
    auto out = freezeAt (1.0f, pluckTrain (sr, 6.0, 220.0));
    const size_t from = (size_t) (2 * sr), to = out.size();
    const double at220 = bandLevel (out, from, to, 220.0, sr);
    const double off = std::max ({ bandLevel (out, from, to, 165.0, sr), bandLevel (out, from, to, 290.0, sr),
                                   bandLevel (out, from, to, 440.0, sr) });
    std::printf ("    220 Hz %.5f, strongest other %.5f\n", at220, off);
    CHECK (at220 > off * 10.0);
}

TEST (full_amount_level_matches_input)
{
    auto in = pluckTrain (sr, 8.0);
    auto out = freezeAt (1.0f, in);
    const double db = 20.0 * std::log10 (rms (out.data() + (size_t) (2 * sr), out.size() - (size_t) (2 * sr)) / soundingRms (in, sr));
    std::printf ("    level %+.2f dB\n", db);
    CHECK (db > -4.0 && db < 4.0);
}

TEST (full_amount_follows_new_notes)
{
    auto in = pluckTrain (sr, 3.0, 220.0);
    auto later = pluckTrain (sr, 5.0, 330.0);
    in.insert (in.end(), later.begin(), later.end());
    auto out = freezeAt (1.0f, in);
    const size_t from = (size_t) (6.5 * sr), to = out.size();
    const double newNote = bandLevel (out, from, to, 330.0, sr), oldNote = bandLevel (out, from, to, 220.0, sr);
    std::printf ("    after the change: 330 Hz %.5f, 220 Hz %.5f\n", newNote, oldNote);
    CHECK (newNote > oldNote * 5.0);
}

TEST (below_half_amount_is_pure_grains)
{
    auto in = pluckTrain (sr, 3.0);
    CHECK (freezeAt (0.45f, in) == freezeAt (0.45f, in, 0.0f));
}

TEST (spectral_freeze_has_no_clicks_and_is_silent_without_input)
{
    auto s = sine (sr, 4.0, 440.0, 0.5f);
    auto out = freezeAt (1.0f, s);
    CHECK (maxStep (out, (size_t) sr) < 4.0 * kTwoPi * 440.0 / sr * 0.5);

    auto silent = freezeAt (1.0f, std::vector<float> ((size_t) (2 * sr), 0.0f));
    CHECK (rms (silent.data(), silent.size()) == 0.0);
}
