#pragma once

// Delay lines (ADR 0021).
// - History<T, Size>: the last Size samples in a fixed array, readable as a contiguous run (each sample is written
//   twice, so the window never wraps): the FIRs' tap lines.
// - DelayLine<T>: a ring sized in prepare (), read by an integer delay, or at a fractional position with sends.py's
//   linear interpolation (the echo's tape). Positions are absolute sample indices (int64), counted from the stream's
//   start; nothing before it was ever written, and reads there give 0.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubansia::engine::fx
{
    // The smallest power of two at or over n (n >= 1).
    inline std::size_t ceilPow2 (std::size_t n) noexcept
    {
        std::size_t p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }

    template <class T, std::size_t Size>
    class History
    {
        static_assert ((Size & (Size - 1)) == 0, "Size is a power of two");

    public:
        void reset() noexcept
        {
            buf.fill (T {});
            pos = 0;
        }

        void push (T x) noexcept
        {
            pos = (pos + 1) & (Size - 1);
            buf[pos] = x;
            buf[pos + Size] = x;
        }

        // newest ()[-i] is the sample pushed i pushes ago, for i < Size.
        const T* newest() const noexcept { return buf.data() + pos + Size; }

    private:
        std::array<T, 2 * Size> buf {};
        std::size_t pos = 0;
    };

    template <class T>
    class DelayLine
    {
    public:
        // Allocates room for delays up to maxDelay samples (and fractional reads up to maxDelay - 1).
        void prepare (std::size_t maxDelay)
        {
            buf.assign (ceilPow2 (maxDelay + 2), T {});
            mask = buf.size() - 1;
            reset();
        }

        void reset() noexcept
        {
            std::fill (buf.begin(), buf.end(), T {});
            written = 0;
        }

        std::size_t capacity() const noexcept { return buf.empty() ? 0 : buf.size() - 2; }

        // The next sample, at absolute index written ().
        void push (T x) noexcept
        {
            buf[static_cast<std::size_t> (written) & mask] = x;
            ++written;
        }

        std::int64_t size() const noexcept { return written; }

        // The sample at absolute index i (0 before the stream or past what is kept).
        T at (std::int64_t i) const noexcept
        {
            if (i < 0 || i >= written || written - i > static_cast<std::int64_t> (buf.size()))
                return T {};
            return buf[static_cast<std::size_t> (i) & mask];
        }

        // The sample at absolute index i, unchecked: i must be under size () and less than capacity () back. Before the
        // stream (i < 0) it reads the ring's still untouched end, which is 0 until the ring first wraps.
        T tap (std::int64_t i) const noexcept { return buf[static_cast<std::size_t> (i) & mask]; }

        // The sample pushed `delay` pushes before the last one (0: the last).
        T delayed (std::int64_t delay) const noexcept { return at (written - 1 - delay); }

        // sends._read: v at a fractional stream position, (1 - fr) v[i] + fr v[i + 1], i = floor (pos); 0 where i < 0.
        // The stream's sample i is the line's absolute index i + offset.
        double read (double pos, std::int64_t offset = 0) const noexcept
        {
            const double fl = std::floor (pos);
            const auto i = static_cast<std::int64_t> (fl);
            if (i < 0)
                return 0.0;
            const double fr = pos - fl;
            return (1.0 - fr) * static_cast<double> (at (i + offset)) + fr * static_cast<double> (at (i + 1 + offset));
        }

        // read () without the range checks: pos must be within capacity () of the newest sample.
        double readFast (double pos, std::int64_t offset = 0) const noexcept
        {
            const double fl = std::floor (pos);
            const auto i = static_cast<std::int64_t> (fl);
            if (i < 0)
                return 0.0;
            const double fr = pos - fl;
            return (1.0 - fr) * static_cast<double> (tap (i + offset)) + fr * static_cast<double> (tap (i + 1 + offset));
        }

    private:
        std::vector<T> buf;
        std::size_t mask = 0;
        std::int64_t written = 0;
    };

    // A short fixed delay of up to Capacity samples on a stereo block, in place (latency compensation, bypass paths).
    template <std::size_t Capacity>
    class BlockDelay
    {
        static_assert ((Capacity & (Capacity - 1)) == 0, "Capacity is a power of two");

    public:
        static constexpr int kMaxDelay = static_cast<int> (Capacity) - 1;

        void setDelay (int samples) noexcept { delay = std::clamp (samples, 0, kMaxDelay); }
        int delaySamples() const noexcept { return delay; }

        void reset() noexcept
        {
            for (auto& c : ring)
                c.fill (0.0f);
            pos = 0;
        }

        // Delays l and r (r may be null) by delaySamples (), in place.
        void process (float* l, float* r, int n) noexcept
        {
            if (delay == 0)
                return;
            float* ch[2] { l, r };
            const std::size_t start = pos;
            for (int c = 0; c < 2; ++c)
            {
                if (ch[c] == nullptr)
                    continue;
                auto& rg = ring[static_cast<std::size_t> (c)];
                std::size_t p = start;
                for (int i = 0; i < n; ++i)
                {
                    const float in = ch[c][i];
                    ch[c][i] = rg[(p - static_cast<std::size_t> (delay)) & (Capacity - 1)];
                    rg[p] = in;
                    p = (p + 1) & (Capacity - 1);
                }
            }
            pos = (start + static_cast<std::size_t> (std::max (n, 0))) & (Capacity - 1);
        }

    private:
        std::array<std::array<float, Capacity>, 2> ring {};
        std::size_t pos = 0;
        int delay = 0;
    };
}
