#include "DragChip.h"
#include "LookAndFeel.h"

namespace wubansia::editor
{
    namespace
    {
        // A phrase title without its bar count: "Vox Synth Riff · 2 bars" is "Vox Synth Riff" (phraseTitle () puts the
        // count last). Any other text comes back as it is.
        juce::String withoutBarCount (const juce::String& title)
        {
            const auto dot = middleDot();
            const auto at = title.lastIndexOf (dot);
            if (at <= 0)
                return title;
            const auto tail = title.substring (at + dot.length());
            const auto count = tail.upToFirstOccurrenceOf (" ", false, false);
            const auto unit = tail.fromFirstOccurrenceOf (" ", false, false);
            const auto isCount = count.isNotEmpty() && count.containsOnly ("0123456789") && (unit == "bar" || unit == "bars");
            return isCount ? title.substring (0, at) : title;
        }
    }

    DragChip::DragChip (Controller& c, const DragRequest& request, const juce::String& label)
        : controller (c)
    {
        startDrag = [] (const juce::File& file, juce::Component& source)
        {
            return juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, &source);
        };
        revealFile = [] (const juce::File& file) { file.revealToUser(); };
        setRequest (request, label);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void DragChip::setRequest (const DragRequest& request, const juce::String& label)
    {
        if (request == dragRequest && label == text)
            return;
        dragRequest = request;
        text = label;
        failed = false;
        setTitle ("Drag MIDI: " + label);
        repaint();
    }

