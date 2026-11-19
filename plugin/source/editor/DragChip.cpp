#include "DragChip.h"
#include "ExternalDrag.h"
#include "LookAndFeel.h"
#include "editor/skin/Paint.h"

#include "dragout/DragFiles.h"

#include <cmath>

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

        // Where a chip with these bounds writes its label: right of the MIDI glyph, as paint () lays the chip out.
        juce::Rectangle<float> labelAreaIn (juce::Rectangle<float> bounds, bool compact)
        {
            return bounds.reduced (compact ? 4.0f : 10.0f, 2.0f);
        }

        const TextStyle& chipStyle (const Skin& skin, bool compact) { return compact ? skin.type.laneChip : skin.type.stripChip; }
    }

    DragChip::DragChip (Controller& c, const DragRequest& request, const juce::String& label)
        : controller (c)
    {
        // The functions themselves, not lambdas around them: a test checks each default is the one (DragOutTests).
        startDrag = startExternalDrag;
        revealFile = dragout::revealToUser;
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

    void DragChip::setTint (juce::Colour colour)
    {
        if (colour != tint)
        {
            tint = colour;
            repaint();
        }
    }

    std::vector<DrawnText> DragChip::drawnText() const
    {
        return { fitText (chipStyle (skinOf (*this), compact), shownLabel(), labelArea(), juce::Justification::centred) };
    }

    void DragChip::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto enabled = isEnabled();
        const auto active = pressed || dragging;
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        const auto colour = compact && ! tint.isTransparent() ? tint : c.accent;
        const auto edge = failed ? c.warning : ! enabled ? c.controlEdge : compact ? colour : c.accent;
        const auto fill = active ? over (compact ? colour.withAlpha (0.25f) : c.accentTint, c.face)
                                 : (hover && enabled ? over (compact ? colour.withAlpha (0.12f) : c.accentTint, c.face) : (compact ? juce::Colours::transparentBlack : over (c.accentTint, c.face)));
        paint::box (g, area, compact ? skin.shape.laneChip : skin.shape.stripChip, fill, edge);
        const auto ink = ! enabled ? c.faint : failed ? c.warning : compact ? colour : c.accentTintText;
        for (const auto& t : drawnText())
            drawText (g, t, ink);
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
        if (juce::GlyphArrangement::getStringWidth (labelFont(), chipStyle (skinOf (*this), compact).apply (text)) <= labelArea().getWidth())
            return text;
        return withoutBarCount (text);
    }

    juce::Font DragChip::labelFont() const
    {
        return chipStyle (skinOf (*this), compact).font();
    }

    juce::Rectangle<float> DragChip::labelArea() const
    {
        return labelAreaIn (getLocalBounds().toFloat(), compact);
    }

    int DragChip::fitWidth (int height) const
    {
        const juce::Rectangle<float> wide (1000.0f, (float) height);
        const auto around = wide.getWidth() - labelAreaIn (wide, compact).getWidth(); // the border, padding and glyph
        const auto& style = chipStyle (skinOf (*this), compact);
        const auto title = juce::GlyphArrangement::getStringWidth (style.font(), style.apply (withoutBarCount (text)));
        return (int) std::ceil (title + around) + 1;
    }

    namespace
    {
        juce::String partWord (DragPart part)
        {
            return juce::String (std::string (kDragPartNames[static_cast<std::size_t> (part)]));
        }

        // The kit's labels for the part's lanes, skipping lanes it lacks: "Kick, Clap, Hats, Perc".
        juce::String partLabels (const EditorModel& model, DragPart part)
        {
            juce::StringArray labels;
            for (int lane = 0; lane < engine::kLanes; ++lane)
                if (const auto l = static_cast<engine::Lane> (lane); (dragPartLanes (part) & engine::laneBit (l)) != 0 && model.hasLane (l))
                    labels.add (model.laneLabel (l));
            return labels.joinIntoString (", ");
        }

        // "drums (Kick, Clap, Hats, Perc)", or the word alone where the kit has none of its lanes.
        juce::String partWithLabels (const EditorModel& model, DragPart part, bool lowercase)
        {
            const auto word = lowercase ? partWord (part).toLowerCase() : partWord (part);
            const auto labels = partLabels (model, part);
            return labels.isEmpty() ? word : word + " (" + labels + ")";
        }

        const juce::String loopAllTip = "Drag the 4-bar loop into your DAW as MIDI: the vox switch picks the vox or no-vox file, muted "
                                        "lanes are left out, and 1 drum track puts its drums on one track. Click to show the file.";
        const juce::String songAllTip = "Drag the 224-bar extended mix into your DAW as MIDI, with its section markers: the vox switch "
                                        "picks the file, muted lanes are left out, and 1 drum track puts its drums on one track. Click to "
                                        "show the file.";

        juce::String chipTip (const EditorModel& model, bool song)
        {
            if (model.dragPart == DragPart::all)
                return song ? songAllTip : loopAllTip;
            return juce::String (song ? "Drag the 224-bar extended mix's " : "Drag the 4-bar loop's ")
                   + partWithLabels (model, model.dragPart, true) + " into your DAW as MIDI, "
                   + (song ? "with its section markers, " : "") + "whatever you muted"
                   + (model.dragPart == DragPart::drums ? "; 1 drum track puts them on one track" : "") + ". Click to show the file.";
        }
    }

    DragStrip::DragStrip (Controller& c)
        : loop (c, { DragWhat::loop, engine::Lane::kick, {} }, "Loop"),
          song (c, { DragWhat::song, engine::Lane::kick, {} }, "Song"),
          drumKit (c),
          controller (c)
    {
        setComponentID ("drag");
        loop.setComponentID ("drag_loop");
        loop.setTooltip (loopAllTip);
        loop.setHelpText (loopAllTip);
        song.setComponentID ("drag_song");
        song.setTooltip (songAllTip);
        song.setHelpText (songAllTip);
        drums.setComponentID ("drag_drums");
        drums.setTooltip ("1 drum track. Lit: the Loop and Song drags bring all their drums as one track, Drums (channel 10, the same "
                          "notes), for one Drum Rack in Live or one Drum Machine Designer in Logic; every other instrument keeps its "
                          "own track. Off: one track per drum part. Kept for your user, not saved in the project.");
        drums.onClick = [this] { controller.setDragDrumsOnOneTrack (drums.getToggleState()); };
        synths.setComponentID ("drag_synths");
        synths.setTooltip ("For my synths. Lit (the default), every MIDI drag is made for your own synths: the bass an octave up, "
                           "chords as whole chords above it, no note shorter than a 16th, no notes that overlap, soft notes at "
                           "velocity 72. The drums and the one-key Accents and FX tracks stay as they are. Off: the files as "
                           "Wubansia's samples play them, for playing a drag back into Wubansia. Kept for your user, not saved in "
                           "the project. More in INSTALL.md, \"Drag MIDI into Live or Logic\".");
        synths.onClick = [this] { controller.setDragForSynths (synths.getToggleState()); };
        part.setComponentID ("drag_part");
        part.setTitle ("Part");
        for (const auto word : kDragPartNames)
            part.addItem (juce::String (std::string (word)), part.getNumItems() + 1);
        part.setSegmentsFitLabels (true);
        part.setRole (SegmentedChoice::Role::strip);
        drums.setRole (LedButton::Role::stripSwitch);
        synths.setRole (LedButton::Role::stripSwitch);
        part.setExplicitFocusOrder (1);
        drumKit.setExplicitFocusOrder (2);
        drums.setExplicitFocusOrder (3);
        synths.setExplicitFocusOrder (4);
        part.setSelectedItemIndex (0, juce::dontSendNotification);
        part.onChange = [this] { controller.setDragPart (static_cast<DragPart> (juce::jlimit (0, kDragParts - 1, part.getSelectedItemIndex()))); };
        addAndMakeVisible (synths);
        addAndMakeVisible (loop);
        addAndMakeVisible (song);
        addAndMakeVisible (part);
        addAndMakeVisible (drums);
        addAndMakeVisible (drumKit);
    }

    juce::String whyNoDrag (const EditorModel& model, bool song)
    {
        const auto mode = song ? engine::PlayMode::song : engine::PlayMode::loop;
        const auto lanesIn = [&] (engine::Variant v) { return model.kit.fileLanes[static_cast<std::size_t> (engine::timelineIndex (mode, v))]; };
        const juce::String file = song ? "extended mix" : "loop";
        if (model.dragPart != DragPart::all)
        {
            if (((lanesIn (engine::Variant::vox) | lanesIn (engine::Variant::novox)) & dragPartLanes (model.dragPart)) != 0)
                return {};
            return model.kit.id + "'s " + file + " has no " + partWithLabels (model, model.dragPart, true)
                   + (song ? ": pick another part." : ": drag the Song, or pick another part.");
        }
        if ((lanesIn (model.vox ? engine::Variant::vox : engine::Variant::novox) & ~model.mutedLanes) != 0)
            return {};
        return "Every lane with notes in the " + file + " is muted: unmute one to drag it.";
    }

    void DragStrip::update (const EditorModel& model)
    {
        // The person's settings, which another instance may change too: shown as they are every tick.
        drums.setToggleState (model.dragDrumsOnOneTrack, juce::dontSendNotification);
        synths.setToggleState (model.dragForSynths, juce::dontSendNotification);

        // The part is the Controller's: shown as it is, and what Loop and Song say follows it.
        part.setSelectedItemIndex (static_cast<int> (model.dragPart), juce::dontSendNotification);
        if (const auto key = model.kit.id + "|" + juce::StringArray (model.kit.laneLabels.data(), engine::kLanes).joinIntoString ("|");
            key != partTipKit)
        {
            partTipKit = key;
            juce::StringArray parts;
            for (int i = 1; i < kDragParts; ++i)
                parts.add (partWithLabels (model, static_cast<DragPart> (i), false));
            part.setTooltip ("Part: what Loop and Song bring. All: the whole kit, muted lanes left out. Or one part, whatever you "
                             "muted: " + parts.joinIntoString (", ") + ". Back to All when the project opens.");
        }
        for (auto* chip : { &loop, &song })
        {
            const bool isSong = chip == &song;
            const auto why = whyNoDrag (model, isSong);
            const auto tip = why.isNotEmpty() ? why : chipTip (model, isSong);
            chip->setEnabled (why.isEmpty());
            if (chip->getTooltip() != tip)
            {
                chip->setTooltip (tip);
                chip->setHelpText (tip);
            }
            const auto title = "Drag MIDI: " + chip->label()
                               + (model.dragPart == DragPart::all ? juce::String() : ", " + partWord (model.dragPart).toLowerCase());
            if (chip->getTitle() != title)
                chip->setTitle (title);
        }
        drumKit.update (model);
    }

    void DragStrip::resized()
    {
        // The look's label, then For my synths, Loop, Song, the Part label and Part, 1 drum track and Drum kit, each as
        // wide as its words in the look (ADR 0032; commit 11 puts them in the boards' order).
        const auto& skin = skinOf (*this);
        auto area = getLocalBounds().reduced (8, 5);
        const auto h = area.getHeight();
        const auto labelWidth = [&] (const TextStyle& style, const juce::String& words)
        { return (int) std::ceil (juce::GlyphArrangement::getStringWidth (style.font(), style.apply (words))) + 2; };
        area.removeFromLeft (labelWidth (skin.type.stripLabel, skin.words.stripLabel) + 10);
        synths.setBounds (area.removeFromLeft (synths.fitWidth (h)));
        area.removeFromLeft (6);
        loop.setBounds (area.removeFromLeft (loop.fitWidth (h)));
        area.removeFromLeft (6);
        song.setBounds (area.removeFromLeft (song.fitWidth (h)));
        area.removeFromLeft (10);
        area.removeFromLeft (labelWidth (skin.type.stripLabel, skin.words.partLabel) + 8);
        part.setBounds (area.removeFromLeft (part.fitWidth (h)));
        area.removeFromLeft (8);
        drums.setBounds (area.removeFromLeft (drums.fitWidth (h)));
        area.removeFromLeft (6);
        drumKit.setBounds (area.removeFromLeft (juce::jmin (area.getWidth(), drumKit.fitWidth (h))));
    }

    juce::Rectangle<float> DragStrip::labelArea() const
    {
        return { 10.0f, 5.0f, (float) synths.getX() - 12.0f, (float) getHeight() - 10.0f };
    }

    juce::Rectangle<float> DragStrip::partLabelArea() const
    {
        return { (float) song.getRight() + 10.0f, (float) part.getY(), (float) part.getX() - (float) song.getRight() - 12.0f, (float) part.getHeight() };
    }

    std::vector<DrawnText> DragStrip::drawnText() const
    {
        const auto& skin = skinOf (*this);
        return { fitText (skin.type.stripLabel, skin.words.stripLabel, labelArea()), fitText (skin.type.stripLabel, skin.words.partLabel, partLabelArea()) };
    }

    void DragStrip::paint (juce::Graphics& g)
    {
        const auto& c = skinOf (*this).colours;
        for (const auto& t : drawnText())
            drawText (g, t, c.label);
    }
}
