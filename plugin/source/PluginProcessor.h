#pragma once

#include "SineVoices.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace wubansia
{
    class PluginProcessor : public juce::AudioProcessor
    {
    public:
        PluginProcessor();
        ~PluginProcessor() override = default;

        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

        using AudioProcessor::processBlock;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        // One named program: Steinberg's VST3 validator fails plugins whose program has no name.
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Default"; }
        void changeProgramName (int, const juce::String&) override {}

        // State is the parameter tree plus a version number that future migrations key off.
        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        static constexpr const char* stateType = "WubansiaState"; // PERMANENT
        static constexpr int stateVersion = 1;

        juce::AudioProcessorValueTreeState apvts;

    private:
        std::atomic<float>* masterGainDb = nullptr; // owned by apvts; read lock-free on the audio thread
        juce::LinearSmoothedValue<float> masterGain;
        SineVoices voices;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
    };
}
