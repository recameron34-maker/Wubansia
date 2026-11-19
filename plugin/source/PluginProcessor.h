#pragma once

// The plugin (P9): the engine (ADR 0017) behind a JUCE AudioProcessor, with the parameters (P10's
// layout, ADR 0018), state version 2, the kit service, the internal transport and the editor's
// Controller.
//
// Threads:
// - message thread: the constructor (no file I/O, no thread), prepareToPlay, the editor, the Controller;
// - any thread but the audio thread: getStateInformation and setStateInformation (some hosts call them
//   off the message thread);
// - the kit service's thread: kit loads (services/KitService.h);
// - the sound thread: the Samples drawer's audition clips (services/SoundService.h, ADR 0024), started by
//   the first audition;
// - the drum kit thread: the Drag MIDI strip's Drum kit folders (services/DrumKitService.h, ADR 0030), started by the
//   first press of Drum kit;
// - audio thread: processBlock, which in order (the design's): stops denormals and clears the buffer;
//   acts on a reset (the engine's voices and the audition; a prepareToPlay since doesn't cancel it); in an
//   offline render only, waits for a kit load in flight (ADR 0005 D5: 1 ms steps, 30 s per load at most;
//   never in real time, never when content is missing); reads the controls; resolves the transport (the
//   host's, or the internal one); copies the MIDI into a fixed array of 3-byte messages (no SysEx); runs
//   the engine in pieces no longer than the prepared block; adds the Samples drawer's audition, dry, over
//   the whole block (7b; never in an offline render, and with none playing it touches nothing); applies
//   master_gain through its 20 ms smoother; and leaves the transport for the editor. A bypassed block lets
//   every audition go unplayed.
//
// The first prepareToPlay or setStateInformation queues the first kit load; nothing before. The sound
// thread waits for the first audition.

#include "ControllerImpl.h"
#include "telemetry/OutputScope.h"
#include "Parameters.h"
#include "State.h"
#include "content/ContentSource.h"
#include "services/DrumKitService.h"
#include "services/KitService.h"
#include "services/PhraseService.h"
#include "services/SoundService.h"
#include "transport/InternalTransport.h"