    void DragChip::setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        repaint();
    }

    bool DragChip::beginDrag()
    {
        const auto file = controller.writeDragFile (dragRequest);
        failed = file == juce::File();
        repaint();
        if (failed)
            return false;
        return startDrag != nullptr && startDrag (file, *this);
    }

    void DragChip::mouseEnter (const juce::MouseEvent&)
    {
        hover = true;
        repaint();
    }

    void DragChip::mouseExit (const juce::MouseEvent&)
    {
        hover = false;
        repaint();
    }

    void DragChip::mouseDown (const juce::MouseEvent&)
    {
        pressed = isEnabled();
        dragging = false;
        repaint();
    }

    void DragChip::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressed || dragging || e.getDistanceFromDragStart() < 4)
            return;
        dragging = true;
        repaint();
        beginDrag(); // the OS runs the drag from here (on macOS, inside this mouse event)
    }

    void DragChip::mouseUp (const juce::MouseEvent& e)
    {
        const auto click = pressed && ! dragging && e.mouseWasClicked();
        pressed = dragging = false;
        repaint();

        if (click)
        {
            const auto file = controller.writeDragFile (dragRequest);
            failed = file == juce::File();
            if (! failed && revealFile != nullptr)
                revealFile (file);
            repaint();
        }
    }

    void DragChip::paint (juce::Graphics& g)
    {
        const auto accent = accentOf (*this);
        const auto enabled = isEnabled();
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
        // A dashed border says "drag me".
        juce::Path outline;
        outline.addRoundedRectangle (area.reduced (0.5f), corner);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 2.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.fillPath (dashed);

        auto content = area.reduced (compact ? 5.0f : 7.0f, 2.0f);
        const auto glyph = content.removeFromLeft (juce::jmin (compact ? 12.0f : 14.0f, content.getHeight())).withSizeKeepingCentre (11.0f, 11.0f);
        draw::midiGlyph (g, glyph, enabled ? (failed ? colours::warning : accent) : colours::textFaint);

        g.setFont (labelFont());
        g.setColour (! enabled ? colours::textFaint : (failed ? colours::warning : colours::text.withAlpha (0.9f)));
        g.drawFittedText (shownLabel(), labelArea().toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }

    juce::String DragChip::shownLabel() const
    {
        if (failed)
            return compact ? "!" : "Couldn't write";
        if (compact)
            return "MIDI";
        // A title that doesn't fit even squeezed to 0.8 (drawFittedText's own test) would be cut short: a phrase title
        // drops its bar count instead, which the tooltip's file name still gives. The platform's font decides: Verdana
        // and DejaVu Sans run 8 and 12 % wider than Liberation Sans.
        if (juce::GlyphArrangement::getStringWidth (labelFont(), text) * 0.8f < (float) labelArea().toNearestInt().getWidth())
            return text;
        return withoutBarCount (text);
    }

    juce::Font DragChip::labelFont() const
    {
        return uiFont (compact ? 9.5f : 10.5f, true, 0.06f);
    }

    juce::Rectangle<float> DragChip::labelArea() const
    {
        // Right of the MIDI glyph, as paint () lays the chip out.
        auto content = getLocalBounds().toFloat().reduced (1.5f).reduced (compact ? 5.0f : 7.0f, 2.0f);
        content.removeFromLeft (juce::jmin (compact ? 12.0f : 14.0f, content.getHeight()));
        content.removeFromLeft (compact ? 4.0f : 5.0f);
        return content;
    }

    DragStrip::DragStrip (Controller& c)
        : loop (c, { DragWhat::loop, engine::Lane::kick, {} }, "Loop"),
          song (c, { DragWhat::song, engine::Lane::kick, {} }, "Song"),
          controller (c)
    {
        setComponentID ("drag");
        loop.setComponentID ("drag_loop");
        loop.setTooltip ("Drag the 4-bar loop into your DAW as MIDI: the vox switch picks the vox or no-vox file, muted lanes "
                         "are left out, and 1 drum track puts its drums on one track. Click to show the file.");
        song.setComponentID ("drag_song");
        song.setTooltip ("Drag the 224-bar extended mix into your DAW as MIDI, with its section markers: the vox switch picks the "
                         "file, muted lanes are left out, and 1 drum track puts its drums on one track. Click to show the file.");
        drums.setComponentID ("drag_drums");
        drums.setTooltip ("1 drum track. Lit: the Loop and Song drags bring all their drums as one track, Drums (channel 10, the same "
                          "notes), for one Drum Rack in Live or one Drum Machine Designer in Logic; every other instrument keeps its "
                          "own track. Off: one track per drum part. Kept for your user, not saved in the project.");
        drums.onClick = [this] { controller.setDragDrumsOnOneTrack (drums.getToggleState()); };
        addAndMakeVisible (loop);
        addAndMakeVisible (song);
        addAndMakeVisible (drums);
    }

    void DragStrip::update (const EditorModel& model)
    {
        // The person's setting, which another instance may change too: shown as it is every tick.
        drums.setToggleState (model.dragDrumsOnOneTrack, juce::dontSendNotification);

        if (model.phraseFiles == files && model.kit.id == kitId)
            return;

        files = model.phraseFiles;
        kitId = model.kit.id;
        phraseChips.clear();
        for (int i = 0; i < files.size(); ++i)
        {
            DragRequest request;
            request.what = DragWhat::phrase;
            request.phraseFile = files[i];
            auto* chip = phraseChips.add (new DragChip (controller, request, phraseTitle (files[i], kitId)));
            chip->setComponentID ("drag_phrase_" + juce::String (i));
            chip->setTooltip ("Drag " + files[i] + " into your DAW. Click to show the file.");
            addAndMakeVisible (chip);
        }
        resized();
    }

    void DragStrip::resized()
    {
        auto area = getLocalBounds().reduced (8, 5);
        area.removeFromLeft (68); // the engraved label (labelArea ())
        loop.setBounds (area.removeFromLeft (80));
        area.removeFromLeft (6);
        song.setBounds (area.removeFromLeft (80));
        area.removeFromLeft (6);
        drums.setBounds (area.removeFromLeft (120));
        area.removeFromLeft (12); // the divider, in the middle

        if (phraseChips.isEmpty())
            return;
        constexpr auto gap = 6;
        const auto width = juce::jlimit (60, 176, (area.getWidth() - gap * (phraseChips.size() - 1)) / phraseChips.size());
        for (auto* chip : phraseChips)
        {
            chip->setBounds (area.removeFromLeft (width));
            area.removeFromLeft (gap);
        }
    }

    juce::Rectangle<float> DragStrip::labelArea() const
    {
        return getLocalBounds().toFloat().reduced (10.0f, 5.0f).withWidth (64.0f);
    }

    void DragStrip::paint (juce::Graphics& g)
    {
        draw::engraved (g, "Drag MIDI", labelArea(), juce::Justification::centredLeft, 9.5f);
        if (! phraseChips.isEmpty())
        {
            const auto x = (float) drums.getRight() + 6.0f;
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (1.0f, (float) getHeight() * 0.5f).withCentre ({ x, (float) getHeight() * 0.5f }));
        }
    }
}
