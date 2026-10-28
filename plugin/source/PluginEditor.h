#pragma once

#include "PluginProcessor.h"

namespace wubansia
{
    // A native placeholder editor with one gain knob. The WebView UI replaces it in M4.
    class PluginEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit PluginEditor (PluginProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
    };
}