#include "audition/audition_player.h"
#include "engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace wubansia
{
    class PluginProcessor : public juce::AudioProcessor
    {
    public:
        static constexpr auto kOfflineWaitLimit = std::chrono::seconds (30); // ADR 0005 D5, per load
        static constexpr int kMaxMidiEvents = 2048; // a block's MIDI messages; more are dropped

        PluginProcessor();
        ~PluginProcessor() override;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

        using AudioProcessor::processBlock;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        // A bypassed instrument outputs silence and drops its voices, so no note can hang.
        using AudioProcessor::processBlockBypassed;
        void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        // Hosts call this to stop all sound, possibly from a non-audio thread, so it only raises a flag
        // that the next block acts on.
        void reset() override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }

        // The engine's: the Space and Echo returns' tails (6 s). The latency (the mix's fixed delay,
        // 424 samples at 48 kHz) is reported in prepareToPlay.
        double getTailLengthSeconds() const override;

        // One named program: Steinberg's VST3 validator fails plugins whose program has no name.
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Default"; }
        void changeProgramName (int, const juce::String&) override {}

        // State version 2 (State.h, ADR 0018).
        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        // The editor's API.
        Controller& controller() noexcept { return controllerImpl; }

        // Replaces where the kits come from (the synthetic kit in tests), with the kit and sound services
        // that read them, and the kit a new instance plays (FUT01) when kit isn't empty. Only before the
        // first prepareToPlay or setStateInformation, and before any audition.
        void setContentForTesting (std::unique_ptr<ContentSourceFactory> content, const juce::String& kit = {});

        // Shortens the offline wait's 30 s, for tests.
        void setOfflineWaitLimitForTesting (std::chrono::milliseconds limit) noexcept { waitLimitMs.store (limit.count()); }

        // For tests: SoundService::setGiveUpProbeForTesting, after setContentForTesting and before any audition.
        void setAuditionGiveUpProbeForTesting (std::function<void()> probe) { sounds->setGiveUpProbeForTesting (std::move (probe)); }

        // For tests: SoundService::setWakeProbeForTesting, likewise.
        void setAuditionWakeProbeForTesting (std::function<void()> probe) { sounds->setWakeProbeForTesting (std::move (probe)); }

        // For the Controller and the tests.
        const KitService& kitService() const noexcept { return *service; }
        const engine::Telemetry& telemetry() const noexcept { return player.telemetry(); }
        const engine::AuditionPlayer& auditionPlayer() const noexcept { return auditions; }
        const SoundService& soundService() const noexcept { return *sounds; }
        DrumKitService& drumKitService() noexcept { return *drumKits; }
        const DrumKitService& drumKitService() const noexcept { return *drumKits; }
        const PhraseService& phraseService() const noexcept { return *phraseThread; }
        const OutputScope& outputScope() const noexcept { return scope; } // machine room's tap (ADR 0032)
        bool phraseOffered() const noexcept { return player.phraseOffered(); } // for tests: a phrase clip waits for a block
        void setPhraseWakeProbeForTesting (std::function<void()> probe) { phraseThread->setWakeProbeForTesting (std::move (probe)); }
        StateOnly settings() const;

        juce::AudioProcessorValueTreeState apvts;

    private:
        friend class ControllerImpl;

        void queueFirstLoad();
        void stopPhrasePreview() noexcept; // turns Preview off if the Phrases drawer's audition turned it on
        int contentVersionOf (const juce::String& kit) const;
        void mirrorSettings (const StateOnly& s) noexcept;
        // Changes the state-only settings under the lock and, when that changed them, tells the host the
        // project changed. Whether it did.
        bool changeSettings (const std::function<void (StateOnly&)>& change);
        void waitForKitOffline() noexcept;

        Parameters::RawParameters raw;
        std::unique_ptr<ContentSourceFactory> content;
        engine::Engine player;
        engine::AuditionPlayer auditions; // ADR 0024: the Samples drawer's clips
        std::unique_ptr<KitService> service; // after the engine and content: it uses both, so it goes first
        std::unique_ptr<SoundService> sounds; // after content and auditions, which it uses: it goes first
        std::unique_ptr<DrumKitService> drumKits; // after content, which it reads: it goes first of all
        std::unique_ptr<PhraseService> phraseThread; // the Phrases drawer's (ADR 0031): after the engine and the kit service
        std::atomic<bool> previewForPhrase { false }; // the Phrases drawer's audition turned Preview on, and nothing since off
        OutputScope scope; // the looks' output scope (ADR 0032): pushed in processBlock while on, pulled by the editor
        ControllerImpl controllerImpl;
        InternalTransport transport;

        // The state-only settings and what a state kept: stateMutex, never on the audio thread.
        mutable std::mutex stateMutex;
        StateOnly stateOnly;
        int stateVersionRead = State::version;
        juce::NamedValueSet otherAttributes;
        bool loadQueued = false;

        // getStateInformation and setStateInformation one at a time (the APVTS's tree isn't safe to
        // replace while it is copied). Recursive: a host may ask for the state from inside a parameter
        // callback that setStateInformation set off.
        std::recursive_mutex stateIoMutex;

        // The audio thread's copies of the state-only settings it uses.
        std::atomic<std::uint8_t> phrasesMirror { 0 };
        std::atomic<std::uint8_t> midiInMirror { 0 };
        std::atomic<std::uint8_t> midiInLaneMirror { 0 };

        // The audio side.
        std::atomic<bool> prepared { false };
        std::atomic<bool> resetPending { false };
        bool bypassed = false;
        int preparedBlock = 512;
        double preparedRate = 48000.0;
        juce::LinearSmoothedValue<float> masterGain;
        engine::Controls controls;
        std::array<engine::MidiEvent, kMaxMidiEvents> midiEvents {};

        // The offline wait's account: the load it waited for, and how long.
        std::uint64_t waitGeneration = 0;
        std::chrono::steady_clock::duration waited {};
        std::atomic<std::int64_t> waitLimitMs { std::chrono::milliseconds (kOfflineWaitLimit).count() };

        // What the last block played, for the Controller.
        std::atomic<bool> lastPlaying { false };
        std::atomic<bool> lastInternal { false };
        std::atomic<double> lastBpm { 0.0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
    };
}
