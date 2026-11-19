#include "DrumKitButton.h"
#include "ExternalDrag.h"
#include "LookAndFeel.h"
#include "editor/skin/Paint.h"
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
            return bounds.reduced (10.0f, 2.0f); // a strip chip's (DragChip.cpp)
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

    juce::Font DrumKitButton::labelFont() const { return skinOf (*this).type.stripChip.font(); }

    juce::Rectangle<float> DrumKitButton::labelArea() const { return labelAreaIn (getLocalBounds().toFloat()); }

    int DrumKitButton::fitWidth (int height) const
    {
        const juce::Rectangle<float> wide (1000.0f, (float) height);
        const auto around = wide.getWidth() - labelAreaIn (wide).getWidth(); // the border, padding and glyph: 36 px at 28
        float widest = 0.0f;
        const auto& style = skinOf (*this).type.stripChip; // its widest state in the look's chip style (ADR 0032)
        for (const auto& words : { juce::String ("Drum kit"), saving(), couldntSave })
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (style.font(), style.apply (words)));
        return (int) std::ceil (widest + around) + 1;
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

    std::vector<DrawnText> DrumKitButton::drawnText() const
    {
        return { fitText (skinOf (*this).type.stripChip, label, labelArea(), juce::Justification::centred) };
    }

    void DrumKitButton::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto enabled = isEnabled();
        const auto failed = label == couldntSave;
        const auto active = pressed || dragging;
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        const auto fill = active || (hover && enabled) ? over (c.accentTint, c.face).brighter (0.05f) : over (c.accentTint, c.face);
        paint::box (g, area, skin.shape.stripChip, enabled ? fill : juce::Colours::transparentBlack, failed ? c.warning : enabled ? c.accent : c.controlEdge);
        for (const auto& t : drawnText())
            drawText (g, t, ! enabled ? c.faint : failed ? c.warning : c.accentTintText);
    }
}
