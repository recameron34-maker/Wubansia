#pragma once

// The engine (ADR 0017): plain C++20, no JUCE. The plugin's processBlock and groovecore render drive
// it the same way: prepare () once per rate and block size, then process () per block with the host's
// transport, the controls and the block's MIDI.
//
// Threads:
// - message thread: prepare (), latencySamples (), tailSeconds ();
// - service thread (the kit service): publish (), collectRetired ();
// - audio thread: process (), reset (); telemetry () is written here and read anywhere.
//
// Kits reach the audio thread as raw pointers. The service keeps a std::shared_ptr<const Kit> for
// every Kit it publishes, until that Kit comes back from publish () (never adopted) or from
// collectRetired () (adopted, then let go), or until the Engine is destroyed. The Engine never frees
// a Kit and the audio thread never touches a shared_ptr. The audio thread adopts the pending Kit at
// block start while stopped, or at the next bar line while playing, and only while the retire FIFO
// has room for the Kit it lets go. Mode, variant and A/B are indices into the adopted Kit, so
// changing them swaps nothing.
//
// Everything process () calls is noexcept and real-time safe: no allocation, no free, no lock, no
// I/O, no logging, no waiting. Capacities are fixed in prepare ().

#include "kit/kit.h"
#include "rt/pending_slot.h"
#include "rt/retire_fifo.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace wubansia::engine
{
    enum class PlayMode : std::uint8_t {
        off, // only MIDI in plays
        loop, // the kit's 4-bar loop, ppq mod 16 quarters from ppq 0, whatever the host's time signature
        song // the extended mix; ppq 0 is song bar 1; silent past the end
    };

    enum class Variant : std::uint8_t {
        vox,
        novox
    };

    enum class PhraseMode : std::uint8_t {
        automatic, // phrase lanes play their recordings while the host tempo is within tolerance
        off // phrase lanes always play the multisample
    };

    enum class MidiInMode : std::uint8_t {
        multi, // incoming notes route by channel (docs/midi-map.md)
        lane // every channel plays Controls::midiInLane
    };

    // The club master limiter's hold. twoSided is master.py's two-sided hold, for groovecore render's verification
    // renders only; the latency follows it.
    enum class MasterHold : std::uint8_t {
        lookBack,
        twoSided
    };

    // The Kit's timeline for a mode and variant: [0] Loop vox ... [3] Song no-vox. -1 for off.
    constexpr int timelineIndex (PlayMode mode, Variant variant) noexcept
    {
        if (mode == PlayMode::off)
            return -1;
        return (mode == PlayMode::song ? 2 : 0) + (variant == Variant::novox ? 1 : 0);
    }

    // The host's transport at the block's first sample. Without a host position (Standalone, Preview
    // while stopped) the plugin's internal transport fills it.
    struct TransportInfo
    {
        bool hasPosition = false; // ppq is valid
        bool playing = false;
        double ppq = 0.0; // quarter notes from the host's zero
        double bpm = 0.0; // 0: unknown
        bool looping = false; // the host loops loopStartPpq..loopEndPpq
        double loopStartPpq = 0.0;
        double loopEndPpq = 0.0;
        bool nonRealtime = false; // an offline render
    };

    // Every control, read once per block on the audio thread. The plugin fills it from its parameters
    // and state-only settings; the defaults are what the club loops play.
    struct Controls
    {
        PlayMode mode = PlayMode::loop;
        Variant variant = Variant::vox;
        std::array<std::uint8_t, kAbSwitches> ab { 1, 1, 0 }; // by AbSwitch: kick, bass, stab; 0 = A, 1 = B
        std::array<float, kLanes> laneGain { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }; // linear, 0 when muted
        float space = 1.0f; // the macros, 0..2
        float echo = 1.0f;
        float pump = 1.0f;
        bool club = true; // the club master
        PhraseMode phrases = PhraseMode::automatic;
        MidiInMode midiIn = MidiInMode::multi;
        std::uint8_t midiInLane = 0; // a Lane value, for MidiInMode::lane
    };

    // One incoming MIDI message: a channel message of up to 3 bytes. SysEx and other long messages
    // never reach the engine.
    struct MidiEvent
    {
        std::int32_t offset = 0; // the sample within the block
        std::uint8_t status = 0;
        std::uint8_t data1 = 0;
        std::uint8_t data2 = 0;
    };

    // What the editor shows. The audio thread writes it; any thread reads it. Readers that "take" a
    // value clear it with exchange (0); the engine sets bits with fetch_or and raises peaks.
    struct Telemetry
    {
        std::atomic<std::uint32_t> laneHits { 0 }; // bit per Lane: a note started since the reader took it
        std::atomic<std::uint32_t> phraseRecorded { 0 }; // bit per Lane: the lane plays its recorded phrase now
        std::array<std::atomic<float>, kLanes> lanePeak {}; // linear peak per lane since the reader took it
        std::atomic<float> gainReduction { 0.0f }; // the club master's, in dB, >= 0
        std::atomic<double> songTicks { -1.0 }; // the position in 960-PPQ ticks in the playing timeline; -1 when not playing
        std::atomic<std::int32_t> section { -1 }; // index into the playing timeline's sections; -1 before the first
        std::atomic<std::int32_t> voices { 0 }; // sounding voices
        std::atomic<std::int32_t> kitIndex { -1 }; // the adopted Kit's KitInfo::index; -1 with no Kit

        static_assert (std::atomic<double>::is_always_lock_free && std::atomic<float>::is_always_lock_free);
    };

    class Engine
    {
    public:
        static constexpr std::size_t kRetireSlots = 16;

        Engine();
        ~Engine();
        Engine (const Engine&) = delete;
        Engine& operator= (const Engine&) = delete;

        // Message thread, with the audio thread stopped. Allocates every buffer and capacity for this
        // rate (11025-192000 Hz), the largest block process () will get, and 1 or 2 outputs. It can
        // be called again at another rate without anything in between; voices and position reset,
        // the adopted Kit stays. hold: the club master limiter's (twoSided only for groovecore render's
        // verification renders); latencySamples () follows it.
        void prepare (double sampleRate, int maxBlock, int numOutputs, MasterHold hold = MasterHold::lookBack);

        // Audio thread (the plugin's reset flag). Silences every voice and tail at once; the adopted
        // Kit stays.
        void reset() noexcept;

        // Service thread. Offers kit (nullptr withdraws an offer) and returns the earlier offer if
        // the audio thread never adopted it, which the service may free at once.
        const Kit* publish (const Kit* kit) noexcept { return pending.publish (kit); }

        // Service thread. Calls f (const Kit*) for each Kit the audio thread has let go, oldest first.
        // The service then drops its reference.
        template <class F>
        void collectRetired (F&& f) noexcept
        {
            while (const Kit* kit = retired.pop())
                f (kit);
        }

        // Audio thread. Renders numSamples (at most prepare's maxBlock) into out[0..numOutputs),
        // overwriting it: 1 output folds to (L + R) / 2. midi holds numMidi events sorted by offset,
        // each offset in [0, numSamples).
        void process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport,
            const Controls& controls, const MidiEvent* midi, int numMidi) noexcept;

        // Constant after prepare (): what the plugin reports to the host.
        int latencySamples() const noexcept;
        double tailSeconds() const noexcept;

        const Telemetry& telemetry() const noexcept { return telemetry_; }

    private:
        struct Impl; // engine.cpp's: the scheduler, router, voices, mixer...
        std::unique_ptr<Impl> impl;
        PendingSlot<Kit> pending;
        RetireFifo<Kit, kRetireSlots> retired;
        Telemetry telemetry_;
    };
}
