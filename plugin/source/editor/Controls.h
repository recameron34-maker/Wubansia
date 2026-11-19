#pragma once

// The editor's controls: JUCE sliders, buttons and combo boxes with their own drawing, so a
// PollingAttachment can bind any of them to a parameter and the Controller-driven ones work the same
// way. Every one draws in the look its LookAndFeel holds (ADR 0032), by its role, and lists the text it
// draws (DrawnTextSource) so the fit tests see what the person sees.

#include "editor/DrawnText.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace wubansia::editor
{
    // Every parameter control answers a right-click (or ctrl-click) with onContextMenu, the host's
    // menu for the parameter, and never moves or toggles on one.
    struct ContextMenuHook
    {
        std::function<void()> onContextMenu;

        // True when the event was a context-menu click, which the control then ignores.
        bool takeContextClick (const juce::MouseEvent& e) const
        {
            if (! e.mods.isPopupMenu())
                return false;
            if (onContextMenu != nullptr)
                onContextMenu();
            return true;
        }
    };

    // The keys a fader or knob takes once Tab gives it the focus (ADR 0032): Up and Down (Right and Left too) move by
    // step, Shift by fine, Home goes to the default; each move is a gesture of its own. A click never takes the focus,
    // so Space and Home stay the host's.
    bool sliderKey (juce::Slider&, const juce::KeyPress&, double step, double fine);

    // A macro knob: drag up or down, scroll, double-click for the default; Up and Down move it by 5 % (Shift 1 %).
    // Deep glass draws it as a horizontal bar (applySkin).
    class Knob : public juce::Slider, public ContextMenuHook
    {
    public:
        Knob();

        void applySkin (const Skin&);
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // A lane's fader: drag the cap, double-click for the default; Up and Down move it by 1 dB (Shift 0.1 dB). Machine
    // room lays it flat (applySkin), and then Right is up.
    class Fader : public juce::Slider, public ContextMenuHook
    {
    public:
        Fader();

        void applySkin (const Skin&);
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // A lane's mute: its look's word ("M", "MUTE", "Mute"), filled while on.
    class PadButton : public juce::Button, public ContextMenuHook, public DrawnTextSource
    {
    public:
        PadButton (const juce::String& name, const juce::String& letter);

        std::vector<DrawnText> drawnText() const override;
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // A toggle drawn by its role: a header button (Phrases, Samples), the vox row (its name and its state word), the
    // club master's round toggle, a drag strip switch (1 drum track, For my synths) or the transport's round Preview.
    class LedButton : public juce::Button, public ContextMenuHook, public DrawnTextSource
    {
    public:
        enum class Role { headerButton, modeRow, club, stripSwitch, transport };

        explicit LedButton (const juce::String& label);

        void setRole (Role);
        Role role() const noexcept { return kind; }
        // The club master's ring: the gain reduction, 0..1 of its scale.
        void setRing (float amount);

        // The label's font and where it's drawn, so tests can check it fits.
        juce::Font labelFont() const;
        juce::Rectangle<float> labelArea() const;
        // The width this role needs at this height to show its label whole.
        int fitWidth (int height) const;

        std::vector<DrawnText> drawnText() const override;
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        Role kind = Role::headerButton;
        float ring = 0.0f;
    };

    // An A/B switch showing the kit's names for both sides: off is A, on is B. Clicking a side picks it; Return flips
    // it, Left picks A and Right picks B (Space is the host's). The panel says what the sides read (setDisplay).
    class AbSwitch : public juce::Button, public ContextMenuHook, public DrawnTextSource
    {
    public:
        explicit AbSwitch (const juce::String& name);

        void setLabels (const juce::String& a, const juce::String& b);
        const juce::String& labelA() const noexcept { return a; }
        const juce::String& labelB() const noexcept { return b; }
        // The words each side shows, before the look's capitals, and whether it's a row of a two- or three-switch kit.
        void setDisplay (const juce::String& sideA, const juce::String& sideB, bool row);

        std::vector<DrawnText> drawnText() const override;
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        using juce::Button::clicked;
        void clicked() override;

    private:
        void pick (int side);
        juce::Rectangle<float> sideArea (int side) const;

        juce::String a, b, shownA, shownB;
        bool asRow = false;
        int pressedSide = -1; // the side the mouse went down on: 0 A, 1 B
    };

    // A choice drawn as segments side by side; clicking a segment picks it, Return steps to the next
    // one, and the arrows move one either way, stopping at the ends. Each tells onChange at once, so a
    // caller that shows its own value every tick doesn't take the key back. It's a ComboBox, so items,
    // selection and onChange work as usual. Its role picks the look: the play mode's big segments, a mode row's
    // small ones, or the drag strip's (Part).
    class SegmentedChoice : public juce::ComboBox, public ContextMenuHook, public DrawnTextSource
    {
    public:
        enum class Role { playMode, row, strip };

        explicit SegmentedChoice (const juce::String& name, const juce::StringArray& items = {});

        void setRole (Role);
        std::vector<DrawnText> drawnText() const override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override {}
        void mouseDrag (const juce::MouseEvent&) override {}
        void showPopup() override;
        bool keyPressed (const juce::KeyPress&) override;

        // Segments as wide as their labels need (the role's font, kLabelPad each side) instead of equal: the drag
        // strip's Part. Room beyond that is shared equally.
        void setSegmentsFitLabels (bool shouldFit);
        // The narrowest width at this height that shows every label whole: the labels' widths, 2 x kLabelPad each, and 1.
        int fitWidth (int height) const;
        // Segment i's area as paint () lights it and mouseDown () picks it, in local coordinates; empty out of range.
        juce::Rectangle<float> segmentArea (int index) const;
        juce::Font labelFont() const; // the selected segment's font, the wider of the two
        static constexpr float kLabelPad = 7.0f;

    private:
        const TextStyle& styleFor (bool on) const;

        Role kind = Role::row;
        bool fitLabels = false;
    };

    // A small button with a drawn icon; toggled ones light up (play). The transport role draws the look's round play
    // button.
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { play, stop, folder, close, minus, plus, reset };

        IconButton (const juce::String& name, Icon icon);

        void setIcon (Icon newIcon);
        void setRound (bool round) { roundButton = round; }
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        Icon icon;
        bool roundButton = false;
    };

    // The transport's round play button, in a look: lit while on, play or stop glyph.
    void paintRoundButton (juce::Graphics&, const Skin&, juce::Rectangle<float> area, bool on, bool stopGlyph, bool over, bool enabled);
}
