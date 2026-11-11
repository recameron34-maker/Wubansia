#pragma once

// The plugin's own transport (P9). It drives playback where the host gives no position: the
// Standalone app (JUCE's player reports no PPQ, no tempo and never plays), and Preview, which plays
// the kit while a host is stopped. It has no position of its own: it hands the engine
// TransportInfo::hasPosition false, so the engine runs its own clock from bar 1 at the tempo set here,
// or at the kit's when that is 0 (engine.h). Preview turns itself off when the host starts playing,
// so stopping the host never restarts it.
//
// The message thread sets it; the audio thread resolves each block's transport from it and the host's
// position. Everything is an atomic: no lock, no allocation.

#include "engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace wubansia
{
    class InternalTransport
    {
    public:
        static constexpr double kMinBpm = 40.0; // the engine sizes its delay lines for 40 BPM
        static constexpr double kMaxBpm = 300.0;

        // 0 stays 0 (follow the kit); anything else is held to kMinBpm..kMaxBpm; NaN gives 0.
        static double clampBpm (double bpm) noexcept;

        // Message thread.
        void setPlaying (bool on) noexcept { playing_.store (on, std::memory_order_release); }
        void setBpm (double bpm) noexcept { bpm_.store (clampBpm (bpm), std::memory_order_release); }
        void setPreview (bool on) noexcept { preview_.store (on, std::memory_order_release); }

        bool playing() const noexcept { return playing_.load (std::memory_order_acquire); }
        double bpm() const noexcept { return bpm_.load (std::memory_order_acquire); }
        bool preview() const noexcept { return preview_.load (std::memory_order_acquire); }

        // One block's transport.
        struct Block
        {
            engine::TransportInfo info;
            bool internal = false; // the plugin's own clock drives it
        };

        // Audio thread: the block's transport from the host's position (none: no play head, or one that
        // gives nothing). With a host PPQ: the host's transport while it plays; Preview's clock while it
        // is stopped and Preview is on (at the host's tempo, else the kit's); else stopped where the host
        // is. Without one (Standalone): the internal clock, playing when set to or on Preview, at the set
        // tempo (else the host's, else the kit's).
        Block resolve (const juce::Optional<juce::AudioPlayHead::PositionInfo>& host, bool nonRealtime) noexcept;

    private:
        std::atomic<bool> playing_ { false };
        std::atomic<bool> preview_ { false };
        std::atomic<double> bpm_ { 0.0 };

        static_assert (std::atomic<double>::is_always_lock_free);
    };
}
