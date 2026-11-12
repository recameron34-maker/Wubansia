#pragma once

// The editor's controls: JUCE sliders, buttons and combo boxes with their own drawing, so a
// PollingAttachment can bind any of them to a parameter and the Controller-driven ones work the same
// way. Every one draws in the editor's accent (LookAndFeel.h).

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

    // A rotary knob with an LED ring: drag up or down, scroll, double-click for the default.
    class Knob : public juce::Slider, public ContextMenuHook
    {
    public:
        Knob();

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // A vertical fader: drag the cap, double-click for the default.
    class Fader : public juce::Slider, public ContextMenuHook
    {
    public:
        Fader();

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // A square rubber pad with a letter that lights when on (the mutes).
    class PadButton : public juce::Button, public ContextMenuHook
    {
    public:
        PadButton (const juce::String& name, const juce::String& letter, juce::Colour light);

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        juce::String letter;
        juce::Colour light;
    };

    // A pill with an LED and a label: the LED lights in the accent while it's on.
    class LedButton : public juce::Button, public ContextMenuHook
    {
    public:
        explicit LedButton (const juce::String& label);

        // The label's font and where it's drawn (upper case, centred) while the button is up, so tests can check it fits.
        juce::Font labelFont() const;
        juce::Rectangle<float> labelArea() const;

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
    };

    // An A/B switch showing the kit's names for both sides: off is A, on is B. Clicking a side picks
    // it. With no alternative in the kit it shows that and stays disabled.
    class AbSwitch : public juce::Button, public ContextMenuHook
    {
    public:
        explicit AbSwitch (const juce::String& name);

        void setLabels (const juce::String& a, const juce::String& b);
        const juce::String& labelA() const noexcept { return a; }
        const juce::String& labelB() const noexcept { return b; }

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        using juce::Button::clicked;
        void clicked() override;

    private:
        juce::String a, b;
        int pressedSide = -1; // the side the mouse went down on: 0 A, 1 B
    };

    // A choice drawn as lit segments side by side; clicking a segment picks it, and space or return
    // steps to the next one. It's a ComboBox, so items, selection and onChange work as usual.
    class SegmentedChoice : public juce::ComboBox, public ContextMenuHook
    {
    public:
        explicit SegmentedChoice (const juce::String& name, const juce::StringArray& items = {});

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override {}
        void mouseDrag (const juce::MouseEvent&) override {}
        void showPopup() override;
    };

    // A small button with a drawn icon; toggled ones light up (play).
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { play, stop, folder, close, minus, plus, reset };

        IconButton (const juce::String& name, Icon icon);

        void setIcon (Icon newIcon);
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        Icon icon;
    };
}
