#include "Lcd.h"
#include "KitPanel.h"
#include "LookAndFeel.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr double ticksPerBeat = 960.0;
        constexpr double ticksPerBar = ticksPerBeat * 4.0;

        // Lit text with a soft bloom, as on a backlit display.
        void lcdText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
            juce::Justification justification, juce::Colour colour)
        {
            g.setFont (font);
            g.setColour (colour.withAlpha (colour.getFloatAlpha() * 0.22f));
            for (const auto offset : { juce::Point<float> (-0.7f, 0.0f), juce::Point<float> (0.7f, 0.0f), juce::Point<float> (0.0f, 0.7f) })
                g.drawText (text, area.translated (offset.x, offset.y), justification, true);
            g.setColour (colour);
            g.drawText (text, area, justification, true);
        }

        juce::String positionOf (const Snapshot& snapshot)
        {
            if (! snapshot.playing || snapshot.songTicks < 0.0)
                return "---.-";
            const auto bar = (int) std::floor (snapshot.songTicks / ticksPerBar) + 1;
            const auto beat = (int) std::floor (std::fmod (snapshot.songTicks, ticksPerBar) / ticksPerBeat) + 1;
            // Leading zeros stay unlit, as on a real display: "  3.2".
            return juce::String::formatted ("%3d.%d", juce::jlimit (0, 999, bar), juce::jlimit (1, 9, beat));
        }

        juce::String modeOf (const EditorModel& model)
        {
            const auto dot = middleDot();
            switch (model.playMode)
            {
                case engine::PlayMode::off:
                    return "PLAYER OFF" + dot + "MIDI IN PLAYS";
                case engine::PlayMode::loop:
                    return "LOOP" + dot + "4 BARS" + dot + (model.vox ? "VOX SYNTH" : "NO VOX");
                case engine::PlayMode::song:
                    break;
            }

            const auto& s = model.snapshot;
            const auto index = s.section;
            if (s.playing && index >= 0 && index < (int) model.sections.size())
            {
                const auto& section = model.sections[(std::size_t) index];
                const auto bar = (int) std::floor (s.songTicks / ticksPerBar) + 1 - (section.startBar - 1);
                return "SONG" + dot + section.name.toUpperCase() + dot + "BAR " + juce::String (juce::jlimit (1, juce::jmax (1, section.bars), bar))
                       + "/" + juce::String (section.bars);
            }

            auto bars = 0;
            for (const auto& section : model.sections)
                bars = juce::jmax (bars, section.startBar - 1 + section.bars);
            return "SONG" + dot + (bars > 0 ? juce::String (bars) + " BARS" : juce::String ("EXTENDED MIX"));
        }
    }

    Lcd::Lcd()
    {
        setComponentID ("lcd");
        setInterceptsMouseClicks (false, false);
    }

    void Lcd::update (const EditorModel& model)
    {
        const auto& s = model.snapshot;
        Lines next;
        next.playing = s.playing;
        next.accent = model.accent;
        next.transport = s.playing ? "PLAYING" : "STOPPED";
        next.sync = model.standalone ? "STANDALONE" : (model.transport.internal || model.preview ? "PREVIEW" : "HOST SYNC");

        const auto bpm = model.tempo();
        next.tempo = bpm > 0.0 ? juce::String (bpm, 1) : juce::String ("---.-");
        next.position = positionOf (s);

        next.kitId = model.hasKit() ? model.kit.id : juce::String ("-----");
        next.kitTitle = model.hasKit() ? model.kit.title.toUpperCase() : juce::String ("NO KIT");
        next.key = model.hasKit() ? model.kit.key.toUpperCase() + middleDot() + model.kit.camelot : juce::String();
        next.direction = model.kit.direction.toUpperCase();
        next.mode = modeOf (model);
        next.status = model.content.state == ContentState::ready ? "VOICES " + juce::String (s.voices) : contentBadge (model.content);

        if (next != shown)
        {
            shown = next;
            repaint();
        }
    }

    void Lcd::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto lit = shown.accent;
        const auto dim = lit.withAlpha (0.55f);

        // Bezel and glass.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 11.0f);
        const auto bezel = bounds.reduced (0.5f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xff30343b), bezel.getY(), juce::Colour (0xff121417), bezel.getBottom()));
        g.fillRoundedRectangle (bezel, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (bezel, 10.0f, 1.0f);

        const auto glass = bezel.reduced (6.0f);
        g.setGradientFill (juce::ColourGradient::vertical (lit.withAlpha (0.13f).overlaidWith (juce::Colour (0xff050709)), glass.getY(),
            lit.withAlpha (0.05f).overlaidWith (juce::Colour (0xff020304)), glass.getBottom()));
        g.fillRoundedRectangle (glass, 6.0f);

        juce::Path glassShape;
        glassShape.addRoundedRectangle (glass, 6.0f);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (glassShape);

            // Pixel rows and an inner shadow.
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            for (auto y = glass.getY(); y < glass.getBottom(); y += 2.0f)
                g.fillRect (glass.getX(), y, glass.getWidth(), 0.7f);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.65f), glass.getY(),
                juce::Colours::transparentBlack, glass.getY() + 10.0f));
            g.fillRect (glass);

            auto inner = glass.reduced (14.0f, 9.0f);
            auto left = inner.removeFromLeft (inner.getWidth() * 0.58f);
            auto right = inner.withTrimmedLeft (18.0f);

            // Transport row.
            auto top = left.removeFromTop (14.0f);
            const auto icon = top.removeFromLeft (10.0f).withSizeKeepingCentre (8.0f, 8.0f);
            juce::Path mark;
            if (shown.playing)
                mark.addTriangle (icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY());
            else
                mark.addRectangle (icon.reduced (0.5f));
            g.setColour (lit);
            g.fillPath (mark);
            top.removeFromLeft (6.0f);
            lcdText (g, shown.transport, top, bodyFont (skinOf (*this), 11.0f, true, 0.14f), juce::Justification::centredLeft, lit);
            lcdText (g, shown.sync, top, bodyFont (skinOf (*this), 10.0f, true, 0.14f), juce::Justification::centredRight, dim);

            // The digits.
            left.removeFromTop (8.0f);
            auto labels = left.removeFromBottom (12.0f);
            auto digits = left;
            const auto half = digits.getWidth() * 0.5f;
            auto tempoArea = digits.removeFromLeft (half).withTrimmedRight (16.0f);
            auto positionArea = digits.withTrimmedRight (2.0f);
            paint::sevenSegment (g, shown.tempo, tempoArea, lit, 0.075f);
            paint::sevenSegment (g, shown.position, positionArea, lit, 0.075f);
            g.setColour (lit.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (1.0f, digits.getHeight() * 0.7f).withCentre ({ tempoArea.getRight() + 9.0f, digits.getCentreY() }));
            lcdText (g, "BPM", labels.removeFromLeft (half).withTrimmedRight (16.0f), bodyFont (skinOf (*this), 9.5f, true, 0.2f), juce::Justification::centredRight, dim);
            lcdText (g, "BAR" + middleDot() + "BEAT", labels.withTrimmedRight (2.0f), bodyFont (skinOf (*this), 9.5f, true, 0.2f), juce::Justification::centredRight, dim);

            // The divider and the kit block.
            g.setColour (lit.withAlpha (0.16f));
            g.fillRect (juce::Rectangle<float> (1.0f, inner.getHeight() - 6.0f).withCentre ({ right.getX() - 9.0f, inner.getCentreY() }));

            const auto line = right.getHeight() / 5.0f;
            auto row1 = right.removeFromTop (line);
            lcdText (g, shown.kitId, row1, bodyFont (skinOf (*this), 10.5f, true, 0.16f), juce::Justification::centredLeft, dim);
            lcdText (g, shown.status, row1, bodyFont (skinOf (*this), 9.5f, true, 0.14f), juce::Justification::centredRight, dim);
            lcdText (g, shown.kitTitle, right.removeFromTop (line * 1.25f), bodyFont (skinOf (*this), 17.0f, true, 0.06f), juce::Justification::centredLeft, lit);
            lcdText (g, shown.key, right.removeFromTop (line), bodyFont (skinOf (*this), 11.5f, true, 0.1f), juce::Justification::centredLeft, lit);
            lcdText (g, shown.direction, right.removeFromTop (line * 0.85f), bodyFont (skinOf (*this), 9.5f, true, 0.12f), juce::Justification::centredLeft, dim);
            lcdText (g, shown.mode, right, bodyFont (skinOf (*this), 11.0f, true, 0.1f), juce::Justification::centredLeft, lit);

            // The glass's sheen.
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.075f), glass.getX(), glass.getY(),
                juce::Colours::white.withAlpha (0.0f), glass.getX() + glass.getWidth() * 0.35f, glass.getBottom(), false));
            juce::Path sheen;
            sheen.startNewSubPath (glass.getX(), glass.getY());
            sheen.lineTo (glass.getX() + glass.getWidth() * 0.55f, glass.getY());
            sheen.lineTo (glass.getX() + glass.getWidth() * 0.32f, glass.getBottom());
            sheen.lineTo (glass.getX(), glass.getBottom());
            sheen.closeSubPath();
            g.fillPath (sheen);
        }

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (glass, 6.0f, 1.0f);
    }

    TransportPanel::TransportPanel (Controller& c, bool isStandalone)
        : controller (c), standalone (isStandalone)
    {
        setComponentID ("transport");

        if (standalone)
        {
            play.setComponentID ("transport_play");
            play.setTooltip ("Play or stop the kit on Wubansia's own clock");
            play.onClick = [this] { controller.setTransport (! transport.playing, transport.standaloneBpm); };
            slower.setComponentID ("tempo_down");
            slower.setTooltip ("Slower: 1 BPM, or 0.1 with shift");
            slower.onClick = [this] { nudgeTempo (juce::ModifierKeys::currentModifiers.isShiftDown() ? -0.1 : -1.0); };
            faster.setComponentID ("tempo_up");
            faster.setTooltip ("Faster: 1 BPM, or 0.1 with shift");
            faster.onClick = [this] { nudgeTempo (juce::ModifierKeys::currentModifiers.isShiftDown() ? 0.1 : 1.0); };
            kitTempo.setComponentID ("tempo_kit");
            kitTempo.setTooltip ("Back to the kit's own tempo");
            kitTempo.onClick = [this] { controller.setTransport (transport.playing, 0.0); };
            for (auto* b : { &play, &slower, &faster, &kitTempo })
                addAndMakeVisible (b);
        }
        else
        {
            preview.setComponentID ("preview");
            preview.setTooltip ("Preview plays the kit on Wubansia's own clock while your host is stopped");
            preview.setClickingTogglesState (false);
            preview.onClick = [this] { controller.setPreview (! controller.preview()); };
            addAndMakeVisible (preview);
        }
    }

    void TransportPanel::nudgeTempo (double delta)
    {
        const auto current = transport.standaloneBpm > 0.0 ? transport.standaloneBpm : (transport.bpm > 0.0 ? transport.bpm : (double) kitBpm);
        const auto next = juce::jlimit (60.0, 200.0, std::round ((current + delta) * 10.0) / 10.0);
        controller.setTransport (transport.playing, next);
    }

    void TransportPanel::update (const EditorModel& model)
    {
        transport = model.transport;
        kitBpm = model.kit.bpm;

        if (standalone)
        {
            const auto playing = transport.playing;
            play.setToggleState (playing, juce::dontSendNotification);
            play.setIcon (playing ? IconButton::Icon::stop : IconButton::Icon::play);
            kitTempo.setEnabled (transport.standaloneBpm > 0.0);
        }
        else
        {
            preview.setToggleState (model.preview, juce::dontSendNotification);
        }
    }

    void TransportPanel::resized()
    {
        auto area = getLocalBounds().reduced (10, 8);
        area.removeFromTop (16); // the engraved title

        if (standalone)
        {
            play.setBounds (area.removeFromTop (48).withSizeKeepingCentre (64, 48));
            area.removeFromTop (8);
            auto row = area.removeFromTop (30);
            const auto w = row.getWidth() / 3;
            slower.setBounds (row.removeFromLeft (w).reduced (2, 0));
            kitTempo.setBounds (row.removeFromLeft (w).reduced (2, 0));
            faster.setBounds (row.reduced (2, 0));
        }
        else
        {
            preview.setBounds (area.removeFromTop (30));
        }
    }

    void TransportPanel::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
        paint::engraved (g, "Transport", area.removeFromTop (12.0f), juce::Justification::centredLeft, 9.5f);

        if (! standalone)
        {
            area.removeFromTop (38.0f);
            g.setFont (bodyFont (skinOf (*this), 10.5f));
            g.setColour (skinOf (*this).colours.faint);
            g.drawFittedText ("Plays the kit while\nthe host is stopped", area.toNearestInt(), juce::Justification::centredLeft, 3, 0.9f);
        }
    }

    Lcd::GridView Lcd::grid() const { return {}; }
    std::vector<Lcd::Satellite> Lcd::satellites() const { return {}; }
    juce::StringArray Lcd::orbitLines() const { return {}; }
}
