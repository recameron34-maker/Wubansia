#pragma once

// The C++ side of tools/sound/export_engine_vectors.py (P8): the same generated inputs, bit for bit, and the stored
// outputs, each checked against vectors.sha256 before it is used. Keep music () and the inputs in step with the
// script.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace fxtest
{
    struct SplitMix64
    {
        std::uint64_t s;

        std::uint64_t next() noexcept
        {
            s += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }

        double uniform() noexcept { return static_cast<double> (next() >> 11) * (1.0 / 9007199254740992.0); }
    };

    struct Burst
    {
        int start;
        int length;
        double amp;
    };

    // export_engine_vectors.music: n frames of `channels` interleaved float32.
    std::vector<float> music (int n, std::uint64_t seed, std::initializer_list<Burst> bursts, double peak, double brown = 1.0,
        int channels = 2, int silent = 0);

    std::vector<float> inputA(); // 1024 stereo, -6 dBFS
    std::vector<float> inputB(); // 2048 stereo, -6 dBFS

    // One channel of an interleaved buffer.
    std::vector<float> channel (const std::vector<float>& interleaved, int channels, int c);

    // engine/tests/vectors/fx/<name>.f32, after checking its SHA-256 against vectors.sha256 (a failed check throws).
    std::vector<float> load (std::string_view name);

    // The largest |a[i] - b[i]| over their common length.
    double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b);
}
