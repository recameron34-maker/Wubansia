#pragma once

#include "ParameterIds.h"
#include "PluginProcessor.h"

namespace wubansia::test
{
    // What a host wrapper does before playback.
    inline void prepare (PluginProcessor& p, double sampleRate, int blockSize)
    {
        p.setRateAndBufferSizeDetails (sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
    }

    inline juce::RangedAudioParameter& masterGainParameter (PluginProcessor& p)
    {
        auto* parameter = p.apvts.getParameter (ParameterIds::masterGain);
        jassert (parameter != nullptr);
        return *parameter;
    }

    inline float masterGainDb (PluginProcessor& p)
    {
        return p.apvts.getRawParameterValue (ParameterIds::masterGain)->load();
    }

    inline void setMasterGainDb (PluginProcessor& p, float db)
    {
        auto& parameter = masterGainParameter (p);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (db));
    }

    // Runs one block with the given MIDI and returns the rendered audio.
    inline juce::AudioBuffer<float> process (PluginProcessor& p, int numSamples, juce::MidiBuffer midi = {})
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), numSamples);
        p.processBlock (buffer, midi);
        return buffer;
    }

    // Opens the editor the way a host does, runs the test code, then closes it.
    inline void withEditor (PluginProcessor& p, const std::function<void (juce::AudioProcessorEditor&)>& testCode)
    {
        auto* editor = p.createEditorAndMakeActive();
        jassert (editor != nullptr);
        testCode (*editor);
        p.editorBeingDeleted (editor);
        delete editor;
    }
}
