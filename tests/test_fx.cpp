#include "Check.h"
#include "Signals.h"
#include "dsp/Fx.h"
#include "dsp/Presets.h"
#include <cstring>

TEST (fx_impulse_has_delay_echo)
{
    gf::FxChain fx;
    fx.prepare (48000, 1);
    fx.setCharacter ({ 0.5f, 0.3f, 0.0f, 0.3f, 0.45f, 1.0f }); // delay only
    std::vector<float> out (48000);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = fx.processSample (0, i == 0 ? 1.0f : 0.0f);
    size_t peakAt = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if (std::abs (out[i]) > std::abs (out[peakAt]))
            peakAt = i;
    CHECK (std::abs ((double) peakAt - 0.3 * 48000) <= 2.0);
}

TEST (reverb_impulse_decays)
{
    gf::FxChain fx;
    fx.prepare (48000, 2);
    fx.setCharacter ({ 0.95f, 0.6f, 1.0f, 0.3f, 0.0f, 0.0f }); // biggest reverb, no delay
    std::vector<float> out (10 * 48000);
    bool finite = true;
    for (size_t i = 0; i < out.size(); ++i)
    {
        out[i] = fx.processSample (0, i == 0 ? 1.0f : 0.0f);
        finite = finite && std::isfinite (out[i]);
    }
    CHECK (finite);
    const double first = rms (out.data(), 48000), second = rms (out.data() + 48000, 48000);
    CHECK (first > 0.0);
    CHECK (second < first);
    CHECK (rms (out.data() + 9 * 48000, 48000) < first * 0.01);
}

TEST (grain_knob_mapping)
{
    CHECK_NEAR (gf::grainKnobToSeconds (0.0f), 0.040, 1e-9);
    CHECK_NEAR (gf::grainKnobToSeconds (1.0f), 0.500, 1e-9);
    CHECK_NEAR (gf::grainKnobToSeconds (gf::grainSecondsToKnob (0.12)), 0.12, 1e-6);
}

TEST (presets_table)
{
    const char* names[] = { "Ice", "Glass", "Haze", "Ember", "Abyss" };
    const double grains[] = { 0.120, 0.050, 0.300, 0.180, 0.500 };
    const double densities[] = { 6, 10, 6, 4, 4 };
    const float fxStarts[] = { 0.0f, 0.2f, 0.4f, 0.35f, 0.7f };
    CHECK (gf::kNumPresets == 5);
    for (int i = 0; i < gf::kNumPresets; ++i)
    {
        CHECK (std::strcmp (gf::preset (i).name, names[i]) == 0);
        CHECK_NEAR (gf::preset (i).grainSeconds, grains[i], 1e-9);
        CHECK_NEAR (gf::preset (i).density, densities[i], 1e-9);
        CHECK_NEAR (gf::preset (i).fx, fxStarts[i], 1e-6);
    }
    CHECK_NEAR (gf::preset (3).character.delaySeconds, 0.3, 1e-6);
    CHECK_NEAR (gf::preset (4).character.delaySeconds, 0.6, 1e-6);
    CHECK (gf::preset (0).character.delayLevel == 0.0f);
}
