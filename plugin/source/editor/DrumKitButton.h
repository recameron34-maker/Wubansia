#pragma once

// The Drag MIDI strip's Drum kit button (ADR 0030), drawn as a drag chip with a 2 x 2 pad glyph. A press asks the
// Controller to save the kit's drum kit (exportDrumKit: the drum kit thread starts at once); a click shows its SFZ once
// it is saved, once; a 4 px drag hands the SFZ to the OS when it is saved, or, while it's saving, starts nothing (macOS
// starts an OS drag only inside its mouse event) and says to drag again. Return and the accessibility press act as a
// click; Space is left to the host. Dimmed while the kit's samples aren't ready. "Couldn't save" stays until the next
// press or a kit change; the problem's detail goes in the tooltip and the announcement, never the label.

#include "Controller.h"
#include "EditorModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace wubansia::editor
{
    class DrumKitButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit DrumKitButton (Controller&);

        void update (const EditorModel&); // the 30 Hz tick: enabled, label, tooltip, a pending reveal, announcements
        juce::String shownLabel() const; // "Drum kit", "Saving…", "Couldn't save"
        juce::String statusText() const; // the sentence the tooltip starts with
        juce::Font labelFont() const; // DragChip's
        juce::Rectangle<float> labelArea() const;

        // The narrowest width at this height that shows each of its three labels whole, unsqueezed: the widest in
        // labelFont () plus DragChip's label margins (36 px at a 28 px height), plus 1.
        int fitWidth (int height) const;

        // How it hands the SFZ on and speaks. Tests replace them; by default startDrag is startExternalDrag, revealFile
        // dragout::revealToUser and announce announceToScreenReader.
        std::function<bool (const juce::File&, juce::Component&)> startDrag;
        std::function<void (const juce::File&)> revealFile;
        std::function<void (const juce::String&)> announce;

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override; // exportDrumKit ()
        void mouseDrag (const juce::MouseEvent&) override; // 4 px: the OS drag if ready, else "drag again"
        void mouseUp (const juce::MouseEvent&) override; // a click: reveal once ready
        bool keyPressed (const juce::KeyPress&) override; // Return: a press and a click
        void enablementChanged() override { repaint(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override; // press: a press and a click

    private:
        void press();
        void click();
        void show (const DrumKitStatus& status); // label, tooltip and announcement for status

        Controller& controller;
        juce::String kit; // the kit the button speaks for
        bool samplesReady = false;
        juce::String samplesWhy; // the sentence while they aren't
        std::uint32_t ticket = 0; // the last press's
        bool asked = false; // pressed since the kit changed
        bool revealPending = false; // a click came while saving: show the SFZ once it's ready
        bool dragAgain = false; // a drag came while saving
        bool hover = false, pressed = false, dragging = false;
        juce::String label { "Drum kit" }, sentence, detail, spoken;
    };
}
