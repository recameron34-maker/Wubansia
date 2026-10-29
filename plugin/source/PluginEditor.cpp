#include "PluginEditor.h"
#include "ParameterIds.h"

namespace wubansia
{
    PluginEditor::PluginEditor (PluginProcessor& p)
        : AudioProcessorEditor (&p),
          gainAttachment (p.apvts, ParameterIds::masterGain, gainSlider)
    {
        gainSlider.setComponentID (ParameterIds::masterGain);
        addAndMakeVisible (gainSlider);
        setSize (400, 300);
    }

    void PluginEditor::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        g.setColour (juce::Colours::white);
        g.setFont (16.0f);
        g.drawText (juce::String (JucePlugin_Name) + " " + WUBANSIA_VERSION,
            getLocalBounds().removeFromTop (40),
            juce::Justification::centred);
    }

    void PluginEditor::resized()
    {
        auto area = getLocalBounds().reduced (10);
        area.removeFromTop (30);
        gainSlider.setBounds (area.withSizeKeepingCentre (160, 160));
    }
}
