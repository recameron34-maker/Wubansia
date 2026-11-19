#pragma once

// The editor's LookAndFeel (ADR 0032): it holds the look the editor wears (a Skin, swapped when the kit changes) and
// draws JUCE's own pieces in it: sliders, combo boxes, popup menus, text buttons and tooltips. Everything is drawn as
// vector paths and the looks' embedded faces, so it stays sharp at every zoom and display scale.

#include "editor/skin/Skin.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor
{
    // Drawing primitives shared by the panels: glyphs drawn as paths, so they never depend on a font's symbols.
    namespace paint
    {
        // A small MIDI-file glyph for the drag chips.
        void midiGlyph (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);

        // A 2 x 2 grid of rounded pads, for the Drum kit button (ADR 0030).
        void padsGlyph (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);


        // Icons drawn as paths, so they never depend on a font's symbols, each filling a square box: a play
        // triangle, a stop square and a folder.
        juce::Path playGlyph (juce::Rectangle<float> box);
        juce::Path stopGlyph (juce::Rectangle<float> box);
        juce::Path folderGlyph (juce::Rectangle<float> box);

        // Six dots in two columns: "drag me by this".
        void grip (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        // The look it draws in (ADR 0032), held by value: set on the message thread when the kit changes, with JUCE's
        // colour IDs for popups, tooltips, scroll bars and combo boxes.
        void setSkin (const Skin& skin);
        const Skin& skin() const noexcept { return current; }

        // Sliders: rotary knobs with an LED ring, and vertical faders. Their values are drawn by the
        // panels, so they have no text boxes.
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
        int getSliderThumbRadius (juce::Slider&) override;
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
            float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
            float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

        // Combo boxes (the lane picker and zoom) and their menus.
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
            int buttonW, int buttonH, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
        juce::Font getPopupMenuFont() override;

        // Text buttons are the lit action buttons ("Choose content folder..."). One whose "quiet" property is
        // set is a dark rubber button instead (the Samples drawer's Show in Finder, Change..., Content...), and
        // an "icon" property of "play" or "folder" draws that icon before the text.
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool isMouseOverButton,
            bool isButtonDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
            juce::Rectangle<int> parentArea) override;

    private:
        Skin current;
    };

}
