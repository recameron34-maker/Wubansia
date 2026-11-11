#pragma once

// The mix graph (ADR 0021): the adopted Kit's MixConfig played in real time.
//
//   channels: inserts -> align -> lane level and mute -> group (drums, music) or the mix bus; sends post-fader
//   throws (the voices' direct sends) -> as late as the channels -> the sends
//   groups: inserts -> align -> mix bus
//   returns: Space, Echo, Resonator -> inserts -> return level x macro -> align -> mix bus
//   mix bus: the variant's bus EQ (both run; a variant change crossfades them) -> align -> the club master
//
// Latency. Every insert reports its own (clip and drive 27, comp 23, a parallel insert its chain's, a level duck the
// wait for a key that arrives later than its signal; the rest 0). The graph delays the shorter paths at each meeting
// point (the channels' outputs, the groups' and returns' outputs, the two bus EQs), keys and controllers wait as long
// as the signal at their insert, and the master's input sits at a fixed kMixLatency samples, so the total,
// kMixLatency + the club master's, is the same for every kit at a rate. A mix whose longest path needs more is
// marked overBudget () (its late paths stay late; P3's exporter refuses such a mix). Block-based stages (the glue, the
// level detector) and absolute-time ones (the echo's wow, the shimmer's taps) count from where aligned time 0 reaches
// them, as engine_ref's streams do.
//
// Keys and controllers carry the mixer's clock (samples since the last reset), so the output is the same for any
// host block size. Everything after prepare () is allocation-free.

#include "engine.h"
#include "fx/delay_line.h"
#include "fx/echo.h"
#include "fx/resonator.h"
#include "fx/space.h"
#include "mix/channel.h"
#include "mix/club_master.h"
#include "mix/inserts.h"
#include "mix/mix_config.h"
#include "mix/mixer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace wubansia::engine::mix
{
    class MixGraph
    {
    public:
        // Allocates every line and buffer. hold: the club master limiter's (twoSided only for verification renders).
        void prepare (double sampleRate, int maxBlock, fx::Limiter::Hold hold = fx::Limiter::Hold::lookBack);

        // The Kit's mix (nullptr: the channel buses summed at unity, as late as a mix). Builds the plan in place and
        // resets every tail.
        void setConfig (const MixConfig* config, const Controls& controls) noexcept;
        void reset (const Controls& controls) noexcept;

        void keyHit (std::uint8_t lane, int velocity, int offset) noexcept;
        void cc (int channel, int controller, int value, int offset, int rampSamples) noexcept;

        // Mixes the buses into outL / outR (outR null: a mono output, (L + R) / 2 into outL).
        void process (MixBus& bus, SendBus& sends, float* outL, float* outR, int n, const Controls& controls, double bpm) noexcept;

        int latency() const noexcept { return kMixLatency + master.latency(); }
        const MixMeters& meters() const noexcept { return meterValues; }

        // The current mix's longest path to the master's input, and whether it fits the budget and the slot pool.
        int pathLatency() const noexcept { return a3; }
        bool overBudget() const noexcept { return tooLate; }
        bool overCapacity() const noexcept { return tooMany; }

        // A mix's longest path to the master's input, without building it (for the manifest checks).
        static int requiredLatency (const MixConfig& config) noexcept;

    private:
        struct Strip // a group, a return or a bus EQ
        {
            int firstSlot = 0, slotCount = 0;
            int latency = 0;
            AlignDelay align;
        };

        int addChain (const std::vector<Insert>& inserts, int nesting, const Controls& controls) noexcept;
        int planChain (int firstSlot, int count, int inputLatency, const std::array<int, kMaxMixChannels>& channelLatency) noexcept;
        void routeChannel (int c, float* l, float* r, int n, const Controls& controls) noexcept;
        void mixInto (float* dl, float* dr, const float* sl, const float* sr, int n) noexcept;
        BlockContext context (int n, const Controls& controls) noexcept;

        double fs = 48000.0;
        int maxBlock = 0;
        const MixConfig* config = nullptr;

        std::vector<InsertSlot> slots;
        int slotsUsed = 0;
        std::array<ChannelStrip, kMaxMixChannels> channels {};
        int numChannels = 0;
        std::array<int, kMaxMixChannels> order {};
        std::array<Strip, 2> groups {};
        std::array<Strip, kSends> returns {};
        std::array<double, kSends> returnGain {};
        std::array<bool, kSends> returnOn {};
        std::array<Strip, 2> busEq {};
        AlignDelay noneAlign, preMaster;
        std::array<AlignDelay, kSends> throwAlign {};
        fx::Space space;
        fx::Echo echo;
        fx::Resonator resonator;
        ClubMaster master;
        Ramp spaceMacro, echoMacro, pumpMacro, busMix;
        int a1 = 0, a2 = 0, a3 = 0;
        bool tooLate = false, tooMany = false;

        std::array<KeyEvent, kKeyEvents> keys {};
        std::uint64_t keysWritten = 0;
        std::vector<CcEvent> ccEvents; // kMaxMixChannels x kCcEvents
        std::array<std::uint64_t, kMaxMixChannels> ccWritten {};
        std::vector<float> keyRingData; // kMaxMixChannels x kKeyRing
        std::array<const float*, kMaxMixChannels> keyRings {};

        // maxBlock-long buffers
        std::vector<float> buffers;
        float* bypass[2][2] {};
        float* parallel[2] {};
        float* groupIn[2][2] {};
        float* noneIn[2] {};
        float* sendIn[kSends] {};
        float* throwBuf = nullptr;
        float* ret[2] {};
        float* mixL = nullptr;
        float* mixR = nullptr;
        float* altL = nullptr;
        float* altR = nullptr;
        float* pump = nullptr;
        float* macro[2] {}; // Space, Echo

        fx::DelayLine<float> plainL, plainR; // the no-config path's delay
        std::int64_t clock = 0;
        MixMeters meterValues;
    };
}
