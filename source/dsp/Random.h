#pragma once
// xoshiro256** PRNG seeded through splitmix64. Deterministic across platforms.
#include <cstdint>

namespace gf
{
class Random
{
public:
    explicit Random (uint64_t s = 1) { seed (s); }

    void seed (uint64_t s)
    {
        for (auto& word : state)
        {
            s += 0x9e3779b97f4a7c15ull;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            word = z ^ (z >> 31);
        }
    }

    uint64_t next()
    {
        const uint64_t result = rotl (state[1] * 5, 7) * 9;
        const uint64_t t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl (state[3], 45);
        return result;
    }

    // Uniform in [0, 1).
    double uniform() { return (double) (next() >> 11) * 0x1.0p-53; }

    // Uniform integer in [0, n).
    int below (int n) { return n > 0 ? (int) (uniform() * n) : 0; }

private:
    static uint64_t rotl (uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    uint64_t state[4] {};
};
} // namespace gf
