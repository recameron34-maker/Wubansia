#include "fx/echo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr double kGlideS = 0.050; // a tempo change glides the tape over about this long
        constexpr double kMaxWobble = 0.04; // wow + flutter, a fraction of the delay (the manifest's 2 % each)
    }

    void Echo::prepare (double sampleRate)
    {
        fs = sampleRate;
        const double longest = kMaxSteps * 60.0 / (4.0 * kMinBpm) * fs * (1.0 + kMaxWobble);
        a.prepare (static_cast<std::size_t> (std::ceil (longest)) + 4);
        b.prepare (static_cast<std::size_t> (std::ceil (longest)) + 4);
        glide = std::exp (-1.0 / (kGlideS * fs));
        setup (s);
    }

    void Echo::setup (const EchoSettings& settings) noexcept
    {
        s = settings;
        std::array<BiquadCoeffs, 2> c {};
        butterworth (PassKind::lowpass, 2, s.lpHz, 0.0, fs, c.data(), 1);
        butterworth (PassKind::highpass, 2, s.hpHz, 0.0, fs, c.data() + 1, 1);
        loop.setSections (c.data(), 2);
        reset();
    }

    void Echo::reset() noexcept
    {
        a.reset();
        b.reset();
        loop.reset();
        t = -origin;
        synced = false;
        primed = false;
        current = target;
    }

    void Echo::sync (Oscillator& o, double hz, double phase) const noexcept
    {
        const double w = 2.0 * std::numbers::pi * hz;
        const double angle = w * (static_cast<double> (t) / fs) + phase;
        o.sin = std::sin (angle);
        o.cos = std::cos (angle);
        o.stepSin = std::sin (w / fs);
        o.stepCos = std::cos (w / fs);
    }

    void Echo::setDelaySeconds (double seconds) noexcept
    {
        const double longest = static_cast<double> (a.capacity()) / (1.0 + kMaxWobble) - 2.0;
        target = std::clamp (seconds * fs, 2.0, std::max (longest, 2.0));
        if (! primed)
        {
            current = target;
            primed = true;
        }
    }

    void Echo::setTempo (double bpm) noexcept
    {
        if (! (bpm > 0.0))
            return;
        setDelaySeconds (static_cast<double> (s.steps) * 60.0 / (4.0 * bpm));
    }

    void Echo::process (const float* in, float* outL, float* outR, int n) noexcept
    {
        const double drive = std::max (s.drive, 1e-9);
        for (int i = 0; i < n; ++i)
        {
            if (current < target || target < current)
            {
                current = target + (current - target) * glide;
                if (std::abs (current - target) < 1e-9)
                    current = target;
            }
            if (! synced || t % kResync == 0)
            {
                sync (wowOsc, s.wowHz, s.wowPhase);
                sync (flutterOsc, s.flutterHz, s.flutterPhase);
                synced = true;
            }
            const double wobble = s.wow * wowOsc.sin + s.flutter * flutterOsc.sin;
            for (auto* o : { &wowOsc, &flutterOsc })
            {
                const double sn = o->sin * o->stepCos + o->cos * o->stepSin;
                o->cos = o->cos * o->stepCos - o->sin * o->stepSin;
                o->sin = sn;
            }
            const double pos = static_cast<double> (t) - current * (1.0 + wobble);
            const double ta = loop.tick (0, std::tanh (drive * a.readFast (pos, origin)) / drive);
            const double tb = loop.tick (1, std::tanh (drive * b.readFast (pos, origin)) / drive);
            outL[i] = static_cast<float> (ta);
            outR[i] = static_cast<float> (tb);
            b.push (static_cast<float> (s.feedback * ta));
            a.push (static_cast<float> (static_cast<double> (in[i]) + s.feedback * tb));
            ++t;
        }
    }
}
