#include "Check.h"
#include "Measures.h"
#include "Signals.h"
#include "dsp/Snowfall.h"

namespace
{
constexpr double sr = 48000.0;

// The noise alone (on silence), stereo, at a fixed Amount and Noise level
std::vector<std::vector<float>> snow (float amount, double seconds, float noise = 1.0f)
{
    gf::Snowfall s;
    s.prepare (sr, 2);
    std::vector<std::vector<float>> io (2, std::vector<float> ((size_t) (sr * seconds), 0.0f));
    std::vector<float> a (io[0].size(), amount), n (io[0].size(), noise);
    for (size_t pos = 0; pos < io[0].size(); pos += 512)
    {
        float* ch[2] = { io[0].data() + pos, io[1].data() + pos };
        s.process (ch, 2, (int) std::min<size_t> (512, io[0].size() - pos), a.data() + pos, n.data() + pos);
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
    std::vector<float> a (in.size(), 1.0f), noise (in.size(), 0.0f);
    float* ch = io.data();
    s.process (&ch, 1, (int) io.size(), a.data(), noise.data());
    CHECK (io == in);
}

TEST (noise_level_follows_the_noise_control)
{
    const double full = rms (snow (1.0f, 2.0, 1.0f)[0].data(), (size_t) (2 * sr));
    const double half = rms (snow (1.0f, 2.0, 0.5f)[0].data(), (size_t) (2 * sr));
    const double none = rms (snow (1.0f, 2.0, 0.0f)[0].data(), (size_t) (2 * sr));
    std::printf ("    noise 100 / 50 / 0 %%: %.5f %.5f %.5f\n", full, half, none);
    CHECK_NEAR (half / full, 0.5, 0.05);
    CHECK (none == 0.0);
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

TEST (snowfall_is_stereo)
{
    auto x = snow (1.0f, 2.0);
    CHECK (x[0] != x[1]);
}
