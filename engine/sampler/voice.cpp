#include "sampler/voice.h"

#include "sampler/sinc_bank.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine
{
    namespace
    {
        // A looping voice's position is brought back by whole loops once it is this far past the loop's
        // end. Until then it runs on as the recipes' running sums do, so the vectors match bit for bit.
        constexpr double kWrapSpan = 1048576.0; // 2^20 frames, about 22 s at 48 kHz

#if defined(__GNUC__) || defined(__clang__)
    #define WUBANSIA_INLINE [[gnu::always_inline]] inline
#else
    #define WUBANSIA_INLINE inline
#endif

        // The sinc read of one or two channels at pos >= 0 through map (silence outside the file). Inlined
        // into the voice's loops: as a call it costs a third of the read.
        template <bool stereo>
        WUBANSIA_INLINE void sincRead2 (const float* table, const FrameMap& map, const float* dl, const float* dr, double pos, double& l,
            double& r) noexcept
        {
            const auto i = static_cast<std::int64_t> (pos);
            float c[SincBank::kTaps];
            sincCoefficients (table, pos - static_cast<double> (i), c);
            const std::int64_t first = i - SincBank::kLeft;
            if (first >= 0 && first + SincBank::kTaps <= map.direct())
            {
                l = sincDot (c, dl + first);
                r = stereo ? static_cast<double> (sincDot (c, dr + first)) : l;
                return;
            }
            float xl[SincBank::kTaps];
            float xr[SincBank::kTaps];
            for (int j = 0; j < SincBank::kTaps; ++j)
            {
                const std::int64_t m = map (first + j);
                xl[j] = m < 0 ? 0.0f : dl[m];
                xr[j] = stereo && m >= 0 ? dr[m] : 0.0f;
            }
            l = sincDot (c, xl);
            r = stereo ? static_cast<double> (sincDot (c, xr)) : l;
        }
    }

    void Voice::start (const VoiceStart& s) noexcept
    {
        kit = s.kit;
        source = s.source;
        instrument = s.instrument;
        trigger = s.trigger;
        key = s.key;
        root = s.root;
        layerVelocity = s.layerVelocity;
        lane = s.lane;
        mixChannel = s.mixChannel;
        order = s.order;
        keyUpAt = -1;

        sample = s.sample;
        interp = s.interp;
        oneShot = s.mode == SampleMode::oneShot;
        map = FrameMap {};
        map.frames = sample.frames;
        if (s.mode == SampleMode::sustainLoop && sample.loopEnd > sample.loopStart)
        {
            map.loopStart = sample.loopStart;
            map.loopEnd = sample.loopEnd;
        }
        map.clampStart = interp == Interp::hermite;

        baseRatio = s.baseRatio;
        unitBase = s.unitBase;
        semitones = s.semitones;
        targetSemitones = s.semitones;
        gliding = false;
        glideCurve = GlideCurve::none;
        setRatio();
        position = s.startPosition;
        everFractional = ! (unitBase && s.semitones == 0) || position - std::floor (position) > 0.0;
        delay = std::max<std::int64_t> (0, s.delay);

        releaseSamples = s.releaseSamples;
        envelope.start (s.attackSamples);
        velocityGain = s.velocityGain;
        gain = s.gain;
        panLeft = s.panLeft;
        panRight = s.panRight;
        throwSpace = s.throwSpace;
        throwEcho = s.throwEcho;
        keyDown = true;
        active = sample.left != nullptr && sample.frames > 0;
    }

    void Voice::setRatio() noexcept
    {
        ratio = baseRatio * std::pow (2.0, semitones / 12.0);
        bank = SincBank::instance().bankFor (ratio);
    }

    void Voice::noteOff (std::int64_t when) noexcept
    {
        if (! keyDown)
            return;
        keyDown = false;
        keyUpAt = when;
        if (! oneShot)
            envelope.release (releaseSamples);
    }

    void Voice::restrike() noexcept
    {
        keyDown = true;
        keyUpAt = -1;
        envelope.unrelease();
    }

    void Voice::glide (int toSemitones, double toVelocityGain, GlideCurve curve, double samples) noexcept
    {
        targetSemitones = toSemitones;
        if (curve == GlideCurve::none || samples <= 0.0)
        {
            gliding = false;
            semitones = toSemitones;
            velocityGain = toVelocityGain;
            setRatio();
            everFractional = everFractional || ! (unitBase && toSemitones == 0);
            return;
        }
        glideFromSemitones = semitones;
        glideFromGain = velocityGain;
        glideToGain = toVelocityGain;
        glideCurve = curve;
        glideSamples = samples;
        glideAt = 0.0;
        gliding = true;
        everFractional = true;
    }

    void Voice::fade (double samples, FadeCurve curve) noexcept
    {
        envelope.fade (samples, curve);
    }

    void Voice::render (const VoiceTarget& target, int from, int to) noexcept
    {
        if (! active || from >= to)
            return;
        if (delay > 0)
        {
            const auto skip = std::min<std::int64_t> (delay, to - from);
            delay -= skip;
            from += static_cast<int> (skip);
            if (from >= to)
                return;
        }
        const bool stereo = sample.channels == 2 && sample.right != sample.left;
        if (! gliding && ! everFractional)
            stereo ? dispatch<Reader::copy, true> (target, from, to) : dispatch<Reader::copy, false> (target, from, to);
        else if (interp == Interp::hermite)
            stereo ? dispatch<Reader::hermite, true> (target, from, to) : dispatch<Reader::hermite, false> (target, from, to);
        else
            stereo ? dispatch<Reader::sinc, true> (target, from, to) : dispatch<Reader::sinc, false> (target, from, to);
    }

    template <Voice::Reader reader, bool stereo>
    void Voice::dispatch (const VoiceTarget& target, int from, int to) noexcept
    {
        if (envelope.isIdle() && ! gliding && throwSpace <= 0.0f && throwEcho <= 0.0f)
            runSteady<reader, stereo> (target, from, to);
        else
            run<reader, stereo> (target, from, to);
    }

    template <Voice::Reader reader, bool stereo>
    void Voice::runSteady (const VoiceTarget& target, int from, int to) noexcept
    {
        // The same arithmetic as run () with an envelope of 1: g = 1 x velocityGain x gain.
        const float* table = SincBank::instance().table (bank);
        const float* dl = sample.left;
        const float* dr = sample.right;
        const auto end = static_cast<double> (sample.frames);
        const bool loops = map.loops();
        const auto loopEnd = static_cast<double> (map.loopEnd);
        const auto loopLength = static_cast<double> (map.loopEnd - map.loopStart);
        const double g = 1.0 * velocityGain * gain;
        const double gl = g * panLeft;
        const double gr = g * panRight;
        const double step = ratio;
        double pos = position;
        float* left = target.left;
        float* right = target.right;

        for (int i = from; i < to; ++i)
        {
            if (! loops && pos >= end)
            {
                position = pos;
                active = false;
                return;
            }
            double l = 0.0;
            double r = 0.0;
            if constexpr (reader == Reader::copy)
            {
                const std::int64_t k = map (static_cast<std::int64_t> (pos));
                l = dl[k];
                r = stereo ? static_cast<double> (dr[k]) : l;
            }
            else if constexpr (reader == Reader::hermite)
            {
                l = hermiteRead (dl, map, pos);
                r = stereo ? hermiteRead (dr, map, pos) : l;
            }
            else
            {
                sincRead2<stereo> (table, map, dl, dr, pos, l, r);
            }
            left[i] += static_cast<float> (l * gl);
            right[i] += static_cast<float> (r * gr);
            pos += step;
            if (loops && pos >= loopEnd + kWrapSpan)
                pos -= loopLength * std::floor ((pos - loopEnd) / loopLength);
        }
        position = pos;
    }

    template <Voice::Reader reader, bool stereo>
    void Voice::run (const VoiceTarget& target, int from, int to) noexcept
    {
        const SincBank& sinc = SincBank::instance();
        const float* table = sinc.table (bank);
        const float* dl = sample.left;
        const float* dr = sample.right;
        const auto end = static_cast<double> (sample.frames);
        const bool loops = map.loops();
        const auto loopEnd = static_cast<double> (map.loopEnd);
        const auto loopLength = static_cast<double> (map.loopEnd - map.loopStart);
        const bool throws = throwSpace > 0.0f || throwEcho > 0.0f;

        for (int i = from; i < to; ++i)
        {
            if (! loops && position >= end)
            {
                active = false;
                return;
            }

            double l = 0.0;
            double r = 0.0;
            if constexpr (reader == Reader::copy)
            {
                const std::int64_t k = map (static_cast<std::int64_t> (position));
                l = dl[k];
                r = stereo ? static_cast<double> (dr[k]) : l;
            }
            else if constexpr (reader == Reader::hermite)
            {
                l = hermiteRead (dl, map, position);
                r = stereo ? hermiteRead (dr, map, position) : l;
            }
            else
            {
                sincRead2<stereo> (table, map, dl, dr, position, l, r);
            }

            const double env = envelope.next();
            if (gliding)
            {
                double w = 1.0;
                bool done = false;
                const double u = glideAt / glideSamples;
                switch (glideCurve)
                {
                    case GlideCurve::raisedCosine:
                        done = u >= 1.0;
                        w = done ? 1.0 : 0.5 - 0.5 * std::cos (kPi * u);
                        break;
                    case GlideCurve::linear:
                        done = u >= 1.0;
                        w = done ? 1.0 : u;
                        break;
                    case GlideCurve::exponential:
                    {
                        const double e = std::exp (-u);
                        done = e < 1.0e-9;
                        w = 1.0 - e;
                        break;
                    }
                    case GlideCurve::none:
                        done = true;
                        break;
                }
                const auto goal = static_cast<double> (targetSemitones);
                semitones = done ? goal : glideFromSemitones + (goal - glideFromSemitones) * w;
                velocityGain = done ? glideToGain : glideFromGain + (glideToGain - glideFromGain) * w;
                gliding = ! done;
                ratio = baseRatio * std::pow (2.0, semitones / 12.0);
                bank = sinc.follow (bank, ratio);
                table = sinc.table (bank);
                glideAt += 1.0;
            }

            const double g = env * velocityGain * gain;
            const double outL = l * (g * panLeft);
            const double outR = r * (g * panRight);
            target.left[i] += static_cast<float> (outL);
            target.right[i] += static_cast<float> (outR);
            if (throws)
            {
                const double mono = 0.5 * (outL + outR) * static_cast<double> (target.laneSend[i]);
                target.space[i] += static_cast<float> (mono * static_cast<double> (throwSpace));
                target.echo[i] += static_cast<float> (mono * static_cast<double> (throwEcho));
            }

            position += ratio;
            if (loops && position >= loopEnd + kWrapSpan)
                position -= loopLength * std::floor ((position - loopEnd) / loopLength);

            if (envelope.isOver())
            {
                active = false;
                return;
            }
        }
    }
}
