#pragma once

// Test helpers for P8's mixer tests: MixConfigs built in code, and a runner that feeds the mixer's buses block by
// block (any block size) with keys and controllers at absolute sample times.

#include "mix/mixer.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace mixtest
{
    using namespace wubansia::engine;

    Insert gain (double db, When when = {});
    Insert clip (double ceilingDb, double kneeDb = 1.5, When when = {});
    Insert comp (double thresholdDb, double makeupDb = 0.0);
    Insert onsetDuck (std::uint16_t keyLanes, double depthDb, double rampS, double holdS, double releaseS, bool pump = false, Band band = {});
    Insert levelDuck (int keyChannel, double depthDb, double rangeDb, double topDb, double attackS, double releaseS, Band band = {});
    Insert ccFilter (int loHz, int hiHz, int qPm, int cc74Default = 127);
    Insert parallel (double levelDb, std::vector<Insert> chain);

    Channel channel (const char* id, Lane lane, std::vector<Insert> inserts = {}, Group group = Group::none,
        std::array<std::int32_t, kSends> sendsPm = {});

    // A plain mix: no returns, no bus EQ, the default club master (its glue threshold 0 dBFS, no drive).
    MixConfig makeMix (std::vector<Channel> channels);

    // A mix with every return and effect on (FUT01-like settings), for the robustness tests.
    MixConfig busyMix();

    struct Event
    {
        std::int64_t time;
        enum class Kind { key, cc } kind;
        int a, b, c, d; // key: lane, velocity; cc: channel, controller, value, ramp
    };

    // The calling thread's CPU time in ns where the platform keeps it (POSIX), so other processes' load doesn't count;
    // elsewhere the wall clock. For the CPU figures.
    double threadCpuNs();

    // Runs `total` samples through the mixer in blocks of `block`: input (channel, sample index, side) -> value,
    // events at their absolute times, controls (sample index of the block's start) -> Controls. Returns the output,
    // interleaved by numOutputs.
    std::vector<float> run (Mixer& mixer, int total, int block, int numOutputs,
        const std::function<float (int channel, std::int64_t t, int side)>& input, int channels, const std::vector<Event>& events = {},
        const std::function<Controls (std::int64_t blockStart)>& controls = {}, double bpm = 134.0);
}
