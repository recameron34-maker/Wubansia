#pragma once

// One multisample voice: a file read at a ratio (sinc, Hermite or a copy), its envelopes, its velocity
// gain, pan and instrument gain, its glide when it is a mono instrument's held voice, and its direct
// sends. The sampler (sampler.cpp) decides what it plays; the pool (voice_pool.h) owns it. Everything
// here runs on the audio thread and is noexcept; a Voice is plain data, copied into a fading slot when
// it is stolen.

#include "kit/kit.h"
#include "sampler/envelope.h"
#include "sampler/hermite.h"
#include "sampler/sampler.h"

#include <cstdint>

namespace wubansia::engine
{
    // What a voice starts with, all resolved (sampler.cpp's startInstrument).
    struct VoiceStart
    {
        const Kit* kit = nullptr;
        Source source = Source::sequencer;
        std::int16_t instrument = -1; // what it plays
        std::int16_t trigger = -1; // whose note-off releases it: itself, or the instrument it is layered under
        std::uint8_t key = 0;
        std::uint8_t root = 60; // its zone's root: legato notes glide relative to it
        std::uint8_t layerVelocity = 127; // its layer's velocity: legato notes' pow15 gain is relative to it
        std::uint8_t lane = 0;
        std::uint8_t mixChannel = 0;
        SampleView sample;
        Interp interp = Interp::sinc;
        SampleMode mode = SampleMode::oneShot;
        double baseRatio = 1.0; // 48000 / host rate
        bool unitBase = true; // the host runs at 48 kHz: baseRatio is exactly 1
        int semitones = 0; // key - the zone's root
        double startPosition = 0.0; // frames into the file
        std::int64_t delay = 0; // host samples before its first sample (end alignment)
        double attackSamples = 0.0;
        double releaseSamples = 0.0;
        double velocityGain = 1.0;
        double gain = 1.0; // the instrument's gain_mdb
        double panLeft = 1.0;
        double panRight = 1.0;
        float throwSpace = 0.0f;
        float throwEcho = 0.0f;
        std::uint64_t order = 0; // start order, for stealing the oldest
    };

    // Where a voice's piece goes: its mix channel's buffers, the send inputs, and its lane's send gain
    // per sample (all indexed by the sample in the piece).
    struct VoiceTarget
    {
        float* left = nullptr;
        float* right = nullptr;
        float* space = nullptr;
        float* echo = nullptr;
        const float* laneSend = nullptr;
    };

    class Voice
    {
    public:
        void start (const VoiceStart& s) noexcept;

        // Key up at an absolute sample time. One-shots ring on; gated and looped voices release.
        void noteOff (std::int64_t when) noexcept;

        // Key down again on the sample it went up (restrike-continue): the release is taken back.
        void restrike() noexcept;

        // Legato: from where the pitch and the velocity gain are now, to semitones (from the zone's root)
        // and velocityGain, along curve over samples (an exponential's time constant). curve none: at once.
        void glide (int semitones, double velocityGain, GlideCurve curve, double samples) noexcept;

        // Fades out over samples (cos^2 or linear), from where its fade level is; a sooner fade stays.
        void fade (double samples, FadeCurve curve) noexcept;

        // Silent at once.
        void stop() noexcept { active = false; }

        // Adds samples [from, to) of the piece into target.
        void render (const VoiceTarget& target, int from, int to) noexcept;

        bool isActive() const noexcept { return active; }
        bool isKeyDown() const noexcept { return keyDown; }
        bool isFading() const noexcept { return envelope.isFading(); }
        // Its gain now (envelope x velocity gain x instrument gain; a rising attack counts as its end):
        // the quietest is stolen.
        double level() const noexcept { return envelope.level() * velocityGain * gain; }

        // Released for stealing: its key is up, or it is fading out (choked, stopped).
        bool isReleased() const noexcept { return ! keyDown || envelope.isFading(); }

        // Its pitch at the last sample it played, in semitones from its zone's root (a glide's trace).
        double currentSemitones() const noexcept { return semitones; }

        const Kit* kit = nullptr;
        Source source = Source::sequencer;
        std::int16_t instrument = -1;
        std::int16_t trigger = -1;
        std::uint8_t key = 0;
        std::uint8_t root = 60;
        std::uint8_t layerVelocity = 127;
        std::uint8_t lane = 0;
        std::uint8_t mixChannel = 0;
        std::uint64_t order = 0;
        std::int64_t keyUpAt = -1; // the absolute sample of its last note-off; -1 while held

    private:
        enum class Reader : std::uint8_t {
            copy,
            hermite,
            sinc
        };

        template <Reader reader, bool stereo>
        void run (const VoiceTarget& target, int from, int to) noexcept;

        // The common case at full speed: no envelope moving, no glide, no throws.
        template <Reader reader, bool stereo>
        void runSteady (const VoiceTarget& target, int from, int to) noexcept;

        template <Reader reader, bool stereo>
        void dispatch (const VoiceTarget& target, int from, int to) noexcept;

        void setRatio() noexcept;

        SampleView sample;
        FrameMap map;
        Envelope envelope;
        Interp interp = Interp::sinc;
        bool active = false;
        bool keyDown = false;
        bool oneShot = true;
        bool unitBase = true;
        bool everFractional = false; // a ratio other than 1 has moved it: copies are off
        bool gliding = false;
        GlideCurve glideCurve = GlideCurve::none;
        double releaseSamples = 0.0;
        double baseRatio = 1.0;
        double semitones = 0.0; // from the zone's root, now
        int targetSemitones = 0;
        double ratio = 1.0;
        int bank = 0;
        double position = 0.0;
        std::int64_t delay = 0;
        double velocityGain = 1.0;
        double gain = 1.0;
        double panLeft = 1.0;
        double panRight = 1.0;
        float throwSpace = 0.0f;
        float throwEcho = 0.0f;
        // the glide
        double glideFromSemitones = 0.0;
        double glideFromGain = 1.0;
        double glideToGain = 1.0;
        double glideSamples = 0.0;
        double glideAt = 0.0; // samples since the legato note-on
    };
}
