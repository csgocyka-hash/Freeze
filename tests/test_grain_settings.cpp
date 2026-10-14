#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/Presets.h"

namespace
{
std::vector<float> ones (size_t n, float v = 1.0f) { return std::vector<float> (n, v); }

std::vector<float> freezeWith (double grain, double density, const std::vector<float>& input,
                               const std::vector<float>* fx = nullptr, const gf::FxCharacter* character = nullptr,
                               int blockSize = 512)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    e.setGrainSeconds (grain);
    e.setDensity (density);
    if (character != nullptr)
        e.setFxCharacter (*character);
    std::vector<std::vector<float>> io { input };
    auto amount = ones (input.size());
    std::vector<float*> ptr (1);
    for (size_t pos = 0; pos < input.size(); pos += (size_t) blockSize)
    {
        const int n = (int) std::min ((size_t) blockSize, input.size() - pos);
        ptr[0] = io[0].data() + pos;
        e.process (ptr.data(), 1, n, amount.data() + pos, fx != nullptr ? fx->data() + pos : nullptr);
    }
    return io[0];
}
} // namespace

TEST (fills_gaps_all_grain_settings)
{
    auto p = pluckTrain (48000, 5.0);
    for (double grain : { 0.04, 0.12, 0.5 })
        for (double density : { 4.0, 6.0, 10.0 })
        {
            std::printf ("    grain %.2f s, density %.0f:\n", grain, density);
            checkGapsFilled (freezeWith (grain, density, p), 48000);
        }
}

TEST (level_all_grain_settings)
{
    auto p = pluckTrain (48000, 5.0);
    const double ref = soundingRms (p, 48000);
    for (double grain : { 0.04, 0.12, 0.5 })
        for (double density : { 4.0, 6.0, 10.0 })
        {
            auto out = freezeWith (grain, density, p);
            const double db = 20.0 * std::log10 (rms (out.data() + 48000, out.size() - 48000) / ref);
            std::printf ("    grain %.2f s, density %.0f: %+.2f dB\n", grain, density, db);
            CHECK (db > -4.0);
            CHECK (db < 4.0);
        }
}

TEST (fx_zero_matches_no_fx)
{
    auto p = pluckTrain (48000, 3.0);
    auto zeros = ones (p.size(), 0.0f);
    const auto& abyss = gf::preset (4).character;
    CHECK (freezeWith (0.12, 6, p, &zeros, &abyss) == freezeWith (0.12, 6, p));
}

TEST (amount_zero_bit_identical_with_fx)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    e.setFxCharacter (gf::preset (4).character);
    auto p = pluckTrain (48000, 3.0);
    auto io = p;
    auto amount = ones (p.size(), 0.0f), fx = ones (p.size());
    float* ptr = io.data();
    for (size_t pos = 0; pos < p.size(); pos += 512)
    {
        float* chunk = ptr + pos;
        e.process (&chunk, 1, (int) std::min<size_t> (512, p.size() - pos), amount.data() + pos, fx.data() + pos);
    }
    CHECK (io == p);
}

TEST (fx_level_bounded)
{
    auto p = pluckTrain (48000, 6.0);
    auto full = ones (p.size());
    const double dry = rms (freezeWith (0.12, 6, p).data() + 48000, p.size() - 48000);
    for (int i = 0; i < gf::kNumPresets; ++i)
    {
        const auto& pr = gf::preset (i);
        auto out = freezeWith (pr.grainSeconds, pr.density, p, &full, &pr.character);
        bool finite = true;
        for (float x : out)
            finite = finite && std::isfinite (x);
        const double db = 20.0 * std::log10 (rms (out.data() + 48000, out.size() - 48000) / dry);
        std::printf ("    %s at FX 100 %%: %+.2f dB vs no FX\n", pr.name, db);
        CHECK (finite);
        CHECK (db > -6.0);
        CHECK (db < 6.0);
    }
}

TEST (fx_block_size_independent)
{
    auto p = pluckTrain (48000, 3.0);
    auto full = ones (p.size());
    const auto& ember = gf::preset (3).character;
    auto a = freezeWith (0.18, 4, p, &full, &ember, 1);
    auto b = freezeWith (0.18, 4, p, &full, &ember, 64);
    auto c = freezeWith (0.18, 4, p, &full, &ember, 512);
    CHECK (a == b);
    CHECK (b == c);
}

TEST (no_clicks_on_grain_knob_move)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 4.0, 440, 0.5f);
    auto amount = ones (s.size());
    for (size_t pos = 0; pos < s.size(); pos += 512)
    {
        // Sweep the grain length from 40 ms to 500 ms between 1 s and 3 s
        const double t = std::clamp (((double) pos / 48000.0 - 1.0) / 2.0, 0.0, 1.0);
        e.setGrainSeconds (gf::grainKnobToSeconds ((float) t));
        float* chunk = s.data() + pos;
        e.process (&chunk, 1, (int) std::min<size_t> (512, s.size() - pos), amount.data() + pos);
    }
    const double maxSlope = kTwoPi * 440.0 / 48000.0 * 0.5;
    const double step = maxStep (s, 48000);
    std::printf ("    max step %.4f, bound %.4f\n", step, 4.0 * maxSlope);
    CHECK (step < 4.0 * maxSlope);
}
