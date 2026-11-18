#pragma once

// MIDI drag-out. A chip asks the Controller to write the MIDI file it stands for (the loop, the song,
// a lane or a phrase file), then hands that file to the OS as an external drag, so it drops straight
// into a DAW's arrangement. A click without a drag shows the file instead, for hosts that don't take
// drops from plug-in windows.

#include "Controls.h"
#include "EditorModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace wubansia::editor
{
    class DragChip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        DragChip (Controller&, const DragRequest& request, const juce::String& label);

        void setRequest (const DragRequest& request, const juce::String& label);
        const DragRequest& request() const noexcept { return dragRequest; }
        const juce::String& label() const noexcept { return text; }

        // A compact chip (in a lane strip, under the lane's name) just says MIDI.
        void setCompact (bool shouldBeCompact);
        bool hasFailed() const noexcept { return failed; }

        // The label's font and the area it's fitted into (squeezed to 0.8 at most, then cut), so tests can check it fits.
        juce::Font labelFont() const;
        juce::Rectangle<float> labelArea() const;

        // The narrowest width at this height that shows the label whole, or a phrase title without its bar count, squeezed
        // to 0.8 at most (shownLabel ()'s test): what the drag strip gives a phrase chip at least, where it can.
        int fitWidth (int height) const;

        // What paint () writes: the label, or a phrase title without its bar count where the whole title doesn't fit even
        // squeezed to 0.8 in the platform's font; "Couldn't write" ("!" when compact) after a failed drag; MIDI when compact.
        juce::String shownLabel() const;

        // Writes the file and starts the external drag; false if either fails. mouseDrag calls it.
        bool beginDrag();

        // How the chip hands a file to the OS. Tests replace them; by default startDrag is startExternalDrag
        // (ExternalDrag.h: the OS drag of the one file) and revealFile dragout::revealToUser (the file browser).
        std::function<bool (const juce::File&, juce::Component&)> startDrag;
        std::function<void (const juce::File&)> revealFile;

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void enablementChanged() override { repaint(); }

    private:
        Controller& controller;
        DragRequest dragRequest;
        juce::String text;
        bool hover = false, pressed = false, dragging = false, failed = false, compact = false;
    };

    // The strip of song-wide chips: the loop, the song and each phrase file, and two switches, the person's settings kept
    // by the Controller: For my synths (ADR 0027), which every drag follows, and 1 drum track (ADR 0025), which the Loop
    // and Song drags follow.
    class DragStrip : public juce::Component
    {
    public:
        explicit DragStrip (Controller&);

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        DragChip loop, song;
        LedButton drums { "1 drum track" };
        LedButton synths { "For my synths" };
        int numPhraseChips() const noexcept { return phraseChips.size(); }
        DragChip* phraseChip (int index) const noexcept { return phraseChips[index]; }

        // Where the engraved "Drag MIDI" is drawn, so tests can check it fits.
        juce::Rectangle<float> labelArea() const;

    private:
        Controller& controller;
        juce::OwnedArray<DragChip> phraseChips;
        juce::StringArray files;
        juce::String kitId;
    };
}
