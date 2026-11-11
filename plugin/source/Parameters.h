#pragma once

// The plugin's parameters (ADR 0018): one definition, shared by the processor, which builds its
// AudioProcessorValueTreeState from createLayout (), and the editor, which binds its controls by the
// IDs in ParameterIds.h. Names and labels may change; IDs, ranges and defaults are what saved projects
// and automation hold, so treat them as permanent too.

#include "ParameterIds.h"
#include "engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace wubansia::Parameters
{
    inline constexpr float minDb = -60.0f; // the bottom of every gain range: silence
    inline constexpr float maxDb = 6.0f;
    inline constexpr float maxMacroPercent = 200.0f;
    inline constexpr float defaultMacroPercent = 100.0f;

    // Every parameter in ParameterIds::all's order: master_gain exactly as M0 shipped it (version
    // hint 1), then the 29 v0.1 parameters (version hint 2).
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Display text for the lane levels ("-inf", "-6.0", "0.0", "+3.5"; the label says dB) and the
    // macros ("100"; the label says %), and their inverses. Exposed for the tests.
    juce::String levelText (float db, int maximumLength = 0);
    float levelFromText (const juce::String& text);
    juce::String percentText (float percent, int maximumLength = 0);
    float percentFromText (const juce::String& text);

    // The audio thread's view of the parameters: the APVTS's raw values (dB, %, choice index, 0 or 1),
    // resolved once, off the audio thread. read () and masterGainDb () are real-time safe.
    class RawParameters
    {
    public:
        explicit RawParameters (juce::AudioProcessorValueTreeState& state);

        // Fills the fields the parameters drive: mode, variant, ab, laneGain (linear; 0 when muted or at
        // the bottom of the range), space, echo, pump (0..2) and club. The state-only fields (phrases,
        // midiIn, midiInLane) are left as they are.
        void read (engine::Controls& controls) const noexcept;

        float masterGainDb() const noexcept { return masterGain->load (std::memory_order_relaxed); }

    private:
        std::atomic<float>* masterGain = nullptr;
        std::atomic<float>* playMode = nullptr;
        std::atomic<float>* voxSynth = nullptr;
        std::array<std::atomic<float>*, ParameterIds::ab.size()> ab {};
        std::array<std::atomic<float>*, ParameterIds::numLanes> laneLevel {};
        std::array<std::atomic<float>*, ParameterIds::numLanes> laneMute {};
        std::atomic<float>* space = nullptr;
        std::atomic<float>* echo = nullptr;
        std::atomic<float>* pump = nullptr;
        std::atomic<float>* clubMaster = nullptr;
    };
}
