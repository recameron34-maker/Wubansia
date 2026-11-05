#include "fx/oversampler_exact.h"

#include "fx/generated/os_fir4.h"

#include <cstddef>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr int kTaps = static_cast<int> (kOsFir4.size()); // 193
        constexpr int kCentre = (kTaps - 1) / 2; // 96

        // The up direction's four phases: phase j holds 4 h[j + 4 m], m = 0, 1, ...
        struct Phases
        {
            std::array<std::array<double, 49>, 4> taps {};
            std::array<int, 4> count {};
        };

        constexpr Phases makePhases() noexcept
        {
            Phases p {};
            for (int j = 0; j < 4; ++j)
            {
                int m = 0;
                for (int k = j; k < kTaps; k += 4)
                    p.taps[static_cast<std::size_t> (j)][static_cast<std::size_t> (m++)] = 4.0 * kOsFir4[static_cast<std::size_t> (k)];
                p.count[static_cast<std::size_t> (j)] = m;
            }
            return p;
        }

        constexpr Phases kPhases = makePhases();
    }

    void ExactUpsampler4::push (double x, double* out) noexcept
    {
        history.push (x);
        const double* h = history.newest();
        for (std::size_t j = 0; j < 4; ++j)
        {
            const auto& taps = kPhases.taps[j];
            const int count = kPhases.count[j];
            double acc = 0.0;
            for (int m = 0; m < count; ++m)
                acc += taps[static_cast<std::size_t> (m)] * h[-m];
            out[j] = acc;
        }
    }

    double ExactDownsampler4::push (const double* in) noexcept
    {
        history.push (in[0]);
        const double* z = history.newest();
        // y = sum h[k] z[-k], the taps symmetric about the centre
        double acc = kOsFir4[static_cast<std::size_t> (kCentre)] * z[-kCentre];
        for (int k = 0; k < kCentre; ++k)
            acc += kOsFir4[static_cast<std::size_t> (k)] * (z[-k] + z[-(kTaps - 1 - k)]);
        history.push (in[1]);
        history.push (in[2]);
        history.push (in[3]);
        return acc;
    }
}
