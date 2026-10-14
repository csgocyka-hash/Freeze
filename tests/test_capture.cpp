#include "Check.h"
#include "Signals.h"
#include "dsp/Random.h"

TEST (capture_skips_silence)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    std::vector<float> silent (48000, 0.0f), amt (48000, 0.0f);
    runEngine (e, { silent }, amt, 512);
    CHECK (e.recordedSamples (0) == 0);
}

TEST (capture_records_sound)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto s = sine (48000, 1.0, 220, 0.5f);
    std::vector<float> amt (s.size(), 0.0f);
    runEngine (e, { s }, amt, 512);
    CHECK (e.recordedSamples (0) == 48000); // 100 whole 480-sample segments
}

TEST (capture_drops_gaps_in_plucks)
{
    gf::FreezeEngine e;
    e.prepare (48000, 512, 1);
    auto p = pluckTrain (48000, 2.0);
    std::vector<float> amt (p.size(), 0.0f);
    runEngine (e, { p }, amt, 512);
    CHECK (e.recordedSamples (0) > 0);
    CHECK (e.recordedSamples (0) < (int64_t) p.size() * 3 / 4); // silent gaps not recorded
}

TEST (capture_is_block_size_independent)
{
    auto p = pluckTrain (48000, 2.0);
    std::vector<float> amt (p.size(), 0.0f);
    int64_t counts[3];
    const int sizes[3] = { 1, 64, 512 };
    for (int i = 0; i < 3; ++i)
    {
        gf::FreezeEngine e;
        e.prepare (48000, 512, 1);
        runEngine (e, { p }, amt, sizes[i]);
        counts[i] = e.recordedSamples (0);
    }
    CHECK (counts[0] == counts[1]);
    CHECK (counts[1] == counts[2]);
}

TEST (random_is_deterministic)
{
    gf::Random a (42), b (42);
    for (int i = 0; i < 1000; ++i)
        CHECK (a.next() == b.next());
}
