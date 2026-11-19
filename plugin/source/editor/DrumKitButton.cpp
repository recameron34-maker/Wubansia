#include "DrumKitButton.h"

namespace wubansia::editor
{
    DrumKitButton::DrumKitButton (Controller& c) : controller (c) {}

    void DrumKitButton::update (const EditorModel&) {}
    juce::String DrumKitButton::shownLabel() const { return label; }
    juce::String DrumKitButton::statusText() const { return sentence; }
    juce::Font DrumKitButton::labelFont() const { return juce::Font (juce::FontOptions (10.5f)); }
    juce::Rectangle<float> DrumKitButton::labelArea() const { return getLocalBounds().toFloat(); }
    int DrumKitButton::fitWidth (int) const { return 0; }
    void DrumKitButton::paint (juce::Graphics& g) { g.drawText (label, getLocalBounds(), juce::Justification::centred); }
    void DrumKitButton::mouseEnter (const juce::MouseEvent&) {}
    void DrumKitButton::mouseExit (const juce::MouseEvent&) {}
    void DrumKitButton::mouseDown (const juce::MouseEvent&) {}
    void DrumKitButton::mouseDrag (const juce::MouseEvent&) {}
    void DrumKitButton::mouseUp (const juce::MouseEvent&) {}
    bool DrumKitButton::keyPressed (const juce::KeyPress&) { return false; }
    std::unique_ptr<juce::AccessibilityHandler> DrumKitButton::createAccessibilityHandler() { return juce::Component::createAccessibilityHandler(); }
    void DrumKitButton::press() {}
    void DrumKitButton::click() {}
    void DrumKitButton::show (const DrumKitStatus&) {}
}
