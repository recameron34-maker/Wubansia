#pragma once

// The closed insert set (ADR 0021; docs/plugin/manifest.md 3.2), one processor class per type, and the slot that
// runs one in a chain. Every processor has setup () (from its MixConfig params, at the host's rate; it doesn't
// allocate and also resets), reset (), process (l, r, n, context) on a stereo block in place, and latency ().
// Processors hold fixed arrays only, so a slot can become any of them on the audio thread when a Kit is adopted.
//
// The slot adds what the manifest's `when` needs: an insert that is active only for a variant or an A/B side
// crossfades over 10 ms against its bypass when that changes. The bypass is delayed by the insert's latency, so both
// stay aligned, and the insert keeps running while bypassed, so it fades in warm.

#include "engine.h"
#include "fx/band_split.h"
#include "fx/biquad.h"
#include "fx/delay_line.h"
#include "fx/duck.h"
#include "fx/glue.h"
#include "fx/level_detector.h"
#include "fx/oversampled.h"
#include "fx/stereo.h"
#include "fx/svf.h"
#include "mix/mix_config.h"

#include <array>
#include <cstdint>
#include <variant>

namespace wubansia::engine::mix
{
    inline constexpr int kMixLatency = 64; // the graph's latency budget, samples at every rate (ADR 0021)
    inline constexpr int kMaxSlots = 512; // the inserts of one mix, parallel chains included
    inline constexpr int kKeyEvents = 256; // onset keys queued
    inline constexpr int kCcEvents = 64; // controller events queued per channel
    inline constexpr std::size_t kKeyRing = 8192; // samples of each key channel's level-duck key kept (a power of 2)
    using AlignDelay = fx::BlockDelay<128>; // up to 127 samples: the budget and its parts fit

    struct KeyEvent
    {
        std::int64_t time = 0; // the mixer's clock
        std::uint8_t lane = 0;
        std::uint8_t velocity = 0;
    };

    struct CcEvent
    {
        std::int64_t time = 0;
        std::uint8_t controller = 0;
        std::uint8_t value = 0;
        std::int32_t ramp = 0; // samples
    };

    // Stereo scratch blocks a chain can use while it runs: per nesting level (0: a chain, 1: a parallel insert's).
    struct Scratch
    {
        float* bypassL[2] {};
        float* bypassR[2] {};
        float* parallelL = nullptr;
        float* parallelR = nullptr;
    };

    // What an insert sees of the block it runs in.
    struct BlockContext
    {
        std::int64_t start = 0; // the mixer's clock at the block's first sample
        int n = 0;
        double fs = 48000.0;
        const Controls* controls = nullptr;
        const float* pump = nullptr; // the Pump macro per sample, 0..2
        const KeyEvent* keys = nullptr; // a ring of kKeyEvents
        std::uint64_t keysWritten = 0; // events ever queued (the ring holds the last kKeyEvents)
        const CcEvent* cc = nullptr; // the channel's ring of kCcEvents (null outside channels)
        std::uint64_t ccWritten = 0;
        const float* const* keyRings = nullptr; // per channel: post-insert mono, indexed by clock & (kKeyRing - 1)
        Scratch scratch;
    };

    // ------------------------------------------------------------------ the processors

    class GainInsert
    {
    public:
        void setup (const GainParams& p, double fs) noexcept;
        void reset() noexcept {}
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept;
        double linear() const noexcept { return gain; }

    private:
        double gain = 1.0;
    };

    class PanInsert
    {
    public:
        void setup (const PanParams& p, double fs) noexcept;
        void reset() noexcept {}
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept;

    private:
        fx::PanGains gains;
    };

    class WidthInsert
    {
    public:
        void setup (const WidthParams& p, double fs) noexcept;
        void reset() noexcept { side.reset(); }
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept { side.process (l, r, n); }

    private:
        fx::SideFilter side;
    };

    class EqInsert // peak, low_shelf, high_shelf
    {
    public:
        void setup (InsertType type, const EqParams& p, double fs) noexcept;
        void reset() noexcept { filter.reset(); }
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept;

    private:
        fx::BiquadCascade<1, 2> filter;
    };

    class PassInsert // highpass, lowpass: Butterworth or RBJ, 1-2 stages, in a crossover band
    {
    public:
        void setup (InsertType type, const PassParams& p, double fs) noexcept;
        void reset() noexcept;
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept;

    private:
        fx::BiquadCascade<8, 2> filter;
        fx::BandSplit band;
    };

    class CcFilterInsert
    {
    public:
        void setup (const CcFilterParams& p, double fs) noexcept;
        void reset() noexcept;
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext& ctx) noexcept;

        // The insert reads its channel's controllers this many samples after they arrive (its input's latency).
        void setDelay (int samples) noexcept { delay = samples; }

        // The controller values now (for tests).
        double cutoffValue() const noexcept { return v74; }
        double resonanceValue() const noexcept { return v71; }

    private:
        struct Ramp
        {
            double step = 0.0;
            std::int32_t left = 0;
            double target = 0.0;
        };

        void apply (const CcEvent& e) noexcept;

