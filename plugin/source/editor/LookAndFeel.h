#pragma once

// The editor's look: a dark anodised panel with engraved labels, recessed displays and lit
// controls, all drawn as vector paths in JUCE's default typeface (no embedded fonts or images), so it
// stays sharp at every zoom and display scale. One accent colour per kit lights the LEDs, the LCD,
// the knob rings and the fader caps.

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor
{
    namespace colours
    {
        inline const juce::Colour background { 0xff0d0e11 };
        inline const juce::Colour panelTop { 0xff1d2025 };
        inline const juce::Colour panelBottom { 0xff131519 };
        inline const juce::Colour plateTop { 0xff252930 };
        inline const juce::Colour plateBottom { 0xff1b1e23 };
        inline const juce::Colour recess { 0xff08090b };
        inline const juce::Colour text { 0xffeceef2 };
        inline const juce::Colour textDim { 0xffa6acb7 };
        inline const juce::Colour textFaint { 0xff686e79 };
        inline const juce::Colour engraved { 0xff7d838e };
        inline const juce::Colour mute { 0xffeef1f6 }; // the mute pads' light: white, apart from every kit's accent
        inline const juce::Colour busy { 0xffffc23d }; // loading
        inline const juce::Colour warning { 0xffff6a4d }; // problems
        inline const juce::Colour hot { 0xfffff5ec }; // a meter's top segment: white-hot whatever the accent
    }

    // The kit's accent: FUT01 UK garage violet, FUT02 sci-fi club cyan, FUT03 deep tech house amber,
    // FUT04 dark techno red. Other kits get a stable colour from their ID.
    juce::Colour accentForKit (const juce::String& kitId);

    // Fonts: JUCE's default sans-serif (the system's UI face) and monospaced typefaces.
    juce::Font uiFont (float height, bool bold = false, float tracking = 0.0f);
    juce::Font monoFont (float height, bool bold = false);

    // Drawing primitives shared by the panels.
    namespace draw
    {
        // The brushed panel behind everything, with corner screws.
        void faceplate (juce::Graphics&, juce::Rectangle<float> area);

        // A raised section plate with a bevel, and an engraved title in its top-left corner.
        void plate (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title = {});

        // A recessed well (displays, meters, slots).
        void recess (juce::Graphics&, juce::Rectangle<float> area, float corner);

        void screw (juce::Graphics&, juce::Point<float> centre, float radius, float angle);

        // Small upper-case label text, letter-spaced, as if engraved.
        void engraved (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
            juce::Justification justification, float height = 10.0f, juce::Colour colour = colours::engraved);

        // A round LED: level 0 is off (a dark lens), 1 fully lit with its glow.
        void led (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, float level);

        // A segmented level meter, vertical (bottom up) or horizontal (left to right). fill and hold
        // are 0..1 of the scale; hold < 0 draws no hold segment. Segments from warmFrom whiten, and
        // from hotFrom glow white-hot, so the top reads the same in every kit's colour.
        void meter (juce::Graphics&, juce::Rectangle<float> area, int segments, float fill, float hold,
            juce::Colour lit, bool vertical, int warmFrom = -1, int hotFrom = -1);

        // Seven-segment digits for the LCD: 0-9, '-', ' ' and a few letters; a '.' lights the previous
        // cell's point. Unlit segments show faintly, like a real backlit display.
        void sevenSegment (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
            juce::Colour lit, float ghostAlpha = 0.07f);

        // A small MIDI-file glyph for the drag chips.
        void midiGlyph (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);

        // The dark rubber body every button shares; a lit one (lit > 0) glows in its light.
        void rubber (juce::Graphics&, juce::Rectangle<float> area, float corner, bool down, bool over, juce::Colour light, float lit);

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

        void setAccent (juce::Colour colour);
        juce::Colour accent() const noexcept { return accentColour; }

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
        juce::Colour accentColour;
    };

    // The accent of the LookAndFeel a component draws with (the editor's), or the default.
    juce::Colour accentOf (const juce::Component&);
}
