#include "metrics/measures.h"

#include "metrics/int128.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace wubansia::metrics
{
    using util::floorDiv;
    using util::floorMod;

    std::int64_t permille (std::int64_t a, std::int64_t b) noexcept
    {
        if (b == 0)
            return -1;
        return floorDiv (2000 * a + b, 2 * b);
    }

    std::optional<std::int64_t> lowerMedian (std::vector<std::int64_t> values)
    {
        if (values.empty())
            return {};
        std::ranges::sort (values); // integers: every sort gives the same order
        return values[(values.size() - 1) / 2];
    }

    std::optional<std::int64_t> sdMilli (std::span<const std::int64_t> values)
    {
        if (values.empty())
            return {};
        // In doubles, in the audit's order: exact while the sums stay below 2^53 (any MIDI data).
        double s1 = 0;
        double s2 = 0;
        for (const std::int64_t v : values)
        {
            const auto d = static_cast<double> (v);
            s1 += d;
            s2 += d * d;
        }
        const auto n = static_cast<double> (values.size());
        const double variance = n * s2 - s1 * s1;
        return static_cast<std::int64_t> (std::floor ((1000.0 * std::sqrt (variance)) / n + 0.5));
    }

    std::int64_t countDistinct (std::vector<std::int64_t> values)
    {
        std::ranges::sort (values);
        return static_cast<std::int64_t> (std::ranges::distance (values.begin(), std::unique (values.begin(), values.end())));
    }

    std::optional<std::int64_t> lag1Permille (std::span<const std::int64_t> x)
    {
        using detail::Int128;
        const std::size_t n = x.size();
        if (n < 2 || n > (std::size_t { 1 } << 31))
            return {};
        // With |x| <= 2^10 and n <= 2^31 the sums fit in 64 bits (S1 < 2^42, S2 and P < 2^52) and
        // every term below in 128 (n^2 P < 2^114; the denominator < 2^114).
        std::int64_t s1 = 0, s2 = 0, p = 0, a = 0, b = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t v = x[i];
            if (v > 1024 || v < -1024)
                return {};
            s1 += v;
            s2 += v * v;
            if (i >= 1)
            {
                p += v * x[i - 1];
                a += v;
            }
            if (i + 2 <= n)
                b += v;
        }
        const Int128 nn = Int128::from (static_cast<std::int64_t> (n));
        const Int128 S1 = Int128::from (s1);
        const Int128 den = nn * (nn * Int128::from (s2) - S1 * S1);
        if (den == Int128::from (0))
            return {};
        const Int128 num = nn * nn * Int128::from (p) - nn * S1 * Int128::from (a + b) + (nn - Int128::from (1)) * S1 * S1;
        // num is the sum of c[i] c[i-1] and den the sum of c[i]^2 for c[i] = n x[i] - S1, so |num| <= den.
        return detail::permilleHalfUp (num, den);
    }

    std::optional<std::int64_t> dfaAlphaMilli (std::span<const std::int64_t> x)
    {
        // Exactly the audit's operations, in its order (README, "G7"); only log () may round differently.
        const std::size_t n = x.size();
        if (n < 64)
            return {};
        double mean = 0;
        for (const std::int64_t v : x)
            mean += static_cast<double> (v);
        mean /= static_cast<double> (n);
        std::vector<double> y (n);
        double acc = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
            acc += static_cast<double> (x[i]) - mean;
            y[i] = acc;
        }
        std::vector<double> ls, lf;
        for (std::size_t s = 4; s <= n / 4; s *= 2)
        {
            const std::size_t boxes = n / s;
            const double kbar = static_cast<double> (s - 1) / 2;
            double skk = 0;
            for (std::size_t k = 0; k < s; ++k)
                skk += (static_cast<double> (k) - kbar) * (static_cast<double> (k) - kbar);
            double total = 0;
            for (std::size_t j = 0; j < boxes; ++j)
            {
                const double* box = y.data() + j * s;
                double vbar = 0;
                for (std::size_t k = 0; k < s; ++k)
                    vbar += box[k];
                vbar /= static_cast<double> (s);
                double skv = 0;
                for (std::size_t k = 0; k < s; ++k)
                    skv += (static_cast<double> (k) - kbar) * (box[k] - vbar);
                const double slope = skv / skk;
                double rss = 0;
                for (std::size_t k = 0; k < s; ++k)
                {
                    const double r = box[k] - vbar - slope * (static_cast<double> (k) - kbar);
                    rss += r * r;
                }
                total += rss / static_cast<double> (s);
            }
            const double f2 = total / static_cast<double> (boxes);
            if (!(f2 > 0))
                return {};
            ls.push_back (std::log (static_cast<double> (s)));
            lf.push_back (0.5 * std::log (f2));
        }
        if (ls.size() < 3)
            return {};
        const auto m = static_cast<double> (ls.size());
        double mx = 0, my = 0;
        for (std::size_t i = 0; i < ls.size(); ++i)
        {
            mx += ls[i];
            my += lf[i];
        }
        mx /= m;
        my /= m;
        double sxy = 0, sxx = 0;
        for (std::size_t i = 0; i < ls.size(); ++i)
        {
            sxy += (ls[i] - mx) * (lf[i] - my);
            sxx += (ls[i] - mx) * (ls[i] - mx);
        }
        const double alpha = std::floor (1000 * (sxy / sxx) + 0.5);
        if (!std::isfinite (alpha) || std::fabs (alpha) > 1e15)
            return {}; // can't happen for finite data; never cast a NaN
        return static_cast<std::int64_t> (alpha);
    }

    int popcount12 (int mask) noexcept
    {
        int c = 0;
        for (int i = 0; i < 12; ++i)
            if ((mask & (1 << i)) != 0)
                ++c;
        return c;
    }

    int pitchClassMask (std::span<const Note> notes) noexcept
    {
        int m = 0;
        for (const auto& n : notes)
            m |= 1 << (n.key % 12);
        return m;
    }

    int rotateMask (int mask, int r) noexcept
    {
        int out = 0;
        for (int i = 0; i < 12; ++i)
            if ((mask & (1 << i)) != 0)
                out |= 1 << floorMod (i - r, 12);
        return out;
    }

    std::string maskHex (int mask)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string out;
        for (int shift = 8; shift >= 0; shift -= 4)
            out += digits[(mask >> shift) & 0xF];
        return out;
    }

    std::span<const Note> between (std::span<const Note> notes, std::int64_t t0, std::int64_t t1) noexcept
    {
        const auto byTick = [] (const Note& n, std::int64_t t) { return n.tick < t; };
        const auto lo = std::lower_bound (notes.begin(), notes.end(), t0, byTick);
        const auto hi = std::lower_bound (lo, notes.end(), t1, byTick);
        return { lo, hi };
    }

    std::vector<Stack> stacksOf (std::span<const Note> notes)
    {
        std::vector<Stack> out;
        std::size_t start = 0;
        for (std::size_t i = 1; i <= notes.size(); ++i)
            if (i == notes.size() || notes[i].tick - notes[start].tick > 10)
            {
                if (i > start)
                    out.push_back ({ notes[start].tick, notes.subspan (start, i - start) });
                start = i;
            }
        return out;
    }

    std::vector<HarmonyStack> harmonyStacks (const Song& song)
    {
        const auto notes = notesWithRole (song, Role::chords);
        std::vector<HarmonyStack> out;
        for (const auto& s : stacksOf (notes))
        {
            const int mask = pitchClassMask (s.notes);
            if (popcount12 (mask) < 2)
                continue; // a single note or octaves never define harmony
            int low = 127;
            for (const auto& n : s.notes)
                low = std::min<int> (low, n.key);
            out.push_back ({ s.tick, mask, low });
        }
        return out;
    }

    std::optional<std::vector<const HarmonyStack*>> harmonySlots (std::span<const HarmonyStack> stacks, std::int64_t w0, int nSlots)
    {
        std::vector<const HarmonyStack*> out (static_cast<std::size_t> (std::max (nSlots, 0)), nullptr);
        std::size_t k = 0;
        for (std::size_t j = 0; j < out.size(); ++j)
        {
            const std::int64_t start = w0 + 960 * static_cast<std::int64_t> (j);
            const std::int64_t lo = start - 250, hi = start + 710;
            while (k < stacks.size() && stacks[k].tick < lo)
                ++k;
            const HarmonyStack* best = nullptr;
            for (std::size_t q = k; q < stacks.size() && stacks[q].tick < hi; ++q)
                if (best == nullptr || std::llabs (stacks[q].tick - start) <= std::llabs (best->tick - start))
                    best = &stacks[q]; // on a tie the later wins
            out[j] = best;
        }
        const auto first = std::find_if (out.begin(), out.end(), [] (const HarmonyStack* s) { return s != nullptr; });
        if (first == out.end())
            return {};
        const auto firstIndex = static_cast<std::size_t> (first - out.begin());
        for (std::size_t j = 0; j < out.size(); ++j)
            if (out[j] == nullptr)
                out[j] = j < firstIndex ? out[firstIndex] : out[j - 1];
        return out;
    }

    const HarmonyStack* harmonyAt (std::span<const HarmonyStack> stacks, std::int64_t t) noexcept
    {
        if (stacks.empty())
            return nullptr;
        const auto it = std::upper_bound (stacks.begin(), stacks.end(), t, [] (std::int64_t v, const HarmonyStack& s) { return v < s.tick; });
        return it == stacks.begin() ? &stacks.back() : &*(it - 1);
    }

    std::vector<std::int64_t> oddOffsets (std::span<const Note> notes)
    {
        std::vector<std::int64_t> out;
        for (const auto& n : notes)
        {
            const std::int64_t r = floorMod (n.tick, std::int64_t { 240 });
            if (r >= 60 && r < 180)
                out.push_back (r - 120);
        }
        return out;
    }

    std::optional<std::int64_t> swingDelay (std::span<const Note> notes, std::size_t minCount)
    {
        auto o = oddOffsets (notes);
        if (o.size() < minCount || o.empty())
            return {};
        return lowerMedian (std::move (o));
    }

    std::int64_t distinctSwings (std::vector<std::int64_t> delays)
    {
        std::ranges::sort (delays);
        std::int64_t count = 0;
        for (std::size_t i = 0; i < delays.size();)
        {
            const std::int64_t start = delays[i];
            ++count;
            while (i < delays.size() && delays[i] <= start + 2)
                ++i;
        }
        return count;
    }

    std::optional<std::int64_t> residual (std::int64_t tick, std::int64_t delay) noexcept
    {
        const std::int64_t g = 240 * floorDiv (tick, std::int64_t { 240 });
        std::int64_t best = tick - g;
        for (const std::int64_t point : { g + 120 + delay, g + 240 })
        {
            const std::int64_t x = tick - point;
            if (std::llabs (x) < std::llabs (best))
                best = x; // a tie keeps the earlier point
        }
        if (std::llabs (best) > 30)
            return {};
        return best;
    }

    std::vector<std::int64_t> residuals (const Song& song, std::span<const Note> notes)
    {
        std::vector<std::int64_t> out;
        for (const auto& sec : sectionsOf (song))
        {
            const auto inSection = between (notes, sec.tick, sec.end);
            const std::int64_t d = swingDelay (inSection, 1).value_or (0);
            for (const auto& n : inSection)
                if (const auto r = residual (n.tick, d))
                    out.push_back (*r);
        }
        return out;
    }

    bool onSwungGrid (std::int64_t tick, std::int64_t delay, std::int64_t tol) noexcept
    {
        const std::int64_t r = floorMod (tick, std::int64_t { 240 });
        return std::llabs (r) <= tol || std::llabs (r - 240) <= tol || std::llabs (r - 120 - delay) <= tol;
    }

    bool onStraightGrid (std::int64_t tick, std::int64_t tol) noexcept
    {
        const std::int64_t r = floorMod (tick, std::int64_t { 120 });
        return r <= tol || 120 - r <= tol;
    }

    std::int64_t fileSwing (const Song& song)
    {
        std::vector<std::int64_t> o;
        for (const auto& t : song.tracks)
        {
            auto part = oddOffsets (t.notes);
            o.insert (o.end(), part.begin(), part.end());
        }
        return lowerMedian (std::move (o)).value_or (0);
    }

    std::vector<StepPrint> stepBars (std::span<const Note> notes, std::int64_t bar0, std::int64_t nBars)
    {
        std::vector<StepPrint> bars (static_cast<std::size_t> (std::max<std::int64_t> (nBars, 0)));
        for (const auto& n : notes)
        {
            const std::int64_t s = step16 (n.tick);
            const std::int64_t b = floorDiv (s, std::int64_t { 16 }) - bar0;
            if (b >= 0 && b < nBars)
                bars[static_cast<std::size_t> (b)].push_back (static_cast<std::int32_t> (floorMod (s, std::int64_t { 16 }) * 128 + n.key));
        }
        for (auto& b : bars)
        {
            std::ranges::sort (b);
            b.erase (std::unique (b.begin(), b.end()), b.end());
        }
        return bars;
    }

    std::vector<std::int64_t> macroChanges (std::span<const StepPrint> prints)
    {
        std::vector<std::int64_t> out;
        bool previous = false; // bar 7 never differs
        for (std::size_t i = 8; i < prints.size(); ++i)
        {
            const bool differs = prints[i] != prints[i - 8];
            if (differs && !previous)
                out.push_back (static_cast<std::int64_t> (i));
            previous = differs;
        }
        return out;
    }

    std::int64_t lhl (std::span<const std::int32_t> onsets)
    {
        static constexpr std::array<std::int64_t, 16> weight = { 0, -4, -3, -4, -2, -4, -3, -4, -1, -4, -3, -4, -2, -4, -3, -4 };
        const auto w = [] (std::int64_t step) { return weight[static_cast<std::size_t> (floorMod (step, std::int64_t { 16 }))]; };
        std::int64_t total = 0;
        for (std::size_t k = 0; k < onsets.size(); ++k)
        {
            const std::int64_t i = onsets[k];
            const std::int64_t j = k + 1 < onsets.size() ? onsets[k + 1] : onsets[0] + 16;
            bool any = false;
            std::int64_t best = 0;
            for (std::int64_t x = i + 1; x < j; ++x)
                if (!any || w (x) > best)
                {
                    best = w (x);
                    any = true;
                }
            if (any && best > w (i))
                total += best - w (i);
        }
        return total;
    }
}
