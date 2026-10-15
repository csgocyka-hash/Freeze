#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/Snowfall.h"

namespace
{
constexpr double sr = 48000.0;

// Snowfall alone (on silence), stereo, at a fixed Amount
std::vector<std::vector<float>> snow (float amount, double seconds, bool enabled = true)
{
    gf::Snowfall s;
    s.prepare (sr, 2);
    s.setEnabled (enabled, true);
    std::vector<std::vector<float>> io (2, std::vector<float> ((size_t) (sr * seconds), 0.0f));
    std::vector<float> a (io[0].size(), amount);
    for (size_t pos = 0; pos < io[0].size(); pos += 512)
    {
        float* ch[2] = { io[0].data() + pos, io[1].data() + pos };
        s.process (ch, 2, (int) std::min<size_t> (512, io[0].size() - pos), a.data() + pos);
    }
    return io;
}

// Average level near frequency f over many short Hann-windowed blocks (a stable estimate for noise; the
// window keeps energy at other frequencies from leaking into the measurement)
double noiseLevelAt (const std::vector<float>& x, double f)
{
    constexpr size_t block = 4800;
    std::vector<float> windowed (block);
    double sum = 0.0;
    int blocks = 0;
    for (size_t pos = 0; pos + block <= x.size(); pos += block, ++blocks)
    {
        for (size_t i = 0; i < block; ++i)
            windowed[i] = x[pos + i] * (float) (0.5 - 0.5 * std::cos (kTwoPi * (double) i / (double) block));
        sum += toneLevel (windowed, 0, block, f, sr);
    }
    return sum / blocks;
}
} // namespace

TEST (snowfall_off_leaves_audio_untouched)
{
    gf::Snowfall s;
    s.prepare (sr, 1);
    auto in = sine (sr, 1.0, 440.0, 0.5f), io = in;
    std::vector<float> a (in.size(), 1.0f);
    float* ch = io.data();
    s.process (&ch, 1, (int) io.size(), a.data());
    CHECK (io == in);
}

TEST (snowfall_level_follows_amount)
{
    const double atZero = rms (snow (0.0f, 2.0)[0].data(), (size_t) (2 * sr));
    const double atHalf = rms (snow (0.5f, 2.0)[0].data(), (size_t) (2 * sr));
    const double atFull = rms (snow (1.0f, 2.0)[0].data(), (size_t) (2 * sr));
    std::printf ("    rms at 0 / 50 / 100 %%: %.5f %.5f %.5f (%.1f dBFS at 100 %%)\n", atZero, atHalf, atFull, 20.0 * std::log10 (atFull));
    CHECK (atZero == 0.0);
    CHECK (atHalf < atFull * 0.7);
    CHECK (std::abs (20.0 * std::log10 (atFull) + 32.0) < 2.0);
}

TEST (snowfall_has_no_low_end)
{
    auto x = snow (1.0f, 10.0)[0];
    const double low = std::max (noiseLevelAt (x, 100.0), noiseLevelAt (x, 250.0)), mid = noiseLevelAt (x, 3000.0);
    std::printf ("    100-250 Hz %.6f vs 3 kHz %.6f\n", low, mid);
    CHECK (low < mid * 0.03);
}

TEST (snowfall_sweeps_brighter_with_amount)
{
    auto dull = snow (0.3f, 6.0)[0], bright = snow (1.0f, 6.0)[0];
    const double dullRatio = noiseLevelAt (dull, 10000.0) / noiseLevelAt (dull, 1500.0);
    const double brightRatio = noiseLevelAt (bright, 10000.0) / noiseLevelAt (bright, 1500.0);
    std::printf ("    10 kHz / 1.5 kHz: at 30 %% %.3f, at 100 %% %.3f\n", dullRatio, brightRatio);
    CHECK (brightRatio > dullRatio * 4.0);
}

TEST (snowfall_is_stereo_and_fades_in)
{
    auto x = snow (1.0f, 2.0);
    CHECK (x[0] != x[1]);

    gf::Snowfall s;
    s.prepare (sr, 1);
    s.setEnabled (true, false); // switched on while playing: fades in
    std::vector<float> io ((size_t) sr, 0.0f), a (io.size(), 1.0f);
    float* ch = io.data();
    s.process (&ch, 1, (int) io.size(), a.data());
    const double firstMs = rms (io.data(), 48), steady = rms (io.data() + 24000, 24000);
    std::printf ("    first ms %.5f, steady %.5f\n", firstMs, steady);
    CHECK (firstMs < steady * 0.2);
}
