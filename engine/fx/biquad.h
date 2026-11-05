#pragma once

// Biquads in double (ADR 0021). The designs are the Python's, at any host rate:
// - butterworth (): scipy.signal.butter (order, f, kind, fs = rate, output = 'sos'), as dsp48.bfilt, lr4 and
//   mono_below call it: the analog prototype's poles, prewarped (4 tan (pi f / fs)), moved by lp2lp, lp2hp or
//   lp2bp, mapped by the bilinear transform, then grouped into sections (conjugate poles together, the zeros at
//   z = -1 and z = +1, the gain in the first section, the poles nearest the unit circle last);
// - rbj (): the RBJ cookbook's peak (dsp48.peaking), shelves, low-pass and high-pass.
// Sections run in transposed direct form II, the form scipy's sosfilt and lfilter use, so the same coefficients
// give the same numbers to rounding.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <utility>

namespace wubansia::engine::fx
{
    // y = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2) x. The default passes the signal unchanged.
    struct BiquadCoeffs
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

    struct BiquadState
    {
        double z1 = 0.0;
        double z2 = 0.0;
    };

    // One section, transposed direct form II.
    inline double biquadTick (const BiquadCoeffs& c, BiquadState& s, double x) noexcept
    {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    enum class PassKind : std::uint8_t {
        lowpass,
        highpass,
        bandpass
    };

    inline constexpr int kMaxButterworthOrder = 8;

    // A Butterworth filter of `order` (1-8) at f1 Hz (and f2 for a band-pass, f1 < f2), sampled at fs, as scipy
    // designs it. Writes its sections to out (a low- or high-pass of order N takes (N + 1) / 2 sections, a band-pass
    // N) and returns how many; 0 if the arguments are out of range (f at or over Nyquist...). Frequencies are
    // clamped into (0, 0.499 fs).
    inline int butterworth (PassKind kind, int order, double f1, double f2, double fs, BiquadCoeffs* out, int capacity) noexcept
    {
        using C = std::complex<double>;
        if (order < 1 || order > kMaxButterworthOrder || ! (fs > 0.0) || out == nullptr)
            return 0;
        const int sections = kind == PassKind::bandpass ? order : (order + 1) / 2;
        if (sections > capacity)
            return 0;

        const double nyquistGuard = 0.499 * fs;
        const auto warp = [&] (double f) { return 4.0 * std::tan (std::numbers::pi * std::clamp (f, 1e-6, nyquistGuard) / fs); };

        // The analog prototype: p = -exp (i pi m / (2 N)), m = -N + 1, -N + 3, ..., N - 1; no zeros, gain 1.
        std::array<C, kMaxButterworthOrder> proto {};
        for (int j = 0; j < order; ++j)
        {
            const double m = static_cast<double> (-order + 1 + 2 * j);
            proto[static_cast<std::size_t> (j)] = -std::exp (C (0.0, std::numbers::pi * m / (2.0 * order)));
        }

        // The transformed analog poles, and the gain the bilinear map leaves (scipy's k_z).
        std::array<C, 2 * kMaxButterworthOrder> poles {};
        int numPoles = 0;
        double gain = 1.0;
        C denominator (1.0, 0.0); // prod (4 - p)
        switch (kind)
        {
            case PassKind::lowpass:
            {
                const double w = warp (f1);
                for (int j = 0; j < order; ++j)
                    poles[static_cast<std::size_t> (numPoles++)] = w * proto[static_cast<std::size_t> (j)];
                gain = std::pow (w, order);
                break;
            }
            case PassKind::highpass:
            {
                const double w = warp (f1);
                C prodMinusP (1.0, 0.0);
                for (int j = 0; j < order; ++j)
                {
                    const C p = proto[static_cast<std::size_t> (j)];
                    poles[static_cast<std::size_t> (numPoles++)] = w / p;
                    prodMinusP *= -p;
                }
                gain = (C (1.0, 0.0) / prodMinusP).real() * std::pow (4.0, order); // lp2hp's, times prod (4 - 0)
                break;
            }
            case PassKind::bandpass:
            {
                const double w1 = warp (std::min (f1, f2));
                const double w2 = warp (std::max (f1, f2));
                const double bw = w2 - w1;
                const double wo2 = w1 * w2;
                for (int j = 0; j < order; ++j)
                {
                    const C pl = proto[static_cast<std::size_t> (j)] * (bw / 2.0);
                    const C root = std::sqrt (pl * pl - wo2);
                    poles[static_cast<std::size_t> (j)] = pl + root;
                    poles[static_cast<std::size_t> (j + order)] = pl - root;
                }
                numPoles = 2 * order;
                gain = std::pow (bw, order) * std::pow (4.0, order);
                break;
            }
        }
        for (int j = 0; j < numPoles; ++j)
            denominator *= 4.0 - poles[static_cast<std::size_t> (j)];
        gain *= (C (1.0, 0.0) / denominator).real();

        // The digital poles, then sections: each pole with Im > 0 with its conjugate; real poles two by two (a
        // lone one, of an odd low- or high-pass, as a first-order section).
        std::array<C, 2 * kMaxButterworthOrder> zp {};
        for (int j = 0; j < numPoles; ++j)
        {
            const C p = poles[static_cast<std::size_t> (j)];
            zp[static_cast<std::size_t> (j)] = (4.0 + p) / (4.0 - p);
        }

        struct Section
        {
            double a1;
            double a2;
            double radius;
            bool firstOrder;
        };
        std::array<Section, 2 * kMaxButterworthOrder> found {};
        int count = 0;
        std::array<double, 2 * kMaxButterworthOrder> reals {};
        int numReals = 0;
        for (int j = 0; j < numPoles; ++j)
        {
            const C p = zp[static_cast<std::size_t> (j)];
            if (std::abs (p.imag()) <= 1e-12 * std::max (1.0, std::abs (p)))
                reals[static_cast<std::size_t> (numReals++)] = p.real();
            else if (p.imag() > 0.0)
                found[static_cast<std::size_t> (count++)] = { -2.0 * p.real(), p.real() * p.real() + p.imag() * p.imag(), std::abs (p), false };
        }
        std::sort (reals.begin(), reals.begin() + numReals);
        for (int j = 0; j + 1 < numReals; j += 2)
        {
            const double r1 = reals[static_cast<std::size_t> (j)];
            const double r2 = reals[static_cast<std::size_t> (j + 1)];
            found[static_cast<std::size_t> (count++)] = { -(r1 + r2), r1 * r2, std::max (std::abs (r1), std::abs (r2)), false };
        }
        if (numReals % 2 != 0)
        {
            const double r = reals[static_cast<std::size_t> (numReals - 1)];
            found[static_cast<std::size_t> (count++)] = { -r, 0.0, std::abs (r), true };
        }
        if (count != sections)
            return 0;
        // the poles nearest the unit circle last (a stable insertion sort: std::stable_sort may allocate)
        for (int i = 1; i < count; ++i)
            for (int j = i; j > 0 && found[static_cast<std::size_t> (j)].radius < found[static_cast<std::size_t> (j - 1)].radius; --j)
                std::swap (found[static_cast<std::size_t> (j)], found[static_cast<std::size_t> (j - 1)]);

        for (int s = 0; s < count; ++s)
        {
            const auto& f = found[static_cast<std::size_t> (s)];
            BiquadCoeffs c;
            c.a1 = f.a1;
            c.a2 = f.a2;
            switch (kind)
            {
                case PassKind::lowpass: // zeros at -1
                    c.b0 = 1.0;
                    c.b1 = f.firstOrder ? 1.0 : 2.0;
                    c.b2 = f.firstOrder ? 0.0 : 1.0;
                    break;
                case PassKind::highpass: // zeros at +1
                    c.b0 = 1.0;
                    c.b1 = f.firstOrder ? -1.0 : -2.0;
                    c.b2 = f.firstOrder ? 0.0 : 1.0;
                    break;
                case PassKind::bandpass: // one zero at +1, one at -1
                    c.b0 = 1.0;
                    c.b1 = 0.0;
                    c.b2 = -1.0;
                    break;
            }
            if (s == 0)
            {
                c.b0 *= gain;
                c.b1 *= gain;
                c.b2 *= gain;
            }
            out[s] = c;
        }
        return count;
    }

    enum class RbjKind : std::uint8_t {
        lowpass,
        highpass,
        peak,
        lowShelf,
        highShelf
    };

    // The RBJ cookbook biquad at fc Hz, gainDb (peak and shelves), q, sampled at fs; normalised so a0 = 1.
    inline BiquadCoeffs rbj (RbjKind kind, double fc, double gainDb, double q, double fs) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * std::numbers::pi * std::clamp (fc, 1e-6, 0.499 * fs) / fs;
        const double cw = std::cos (w0);
        const double al = std::sin (w0) / (2.0 * std::max (q, 1e-6));
        const double sa = 2.0 * std::sqrt (A) * al;
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;
        switch (kind)
        {
            case RbjKind::lowpass:
                b0 = (1.0 - cw) / 2.0;
                b1 = 1.0 - cw;
                b2 = (1.0 - cw) / 2.0;
                a0 = 1.0 + al;
                a1 = -2.0 * cw;
                a2 = 1.0 - al;
                break;
            case RbjKind::highpass:
                b0 = (1.0 + cw) / 2.0;
                b1 = -(1.0 + cw);
                b2 = (1.0 + cw) / 2.0;
                a0 = 1.0 + al;
                a1 = -2.0 * cw;
                a2 = 1.0 - al;
                break;
            case RbjKind::peak:
                b0 = 1.0 + al * A;
                b1 = -2.0 * cw;
                b2 = 1.0 - al * A;
                a0 = 1.0 + al / A;
                a1 = -2.0 * cw;
                a2 = 1.0 - al / A;
                break;
            case RbjKind::lowShelf:
                b0 = A * ((A + 1.0) - (A - 1.0) * cw + sa);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
                b2 = A * ((A + 1.0) - (A - 1.0) * cw - sa);
                a0 = (A + 1.0) + (A - 1.0) * cw + sa;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
                a2 = (A + 1.0) + (A - 1.0) * cw - sa;
                break;
            case RbjKind::highShelf:
                b0 = A * ((A + 1.0) + (A - 1.0) * cw + sa);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
                b2 = A * ((A + 1.0) + (A - 1.0) * cw - sa);
                a0 = (A + 1.0) - (A - 1.0) * cw + sa;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
                a2 = (A + 1.0) - (A - 1.0) * cw - sa;
                break;
        }
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    // A cascade of up to MaxSections sections on Channels channels, each with its own state.
    template <int MaxSections, int Channels>
    class BiquadCascade
    {
        static constexpr auto kSections = static_cast<std::size_t> (MaxSections);
        static constexpr auto kChannels = static_cast<std::size_t> (Channels);

    public:
        // Keeps the state when the sections stay as many (a coefficient change), clears it otherwise.
        void setSections (const BiquadCoeffs* sections, int count) noexcept
        {
            const int n = std::clamp (count, 0, MaxSections);
            if (n != numSections)
                reset();
            numSections = n;
            for (int s = 0; s < n; ++s)
                coeffs[static_cast<std::size_t> (s)] = sections[s];
        }

        void setButterworth (PassKind kind, int order, double f1, double f2, double fs) noexcept
        {
            std::array<BiquadCoeffs, kSections> c {};
            setSections (c.data(), butterworth (kind, order, f1, f2, fs, c.data(), MaxSections));
        }

        // The same section twice (an LR4 is a 2nd-order Butterworth squared).
        void setRepeated (const BiquadCoeffs& c, int times) noexcept
        {
            std::array<BiquadCoeffs, kSections> cs {};
            const int n = std::clamp (times, 0, MaxSections);
            for (int s = 0; s < n; ++s)
                cs[static_cast<std::size_t> (s)] = c;
            setSections (cs.data(), n);
        }

        void reset() noexcept { state = {}; }

        int sections() const noexcept { return numSections; }

        double tick (int channel, double x) noexcept
        {
            auto& st = state[static_cast<std::size_t> (channel)];
            for (int s = 0; s < numSections; ++s)
                x = biquadTick (coeffs[static_cast<std::size_t> (s)], st[static_cast<std::size_t> (s)], x);
            return x;
        }

        // In place over n float samples of one channel.
        void process (int channel, float* x, int n) noexcept
        {
            if (numSections == 0)
                return;
            for (int i = 0; i < n; ++i)
                x[i] = static_cast<float> (tick (channel, static_cast<double> (x[i])));
        }

    private:
        std::array<BiquadCoeffs, kSections> coeffs {};
        std::array<std::array<BiquadState, kSections>, kChannels> state {};
        int numSections = 0;
    };

    // The 2nd-order Butterworth section an LR4 crossover squares (dsp48.lr4).
    inline BiquadCoeffs lr4Section (PassKind kind, double fc, double fs) noexcept
    {
        std::array<BiquadCoeffs, 1> c {};
        butterworth (kind, 2, fc, 0.0, fs, c.data(), 1);
        return c[0];
    }
}
