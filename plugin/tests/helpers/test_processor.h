#pragma once

// A small processor for the parameter and editor tests: the plugin's parameter layout
// (Parameters.cpp) in an APVTS, silence for audio, and an editor made by whatever factory a test
// installs. PluginProcessor is P9's; this tests the layout and the editor without it.

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <utility>

namespace wubansia::test
{
    class TestProcessor : public juce::AudioProcessor
    {
    public:
        using EditorFactory = std::function<juce::AudioProcessorEditor* (TestProcessor&)>;

        explicit TestProcessor (EditorFactory factory = {})
            : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "WubansiaState", Parameters::createLayout()),
              editorFactory (std::move (factory))
        {
        }

        juce::RangedAudioParameter& parameter (const juce::String& id)
        {
            auto* p = apvts.getParameter (id);
            jassert (p != nullptr);
            return *p;
        }

        // The parameter's value in its own units (dB, %, choice index, 0 or 1).
        float value (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); }

        // As a host's automation does: from any thread, notifying the host.
        void set (const juce::String& id, float plainValue)
        {
            auto& p = parameter (id);
            p.setValueNotifyingHost (p.convertTo0to1 (plainValue));
        }

        const juce::String getName() const override { return "Wubansia"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        using AudioProcessor::processBlock;
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }

        juce::AudioProcessorEditor* createEditor() override { return editorFactory != nullptr ? editorFactory (*this) : nullptr; }
        bool hasEditor() const override { return editorFactory != nullptr; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Default"; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;

    private:
        EditorFactory editorFactory;
    };
}
