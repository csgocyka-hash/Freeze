#pragma once
// Small in-place radix-2 complex FFT. Size must be a power of two; tables are built in prepare().
#include <cmath>
#include <complex>
#include <vector>

namespace gf
{
class Fft
{
public:
    void prepare (int size)
    {
        n = size;
        twiddles.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            twiddles[(size_t) k] = std::polar (1.0f, -6.2831853f * (float) k / (float) n);
        bitReversed.resize ((size_t) n);
        int bits = 0;
        while ((1 << bits) < n)
            ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                r |= ((i >> b) & 1) << (bits - 1 - b);
            bitReversed[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    // inverse = true computes the unscaled inverse transform.
    void perform (std::complex<float>* data, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < bitReversed[(size_t) i])
                std::swap (data[i], data[bitReversed[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < len / 2; ++k)
                {
                    auto w = twiddles[(size_t) (k * step)];
                    if (inverse)
                        w = std::conj (w);
                    const auto a = data[i + k], b = data[i + k + len / 2] * w;
                    data[i + k] = a + b;
                    data[i + k + len / 2] = a - b;
                }
        }
    }

private:
    int n = 0;
    std::vector<std::complex<float>> twiddles;
    std::vector<int> bitReversed;
};
} // namespace gf
