#include "DrumKitButton.h"
#include "ExternalDrag.h"
#include "LookAndFeel.h"
#include "SamplesPanel.h"

#include "dragout/DragFiles.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        const juce::String saving() { return utf8 ("Saving\xe2\x80\xa6"); }
        const juce::String couldntSave ("Couldn't save");

        // Where the label goes for these bounds: right of the glyph, as a drag chip lays its label out (DragChip.cpp).
        juce::Rectangle<float> labelAreaIn (juce::Rectangle<float> bounds)
        {
            auto content = bounds.reduced (1.5f).reduced (7.0f, 2.0f);
            content.removeFromLeft (juce::jmin (14.0f, content.getHeight()));
            content.removeFromLeft (5.0f);
            return content;
        }

        juce::String idleSentence (const juce::String& kit)
        {
            return "Drum kit. Saves " + kit + "'s drums into your samples folder: a WAV per pad for your drum rack, every layer and round robin, an SFZ "
                   "file that plays the drum MIDI as Wubansia does, and a README with the pad map. Click to save it and show it; drag it onto an SFZ "
                   "player (sfizz, sforzando, Bitwig's Sampler). Wubansia never changes it once saved.";
        }

        // What helps, after a failure's problem.
        juce::String advice (const DrumKitStatus& s)
        {
            if (s.reinstall)
                return {}; // the problem says to reinstall the kit
            if (s.sourceProblem)
                return " Check that " + s.kit + "'s samples are still installed.";
            if (s.problem.startsWith ("the samples folder") || s.problem.startsWith ("every name for it"))
                return utf8 (" Choose another folder with Change\xe2\x80\xa6 in the Samples drawer.");
            return {};
        }

        // The disk's or the reader's words as a sentence: "can't write /x" is "Can't write /x.".
        juce::String asSentence (const juce::String& detail)
        {
            if (detail.isEmpty())
                return {};
            auto sentence = detail.substring (0, 1).toUpperCase() + detail.substring (1);
            return sentence.endsWithChar ('.') ? sentence : sentence + ".";
        }
    }

    DrumKitButton::DrumKitButton (Controller& c)
        : controller (c)
    {
        // The functions themselves, not lambdas around them: a test checks each default is the one.
        startDrag = startExternalDrag;
        revealFile = dragout::revealToUser;
        announce = announceToScreenReader;
        setComponentID ("drag_drumkit");
        setTitle ("Drum kit");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setWantsKeyboardFocus (true); // Return, by Tab; Space goes on to the host
        setMouseClickGrabsKeyboardFocus (false);
    }

    void DrumKitButton::update (const EditorModel& model)
    {
        if (model.kit.id != kit)
        {
            kit = model.kit.id;
            ticket = 0;
            asked = revealPending = dragAgain = false;
        }
        samplesReady = model.content.state == ContentState::ready && model.content.folder != juce::File();
        samplesWhy = model.content.state == ContentState::loading ? kit + utf8 ("'s samples are loading\xe2\x80\xa6")
                                                                   : kit + "'s samples aren't installed: there's no drum kit to save yet.";
        if (isEnabled() != samplesReady)
            setEnabled (samplesReady);
        show (model.drumKit);
    }

    void DrumKitButton::show (const DrumKitStatus& s)
    {
        auto newLabel = juce::String ("Drum kit");
        juce::String newSentence, newDetail;
        bool speak = false;
        const bool ours = asked && s.kit == kit && s.ticket == ticket;
        if (! samplesReady)
            newSentence = samplesWhy;
        else if (ours && s.stage == DrumKitStage::saving)
        {
            newLabel = saving();
            newSentence = "Saving " + kit + utf8 ("'s drum kit\xe2\x80\xa6") + (dragAgain ? " drag again when it's ready." : "");
            speak = true;
        }
        else if (ours && s.stage == DrumKitStage::ready)
        {
            newSentence = s.wrote ? "Saved " + kit + "'s drum kit: " + s.folder.getFileName() + ", " + juce::String (s.files) + " files."
                                  : kit + "'s drum kit is saved: " + s.folder.getFileName() + ".";
            speak = true;
            dragAgain = false;
            if (revealPending)
            {
                revealPending = false;
                if (revealFile != nullptr)
                    revealFile (s.sfz);
            }
        }
        else if (ours && s.stage == DrumKitStage::failed)
        {
            newLabel = couldntSave;
            newSentence = "Couldn't save " + kit + "'s drum kit: " + s.problem + "." + advice (s);
            newDetail = asSentence (s.detail);
            speak = true;
            revealPending = dragAgain = false;
        }
        else
            newSentence = idleSentence (kit);

        const auto tip = newDetail.isEmpty() ? newSentence : newSentence + " " + newDetail;
        if (newLabel != label)
        {
            label = newLabel;
            repaint();
        }
        sentence = newSentence;
        detail = newDetail;
        if (getTooltip() != tip)
        {
            setTooltip (tip);
            setHelpText (tip);
        }
        if (speak && tip != spoken)
        {
            spoken = tip;
            if (announce != nullptr)
                announce (tip);
        }
        else if (! speak)
            spoken.clear();
    }

    juce::String DrumKitButton::shownLabel() const { return label; }

    juce::String DrumKitButton::statusText() const { return sentence; }

    juce::Font DrumKitButton::labelFont() const { return uiFont (10.5f, true, 0.06f); }

    juce::Rectangle<float> DrumKitButton::labelArea() const { return labelAreaIn (getLocalBounds().toFloat()); }

    int DrumKitButton::fitWidth (int height) const
    {
        const juce::Rectangle<float> wide (1000.0f, (float) height);
        const auto around = wide.getWidth() - labelAreaIn (wide).getWidth(); // the border, padding and glyph: 36 px at 28
        float widest = 0.0f;
        for (const auto& words : { juce::String ("Drum kit"), saving(), couldntSave })
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (labelFont(), words));
        return (int) std::floor (widest + around) + 1;
    }

    void DrumKitButton::press()
    {
        if (! isEnabled())
            return;
        ticket = controller.exportDrumKit();
        asked = true;
        revealPending = dragAgain = false;
        show (controller.drumKitStatus()); // a kit saved already is ready at once
    }

    void DrumKitButton::click()
    {
        const auto s = controller.drumKitStatus();
        if (! asked || s.ticket != ticket || s.kit != kit)
            return;
        if (s.stage == DrumKitStage::ready)
        {
            revealPending = false;
            if (revealFile != nullptr)
                revealFile (s.sfz);
        }
        else if (s.stage == DrumKitStage::saving)
            revealPending = true; // shown once it's ready, once
    }

    void DrumKitButton::mouseEnter (const juce::MouseEvent&)
    {
        hover = true;
        repaint();
    }

    void DrumKitButton::mouseExit (const juce::MouseEvent&)
    {
        hover = false;
        repaint();
    }

    void DrumKitButton::mouseDown (const juce::MouseEvent&)
    {
        if (! isEnabled())
            return;
        pressed = true;
        dragging = false;
        press();
        repaint();
    }

    void DrumKitButton::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressed || dragging || e.getDistanceFromDragStart() < 4)
            return;
        dragging = true;
        const auto s = controller.drumKitStatus();
        const bool ours = asked && s.ticket == ticket && s.kit == kit;
        if (ours && s.stage == DrumKitStage::ready)
        {
            if (startDrag != nullptr)
                startDrag (s.sfz, *this); // the OS runs the drag from here (on macOS, inside this mouse event)
        }
        else if (ours && s.stage == DrumKitStage::saving)
        {
            dragAgain = true; // no OS drag can start later, outside this event
            show (s);
        }
        repaint();
    }

    void DrumKitButton::mouseUp (const juce::MouseEvent& e)
    {
        const auto wasClick = pressed && ! dragging && e.mouseWasClicked();
        pressed = dragging = false;
        if (wasClick)
            click();
        repaint();
    }

    bool DrumKitButton::keyPressed (const juce::KeyPress& key)
    {
        if (key != juce::KeyPress::returnKey || ! isEnabled())
            return false; // Space and everything else go on to the host
        press();
        click();
        return true;
    }

    std::unique_ptr<juce::AccessibilityHandler> DrumKitButton::createAccessibilityHandler()
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] {
                press();
                click();
            }));
    }

    void DrumKitButton::paint (juce::Graphics& g)
    {
        const auto accent = accentOf (*this);
        const auto enabled = isEnabled();
        const auto failed = label == couldntSave;
        auto area = getLocalBounds().toFloat().reduced (1.5f);
        const auto corner = juce::jmin (6.0f, area.getHeight() * 0.5f);
        const auto active = pressed || dragging;

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.2f), corner);
        g.setGradientFill (juce::ColourGradient::vertical (active ? accent.withAlpha (0.42f).overlaidWith (juce::Colour (0xff1d2025))
                                                                  : juce::Colour (hover && enabled ? 0xff2c3138 : 0xff23272d),
            area.getY(), juce::Colour (0xff15171b), area.getBottom()));
        g.fillRoundedRectangle (area, corner);

        const auto border = failed ? colours::warning : (enabled && (hover || active) ? accent : juce::Colour (0xff383d45));
        g.setColour (border.withAlpha (enabled ? 0.9f : 0.4f));
        juce::Path outline;
        outline.addRoundedRectangle (area.reduced (0.5f), corner);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 2.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.fillPath (dashed);

        auto content = area.reduced (7.0f, 2.0f);
        const auto glyph = content.removeFromLeft (juce::jmin (14.0f, content.getHeight())).withSizeKeepingCentre (11.0f, 11.0f);
        draw::padsGlyph (g, glyph, enabled ? (failed ? colours::warning : accent) : colours::textFaint);

        g.setFont (labelFont());
        g.setColour (! enabled ? colours::textFaint : (failed ? colours::warning : colours::text.withAlpha (0.9f)));
        g.drawFittedText (label, labelArea().toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }
}