        CcFilterParams params;
        double fs = 48000.0;
        double v74 = 127.0, v71 = 0.0;
        Ramp r74, r71;
        fx::SvfCoeffs coeffs;
        bool dirty = true;
        std::array<fx::Svf, 2> svf {};
        fx::BandSplit band;
        std::uint64_t nextEvent = 0;
        int delay = 0;
    };

    class MonoBelowInsert
    {
    public:
        void setup (const MonoBelowParams& p, double fs) noexcept;
        void reset() noexcept { side.reset(); }
        int latency() const noexcept { return 0; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept { side.process (l, r, n); }

    private:
        fx::SideFilter side;
    };

    class ClipInsert
    {
    public:
        void setup (const ClipParams& p, double fs) noexcept;
        void reset() noexcept { clip.reset(); }
        int latency() const noexcept { return fx::LaneClip::kLatency; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept { clip.process (l, r, n); }

    private:
        fx::LaneClip clip;
    };

    class DriveInsert
    {
    public:
        void setup (const DriveParams& p, double fs) noexcept;
        void reset() noexcept { drive.reset(); }
        int latency() const noexcept { return fx::LaneDrive::kLatency; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept { drive.process (l, r, n); }

    private:
        fx::LaneDrive drive;
    };

    class DuckInsert
    {
    public:
        void setup (const DuckParams& p, double fs) noexcept;
        void reset() noexcept;
        int latency() const noexcept { return preDelay.delaySamples(); }
        void process (float* l, float* r, int n, const BlockContext& ctx) noexcept;

        // Onset: keys count this many samples after they arrive. Level: the key channel and how far behind this
        // insert's input its signal is (keyDelay >= 0), and the signal's own delay before the duck when the key was
        // later than it (preDelay, the insert's latency). A key channel < 0 bypasses the duck (a cycle).
        void setOnsetDelay (int samples) noexcept { keyDelay = samples; }
        void setLevelKey (int channel, int keyDelaySamples, int preDelaySamples) noexcept;

        // Where aligned time 0 reaches the level detector (its blocks count from there). Resets it.
        void setOrigin (int samples) noexcept
        {
            detector.setOrigin (samples);
            detector.reset();
        }

        DuckMode mode() const noexcept { return params.mode; }
        int keyChannel() const noexcept { return keyChan; }

    private:
        DuckParams params;
        double depthDb = 0.0;
        fx::BandPart band;
        fx::OnsetEnvelope envelope;
        fx::LevelDetector detector;
        AlignDelay preDelay;
        std::uint64_t nextKey = 0;
        int keyDelay = 0;
        int keyChan = -1;
    };

    class CompInsert
    {
    public:
        void setup (const CompParams& p, double fs) noexcept;
        void reset() noexcept { glue.reset(); }

        // Where aligned time 0 reaches the glue (its blocks count from there). Resets it.
        void setOrigin (int samples) noexcept
        {
            glue.setOrigin (samples);
            glue.reset();
        }
        int latency() const noexcept { return fx::Glue::kLatency; }
        void process (float* l, float* r, int n, const BlockContext&) noexcept { glue.process (l, r, n); }

    private:
        fx::Glue glue;
    };

    struct InsertSlot;

    // The parallel insert's own part: its chain lives in the slot pool (first, count); the dry signal waits for it.
    class ParallelInsert
    {
    public:
        void setup (const ParallelParams& p, double fs) noexcept;
        void reset() noexcept { dry.reset(); }
        int latency() const noexcept { return dry.delaySamples(); }

        void setChain (InsertSlot* first, int count, int chainLatency) noexcept;
        void process (float* l, float* r, int n, const BlockContext& ctx) noexcept;

    private:
        double level = 1.0;
        InsertSlot* chain = nullptr;
        int chainCount = 0;
        AlignDelay dry;
    };

    // ------------------------------------------------------------------ the slot

    struct InsertSlot
    {
        InsertType type = InsertType::gain;
        When when;
        bool conditional = false; // `when` restricts it
        int depth = 0; // 0 in a chain, 1 in a parallel insert's
        int span = 1; // slots it takes in the pool: 1, or 1 + its chain's for a parallel insert (its chain follows it)
        int inputLatency = 0; // the latency of the signal at its input, from the voices
        double fade = 1.0; // the processed share now, 0..1
        double fadeStep = 0.0; // per sample, for 10 ms
        AlignDelay bypassDelay; // the bypass, as late as the processor
        std::variant<GainInsert, PanInsert, WidthInsert, EqInsert, PassInsert, CcFilterInsert, MonoBelowInsert, ClipInsert,
            DriveInsert, DuckInsert, CompInsert, ParallelInsert>
            proc;

        // Builds the processor for an insert (no allocation). fs: the host's rate.
        void setup (const Insert& insert, int nesting, double fs, const Controls& controls) noexcept;
        void reset (const Controls& controls) noexcept;
        int latency() const noexcept;
        void process (float* l, float* r, int n, const BlockContext& ctx) noexcept;

        bool activeFor (const Controls& controls) const noexcept;
    };

    // Whether `when` holds for these controls.
    bool whenHolds (const When& when, const Controls& controls) noexcept;

    // Runs a chain in order, in place: `total` slots of the pool from `slots`, a parallel insert's own chain inside it.
    void processChain (InsertSlot* slots, int total, float* l, float* r, int n, const BlockContext& ctx) noexcept;
}
