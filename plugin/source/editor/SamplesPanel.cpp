#include "SamplesPanel.h"

// Stubs: the tests that drive the drawer build against these and fail until the drawer is written.

namespace wubansia::editor
{
    juce::String exportFileName (const KitCard&, const SoundFile&)
    {
        return {};
    }

    KitChip::KitChip (const juce::String& id)
        : juce::Button (id), kitId (id)
    {
    }

    void KitChip::show (bool isBrowsed, bool isPlayingKit)
    {
        browsed = isBrowsed;
        playing = isPlayingKit;
    }

    void KitChip::paintButton (juce::Graphics&, bool, bool) {}

    CategoryButton::CategoryButton (SoundCategory c)
        : juce::Button ("Category"), which (c)
    {
    }

    void CategoryButton::show (int numSounds, bool isSelected)
    {
        sounds = numSounds;
        selected = isSelected;
    }

    void CategoryButton::paintButton (juce::Graphics&, bool, bool) {}

    SoundRow::SoundRow (SamplesPanel& o, int index)
        : owner (o), row (index)
    {
        juce::ignoreUnused (owner, hover, pressed, dragging);
    }

    void SoundRow::show (const Look& look) { shown = look; }
    juce::Rectangle<int> SoundRow::markArea() const { return {}; }
    void SoundRow::paint (juce::Graphics&) {}
    void SoundRow::mouseEnter (const juce::MouseEvent&) {}
    void SoundRow::mouseExit (const juce::MouseEvent&) {}
    void SoundRow::mouseDown (const juce::MouseEvent&) {}
    void SoundRow::mouseDrag (const juce::MouseEvent&) {}
    void SoundRow::mouseUp (const juce::MouseEvent&) {}
    std::unique_ptr<juce::AccessibilityHandler> SoundRow::createAccessibilityHandler() { return juce::Component::createAccessibilityHandler(); }

    SoundList::SoundList (SamplesPanel& o)
        : owner (o)
    {
        juce::ignoreUnused (owner);
    }

    bool SoundList::keyPressed (const juce::KeyPress&) { return false; }
    void SoundList::focusGained (FocusChangeType) {}
    void SoundList::focusLost (FocusChangeType) {}
    std::unique_ptr<juce::AccessibilityHandler> SoundList::createAccessibilityHandler() { return juce::Component::createAccessibilityHandler(); }

    SoundDragTile::SoundDragTile (SamplesPanel& o)
        : owner (o)
    {
        juce::ignoreUnused (owner, hover, pressed, dragging);
    }

    void SoundDragTile::show (const juce::String& fileName, bool isFailed)
    {
        name = fileName;
        failed = isFailed;
    }

    void SoundDragTile::paint (juce::Graphics&) {}
    void SoundDragTile::mouseEnter (const juce::MouseEvent&) {}
    void SoundDragTile::mouseExit (const juce::MouseEvent&) {}
    void SoundDragTile::mouseDown (const juce::MouseEvent&) {}
    void SoundDragTile::mouseDrag (const juce::MouseEvent&) {}
    void SoundDragTile::mouseUp (const juce::MouseEvent&) {}
    bool SoundDragTile::keyPressed (const juce::KeyPress&) { return false; }
    std::unique_ptr<juce::AccessibilityHandler> SoundDragTile::createAccessibilityHandler() { return juce::Component::createAccessibilityHandler(); }

    SamplesPanel::SamplesPanel (Controller& c)
        : platform (ContentPanel::Platform::linuxOS), controller (c)
    {
        setComponentID ("samples_drawer");
        juce::ignoreUnused (controller, chipPicked, wantedCategory, ticket, auditionDismissed, tileFailed);
    }

    SamplesPanel::~SamplesPanel() = default;

    void SamplesPanel::open (const EditorModel&) {}
    void SamplesPanel::update (const EditorModel&) {}
    void SamplesPanel::resized() {}
    void SamplesPanel::paint (juce::Graphics&) {}
    bool SamplesPanel::keyPressed (const juce::KeyPress&) { return false; }
    juce::String SamplesPanel::statusText() const { return {}; }
    juce::String SamplesPanel::bannerText() const { return {}; }
    juce::String SamplesPanel::folderText() const { return {}; }
    juce::String SamplesPanel::detailText() const { return {}; }
    juce::String SamplesPanel::hintText() const { return {}; }

    const std::vector<SoundInfo>& SamplesPanel::soundsOf (const juce::String& kit) { return catalog[kit]; }
    const KitCard* SamplesPanel::cardOf (const juce::String&) const { return nullptr; }
    ContentStatus SamplesPanel::contentOf (const juce::String&) const { return {}; }
    bool SamplesPanel::kitReady() const { return false; }
    const SoundInfo* SamplesPanel::selectedSound() const { return nullptr; }
    int SamplesPanel::chosenFile (const SoundInfo&) const { return 0; }
    SoundRef SamplesPanel::refOf (const SoundInfo&) const { return {}; }
    void SamplesPanel::browse (const juce::String&) {}
    void SamplesPanel::pickKit (const juce::String&) {}
    void SamplesPanel::pickCategory (SoundCategory) {}
    void SamplesPanel::showCategory (SoundCategory) {}
    void SamplesPanel::select (int) {}
    void SamplesPanel::rowPressed (int, bool) {}
    bool SamplesPanel::rowDragged (int, juce::Component&) { return false; }
    bool SamplesPanel::listKey (const juce::KeyPress&) { return false; }
    void SamplesPanel::play() {}
    void SamplesPanel::stop() {}
    void SamplesPanel::fileChosen() {}
    bool SamplesPanel::dragOut (juce::Component&) { return false; }
    void SamplesPanel::reveal() {}
    juce::File SamplesPanel::exportSelected() { return {}; }
    void SamplesPanel::changeFolder() {}
    void SamplesPanel::folderChosen (const juce::File&) {}
    bool SamplesPanel::auditioning() const { return false; }
    juce::String SamplesPanel::auditionMessage() const { return {}; }
    void SamplesPanel::setMessage (const juce::String&, juce::Colour, int) {}
    void SamplesPanel::refresh() {}
    void SamplesPanel::refreshRows() {}
    void SamplesPanel::refreshDetail() {}
    void SamplesPanel::layOutList() {}
}
