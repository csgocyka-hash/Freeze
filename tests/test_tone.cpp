#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/Random.h"
#include "dsp/Tone.h"

namespace
{
constexpr double sr = 48000.0;

std::vector<std::vector<float>> runTone (const gf::ToneCharacter& t, std::vector<std::vector<float>> io, int blockSize = 512)
{
    gf::Tone tone;
    tone.prepare (sr, (int) io.size());
    tone.setCharacter (t);
    std::vector<float*> ptr (io.size());
    for (size_t pos = 0; pos < io[0].size(); pos += (size_t) blockSize)
    {
        for (size_t c = 0; c < io.size(); ++c)
            ptr[c] = io[c].data() + pos;
        tone.process (ptr.data(), (int) io.size(), (int) std::min<size_t> ((size_t) blockSize, io[0].size() - pos));
    }
    return io;
}

double levelAt (double freq, const gf::ToneCharacter& t)
{
    auto out = runTone (t, { sine (sr, 1.0, freq, 0.25f) })[0];
    return rms (out.data() + 24000, 24000) / rms (sine (sr, 1.0, freq, 0.25f).data() + 24000, 24000);
}
} // namespace

TEST (neutral_tone_changes_nothing)
{
    auto s = sine (sr, 1.0, 440.0, 0.5f);
    CHECK (runTone ({}, { s })[0] == s);
}

TEST (tone_eq_cuts_and_shelves)
{
    gf::ToneCharacter t;
    t.lowCutHz = 200.0f;
    t.highShelfHz = 4000.0f;
    t.highShelfDb = -6.0f;
    const double low = 20.0 * std::log10 (levelAt (60.0, t)), mid = 20.0 * std::log10 (levelAt (1000.0, t)), high = 20.0 * std::log10 (levelAt (12000.0, t));
    std::printf ("    60 Hz %.1f dB, 1 kHz %.1f dB, 12 kHz %.1f dB\n", low, mid, high);
    CHECK (low < -12.0);
    CHECK (std::abs (mid) < 1.0);
    CHECK (std::abs (high + 6.0) < 1.0);
}

TEST (tone_saturation_adds_harmonics_and_keeps_level)
{
    gf::ToneCharacter t;
    t.saturation = 0.3f;
    auto out = runTone (t, { sine (sr, 1.0, 220.0, 0.5f) })[0];
    const double third = toneLevel (out, 24000, 48000, 660.0, sr), fund = toneLevel (out, 24000, 48000, 220.0, sr);
    const double db = 20.0 * std::log10 (rms (out.data() + 24000, 24000) / (0.5 / std::sqrt (2.0)));
    std::printf ("    3rd harmonic %.4f of fundamental, level %+.2f dB\n", third / fund, db);
    CHECK (third / fund > 0.005);
    CHECK (std::abs (db) < 1.5);
}

TEST (tone_chorus_and_width_decorrelate_stereo)
{
    gf::ToneCharacter t;
    t.chorusMix = 0.3f;
    t.width = 1.6f;
    auto s = sine (sr, 2.0, 330.0, 0.4f);
    auto noise = s;
    gf::Random r (5);
    for (auto& x : noise)
        x = (float) (0.2 * (2.0 * r.uniform() - 1.0));
    auto out = runTone (t, { noise, noise });
    CHECK (out[0] != out[1]);
    double maxStepValue = 0.0;
    for (size_t i = 1; i < out[0].size(); ++i)
        maxStepValue = std::max (maxStepValue, (double) std::abs (out[0][i] - out[0][i - 1]));
    CHECK (std::isfinite (maxStepValue));
}

TEST (tone_is_block_size_independent)
{
    gf::ToneCharacter t;
    t.lowCutHz = 120.0f;
    t.highShelfDb = 2.0f;
    t.saturation = 0.2f;
    t.chorusMix = 0.25f;
    t.width = 1.3f;
    auto s = sine (sr, 1.0, 440.0, 0.4f);
    CHECK (runTone (t, { s, s }, 1) == runTone (t, { s, s }, 512));
}
