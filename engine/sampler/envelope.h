#pragma once

// A voice's gain envelopes, each a ramp of n samples (n from microseconds at the host rate, not rounded,
// as the recipes take seconds x 48000):
// - the attack, a raised-cosine fade-in, 0.5 - 0.5 cos (pi j / n), then 1 (parts.raised; attack_us 0:
//   none, the first sample at full level);
// - the release after note-off, a raised-cosine fall, 0.5 + 0.5 cos (pi j / n), over when j reaches n
//   (dsp48.taper, from the note-off sample);
// - a fade (a choke, a steal, a stop, a kit change) from the level it starts at, cos^2 (pi j / 2n) =
//   0.5 + 0.5 cos (pi j / n) or linear 1 - j / n, over when j reaches n (FUT02-04's chokes, FUT01's).
// The voice multiplies them. The cosines come from a rotation, two multiply-adds a sample with no
// cosine call, so 64 voices in their release cost little.

#include "kit/kit.h"

#include <cmath>
#include <cstdint>

namespace wubansia::engine
{
    inline constexpr double kPi = 3.14159265358979323846;

    // A time in microseconds as samples at rate, unrounded: 30000 us at 48 kHz is 1440.
    inline double samplesOf (std::int64_t us, double rate) noexcept
    {
        return static_cast<double> (us) * rate / 1.0e6;
    }

    // cos (pi j / n) for j = 0, 1, 2... by rotation.
    class CosineRamp
    {
    public:
        void start (double samples) noexcept
        {
            n = samples;
            j = 0.0;
            c = 1.0;
            s = 0.0;
            dc = n > 0.0 ? std::cos (kPi / n) : 1.0;
            ds = n > 0.0 ? std::sin (kPi / n) : 0.0;
        }

        double cosine() const noexcept { return c; }
        bool done() const noexcept { return j >= n; } // the ramp's end: j has reached n
        double position() const noexcept { return j; }
        double length() const noexcept { return n; }

        void advance() noexcept
        {
            const double nc = c * dc - s * ds;
            s = s * dc + c * ds;
            c = nc;
            j += 1.0;
        }

    private:
        double n = 0.0;
        double j = 0.0;
        double c = 1.0;
        double s = 0.0;
        double dc = 1.0;
        double ds = 0.0;
    };

    // How a fade falls.
    enum class FadeCurve : std::uint8_t {
        cos2, // cos^2 (pi j / 2n): FUT02-04's chokes; steals, stops and kit changes
        linear // 1 - j / n: FUT01's chokes
    };

    inline FadeCurve fadeCurveOf (ChokeCurve curve) noexcept
    {
        return curve == ChokeCurve::linear ? FadeCurve::linear : FadeCurve::cos2;
    }

    class Envelope
    {
    public:
        // A new voice: the attack over attackSamples (0: none), no release, no fade.
        void start (double attackSamples) noexcept
        {
            attacking = attackSamples > 0.0;
            if (attacking)
                attack.start (attackSamples);
            releasing = false;
            fading = false;
            fadeFrom = 1.0;
            over = false;
            last = attacking ? 0.0 : 1.0;
        }

        // The release starts at the next value (whose gain is 1 x the rest).
        void release (double releaseSamples) noexcept
        {
            if (releasing || over)
                return;
            if (releaseSamples <= 0.0)
            {
                over = true;
                return;
            }
            releasing = true;
            releaseRamp.start (releaseSamples);
        }

        // Takes a release back (restrike-continue on the release's first sample).
        void unrelease() noexcept { releasing = false; }

        bool released() const noexcept { return releasing; }

        // A fade from the current fade level over fadeSamples; a fade already under way that ends sooner
        // keeps going.
        void fade (double fadeSamples, FadeCurve curve) noexcept
        {
            if (over)
                return;
            if (fadeSamples <= 0.0)
            {
                over = true;
                return;
            }
            if (fading && fadeRamp.length() - fadeRamp.position() <= fadeSamples)
                return;
            const double level = fading ? fadeValue() : 1.0;
            fading = true;
            fadeCurve = curve;
            fadeFrom = level;
            fadeRamp.start (fadeSamples);
        }

        bool isFading() const noexcept { return fading; }

        // The next sample's gain, then one sample on. After it, over () may turn true.
        double next() noexcept
        {
            double g = 1.0;
            if (attacking)
            {
                g = 0.5 - 0.5 * attack.cosine();
                attack.advance();
                attacking = ! attack.done();
            }
            if (releasing)
            {
                g *= 0.5 + 0.5 * releaseRamp.cosine();
                releaseRamp.advance();
                if (releaseRamp.done())
                    over = true;
            }
            if (fading)
            {
                g *= fadeValue();
                fadeRamp.advance();
                if (fadeRamp.done())
                    over = true;
            }
            last = g;
            return g;
        }

        // Whether the envelope has reached its end: nothing after it sounds.
        bool isOver() const noexcept { return over; }

        // Neither attacking, releasing nor fading: next () would give 1 until an event changes that.
        bool isIdle() const noexcept { return ! attacking && ! releasing && ! fading && ! over; }

        // The last gain next () gave: the voice's level for choosing what to steal.
        double level() const noexcept { return last; }

    private:
        double fadeValue() const noexcept
        {
            if (fadeCurve == FadeCurve::linear)
                return fadeFrom * (1.0 - fadeRamp.position() / fadeRamp.length());
            return fadeFrom * (0.5 + 0.5 * fadeRamp.cosine());
        }

        CosineRamp attack;
        CosineRamp releaseRamp;
        CosineRamp fadeRamp;
        double fadeFrom = 1.0;
        double last = 1.0;
        FadeCurve fadeCurve = FadeCurve::cos2;
        bool attacking = false;
        bool releasing = false;
        bool fading = false;
        bool over = false;
    };
}
