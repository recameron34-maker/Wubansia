#include "PhrasesPanel.h"

namespace wubansia::editor
{
    PhraseFilter::PhraseFilter (const juce::String& name)
        : juce::Button (name)
    {
    }

    void PhraseFilter::show (int, bool) {}

    juce::String PhraseFilter::label() const
    {
        return {};
    }

    void PhraseFilter::updateTitle() {}

    void PhraseFilter::paintButton (juce::Graphics&, bool, bool) {}

    PhraseTile::PhraseTile (PhrasesPanel& o, int index)
        : owner (o), tile (index)
    {
    }

    void PhraseTile::show (const PhraseInfo&, bool, bool) {}

    juce::String PhraseTile::detailText() const
    {
        return {};
    }

    void PhraseTile::paint (juce::Graphics&) {}
    void PhraseTile::mouseEnter (const juce::MouseEvent&) {}
    void PhraseTile::mouseExit (const juce::MouseEvent&) {}
    void PhraseTile::mouseDown (const juce::MouseEvent&) {}
    void PhraseTile::mouseDrag (const juce::MouseEvent&) {}
    void PhraseTile::mouseUp (const juce::MouseEvent&) {}

    std::unique_ptr<juce::AccessibilityHandler> PhraseTile::createAccessibilityHandler()
    {
        return juce::Component::createAccessibilityHandler();
    }

    PhraseGrid::PhraseGrid (PhrasesPanel& o)
        : owner (o)
    {
    }

    bool PhraseGrid::keyPressed (const juce::KeyPress&)
    {
        return false;
    }

    void PhraseGrid::focusGained (FocusChangeType) {}
    void PhraseGrid::focusLost (FocusChangeType) {}

    std::unique_ptr<juce::AccessibilityHandler> PhraseGrid::createAccessibilityHandler()
    {
        return juce::Component::createAccessibilityHandler();
    }

    PhrasesPanel::PhrasesPanel (Controller& c)
        : controller (c)
    {
        setComponentID ("phrases_drawer");
        setTitle ("Phrases");
    }

    void PhrasesPanel::open (const EditorModel&) {}
    void PhrasesPanel::update (const EditorModel&) {}
    void PhrasesPanel::focusGrid() {}
    void PhrasesPanel::resized() {}
    void PhrasesPanel::paint (juce::Graphics&) {}

    bool PhrasesPanel::keyPressed (const juce::KeyPress&)
    {
        return false;
    }

    int PhrasesPanel::numTiles() const noexcept
    {
        return 0;
    }

    juce::String PhrasesPanel::kitLine() const
    {
        return {};
    }

    juce::String PhrasesPanel::statusText() const
    {
        return {};
    }

    juce::String PhrasesPanel::synthsText() const
    {
        return {};
    }
}
