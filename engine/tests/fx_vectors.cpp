#include "fx_vectors.h"

#include "util/sha256.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fxtest
{
    namespace
    {
        std::string vectorDir() { return std::string (WUBANSIA_SOURCE_DIR) + "/engine/tests/vectors/fx/"; }

        std::vector<std::uint8_t> readBytes (const std::string& path)
        {
            std::ifstream in (path, std::ios::binary);
            if (! in)
                throw std::runtime_error ("can't read " + path);
            return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
        }

        const std::map<std::string, std::string>& hashes()
        {
            static const std::map<std::string, std::string> table = [] {
                std::map<std::string, std::string> t;
                const auto bytes = readBytes (vectorDir() + "vectors.sha256");
                std::istringstream lines (std::string (bytes.begin(), bytes.end()));
                std::string hash, name;
                while (lines >> hash >> name)
                    t[name] = hash;
                return t;
            }();
            return table;
        }
    }

    std::vector<float> music (int n, std::uint64_t seed, std::initializer_list<Burst> bursts, double peak, double brown, int channels,
        int silent)
    {
        SplitMix64 rng { seed };
        const auto size = static_cast<std::size_t> (n) * static_cast<std::size_t> (channels);
        std::vector<double> x (size, 0.0);
        std::vector<double> b (static_cast<std::size_t> (channels), 0.0);
        for (std::size_t i = 0; i < static_cast<std::size_t> (n); ++i)
            for (std::size_t c = 0; c < static_cast<std::size_t> (channels); ++c)
            {
                const double w = 2.0 * rng.uniform() - 1.0;
                b[c] = 0.995 * b[c] + 0.05 * w;
                x[i * static_cast<std::size_t> (channels) + c] = brown * b[c];
            }
        for (const auto& burst : bursts)
            for (int i = burst.start; i < std::min (n, burst.start + burst.length); ++i)
            {
                const double e = burst.amp * (1.0 - static_cast<double> (i - burst.start) / static_cast<double> (burst.length));
                for (std::size_t c = 0; c < static_cast<std::size_t> (channels); ++c)
                    x[static_cast<std::size_t> (i) * static_cast<std::size_t> (channels) + c] += e * (2.0 * rng.uniform() - 1.0);
            }
        std::fill_n (x.begin(), static_cast<std::size_t> (std::max (silent, 0)) * static_cast<std::size_t> (channels), 0.0);
        double m = 0.0;
        for (const double v : x)
            m = std::max (m, std::abs (v));
        const double scale = peak / m;
        std::vector<float> out (size);
        for (std::size_t i = 0; i < size; ++i)
            out[i] = static_cast<float> (x[i] * scale);
        return out;
    }

    std::vector<float> inputA() { return music (1024, 1, { { 0, 300, 0.8 }, { 400, 200, 1.0 }, { 700, 300, 0.6 } }, 0.5); }

    std::vector<float> inputB() { return music (2048, 2, { { 100, 500, 1.0 }, { 1200, 400, 0.7 } }, 0.5); }

    std::vector<float> channel (const std::vector<float>& interleaved, int channels, int c)
    {
        const auto ch = static_cast<std::size_t> (channels);
        std::vector<float> out (interleaved.size() / ch);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = interleaved[i * ch + static_cast<std::size_t> (c)];
        return out;
    }

    std::vector<float> load (std::string_view name)
    {
        static_assert (std::endian::native == std::endian::little, "the vectors are little-endian float32");
        const std::string file = std::string (name) + ".f32";
        const auto bytes = readBytes (vectorDir() + file);
        const auto& table = hashes();
        const auto it = table.find (file);
        if (it == table.end())
            throw std::runtime_error (file + " is not in vectors.sha256");
        if (wubansia::util::sha256Hex (bytes) != it->second)
            throw std::runtime_error (file + " doesn't match its SHA-256 in vectors.sha256");
        std::vector<float> out (bytes.size() / sizeof (float));
        std::memcpy (out.data(), bytes.data(), out.size() * sizeof (float));
        return out;
    }

    double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
    {
        double m = 0.0;
        const std::size_t n = std::min (a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i)
            m = std::max (m, std::abs (static_cast<double> (a[i]) - static_cast<double> (b[i])));
        return m;
    }
}
