#include "Lcd.h"

#include "KitPanel.h"
#include "LookAndFeel.h"
#include "editor/ChordName.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"
#include "services/PatternView.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr double ticksPerBeat = 960.0;
        constexpr double ticksPerBar = ticksPerBeat * 4.0;

        // The kit's status words while its samples aren't in: the display's header says them at the right.
        juce::String statusWords (const ContentStatus& content)
        {
            switch (content.state)
            {
                case ContentState::ready: return {};
                case ContentState::loading: return "LOADING " + juce::String (juce::jlimit (0, 100, content.percent)) + " %";
                case ContentState::missing: return "NO SAMPLES";
                case ContentState::lfsPointers: return "LFS POINTERS";
                case ContentState::mismatch: return "WRONG FILES";
                case ContentState::failed: return "LOAD ERROR";
            }
            return {};
        }

        // Where playback is, for the header's right side: the section and song bar, the loop's bar, or neither.
        juce::String whereWords (const EditorModel& model)
        {
            const auto dot = middleDot();
            if (model.playMode == engine::PlayMode::off)
                return "PLAYER OFF" + dot + "MIDI IN";
            const auto& s = model.snapshot;
            if (! s.playing || s.songTicks < 0.0)
                return "STOPPED";
            const auto bar = (int) std::floor (s.songTicks / ticksPerBar);
            if (model.playMode == engine::PlayMode::loop)
                return "LOOP" + dot + "BAR " + juce::String (bar % 4 + 1);
            if (s.section >= 0 && s.section < (int) model.sections.size())
                return model.sections[(std::size_t) s.section].name.toUpperCase() + dot + "BAR " + juce::String (bar + 1);
            return "SONG" + dot + "BAR " + juce::String (bar + 1);
        }

        juce::String headerLeft (const EditorModel& model)
        {
            const auto dot = middleDot();
            if (! model.hasKit())
                return "NO KIT";
            const auto chord = model.chord.isNotEmpty() ? model.chord : shortKey (model.kit.key);
            const auto bpm = model.tempo();
            return chord + dot + model.kit.camelot + dot + (bpm > 0.0 ? shortTempo (bpm) : juce::String ("---")) + " BPM";
        }

        const std::array<engine::Lane, 3> gridLanes { engine::Lane::kick, engine::Lane::snare, engine::Lane::bass };

        juce::Colour cellColour (const Skin& skin, StepCell cell, juce::Colour lane)
        {
            switch (cell)
            {
                case StepCell::off: return over (skin.colours.cellOff, skin.colours.face);
                case StepCell::held: return lane.withAlpha (0.3f);
                case StepCell::hit: return lane.withAlpha (0.8f);
                case StepCell::accent: return lane;
            }
            return lane;
        }
    }

    Lcd::Lcd()
    {
        setComponentID ("lcd");
        setInterceptsMouseClicks (false, false); // a picture: clicks go through to nothing
    }

    void Lcd::update (const EditorModel& model)
    {
        const auto& skin = skinOf (*this);
        Lines next;
        next.left = headerLeft (model);
        next.ready = model.content.state == ContentState::ready;
        next.right = next.ready ? whereWords (model) : statusWords (model.content);
        if (next != shown)
        {
            shown = next;
            repaintArea (*this, headerArea().expanded (2));
        }
        const auto wasOff = playerOff;
        playerOff = model.playMode == engine::PlayMode::off;
        if (kind != skin.display || wasOff != playerOff)
        {
            kind = skin.display;
            repaintArea (*this, viewArea().expanded (4));
        }
        switch (kind)
        {
            case DisplayKind::stepGrid: updateGrid (model); break;
            case DisplayKind::orbit: updateOrbit (model); break;
            case DisplayKind::current: updateLevels (model); break;
            case DisplayKind::scope: updateScope (model); break;
            case DisplayKind::lcd: break;
        }
    }

    void Lcd::updateGrid (const EditorModel& model)
    {
        juce::StringArray labels;
        for (const auto lane : gridLanes)
            labels.add (model.laneLabel (lane).toUpperCase());
        GridView next;
        next.playing = model.patternStep;
        next.page = model.patternStep >= 0 ? model.patternStep / 32 * 32 : 0;
        if (const auto& p = model.pattern; p != nullptr && p->steps > 0)
            for (std::size_t row = 0; row < gridLanes.size(); ++row)
                for (const auto& n : p->notesIn (gridLanes[row], next.page, next.page + 32))
                    for (auto step = std::max (n.step, next.page); step < std::min (n.end(), next.page + 32); ++step)
                    {
                        auto& cell = next.rows[row][(std::size_t) (step - next.page)];
                        const auto value = step == n.step ? (n.velocity >= 100 ? StepCell::accent : StepCell::hit) : StepCell::held;
                        cell = std::max (cell, value);
                    }
        if (! (next == gridView) || labels != gridLabels)
        {
            gridView = next;
            gridLabels = labels;
            repaintArea (*this, viewArea().expanded (2));
        }
    }

    void Lcd::updateOrbit (const EditorModel&) {}
    void Lcd::updateLevels (const EditorModel&) {}
    void Lcd::updateScope (const EditorModel&) {}
    void Lcd::pushScope (const float*, int) {}
    void Lcd::paintOrbit (juce::Graphics&, const Skin&) const {}
    void Lcd::paintLevels (juce::Graphics&, const Skin&) const {}
    void Lcd::paintScope (juce::Graphics&, const Skin&) const {}

    std::vector<DrawnText> Lcd::drawnText() const
    {
        const auto& skin = skinOf (*this);
        const auto header = headerArea().toFloat();
        const auto rightStyle = skin.type.displayLineStrong;
        auto right = fitText (rightStyle, shown.right, header.withTrimmedLeft (header.getWidth() * 0.55f), juce::Justification::centredRight);
        const auto rightWidth = juce::GlyphArrangement::getStringWidth (right.font, right.text);
        std::vector<DrawnText> out { fitText (skin.type.displayLine, shown.left, header.withTrimmedRight (rightWidth + 16.0f)), right };
        if (kind == DisplayKind::stepGrid)
        {
            const auto view = viewArea().toFloat();
            for (int row = 0; row < gridLabels.size(); ++row)
                out.push_back (fitText (skin.type.displayLabel, gridLabels[row], { view.getX(), view.getY() + 2.0f + 25.0f * (float) row, 60.0f, 20.0f }));
        }
        else if (kind == DisplayKind::orbit)
        {
            const auto view = viewArea().toFloat();
            for (int i = 0; i < orbitText.size(); ++i)
                out.push_back (fitText (skin.type.displayLine, orbitText[i], { view.getX() + 313.0f, view.getY() + 2.0f + 18.0f * (float) i, 243.0f, 16.0f }));
        }
        return out;
    }

    void Lcd::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto texts = drawnText();
        drawText (g, texts[0], c.dim);
        drawText (g, texts[1], shown.ready ? c.accent2 : (shown.right.startsWith ("LOADING") ? c.busy : c.warning));

        juce::Graphics::ScopedSaveState state (g);
        if (playerOff)
            g.beginTransparencyLayer (0.4f); // the player is off: the Loop's notes, faint
        switch (kind)
        {
            case DisplayKind::stepGrid: paintGrid (g, skin); break;
            case DisplayKind::orbit: paintOrbit (g, skin); break;
            case DisplayKind::current: paintLevels (g, skin); break;
            case DisplayKind::scope: paintScope (g, skin); break;
            case DisplayKind::lcd: break;
        }
        if (playerOff)
            g.endTransparencyLayer();
    }

    void Lcd::paintGrid (juce::Graphics& g, const Skin& skin) const
    {
        const auto& c = skin.colours;
        const auto view = viewArea().toFloat();
        const auto texts = drawnText();
        for (std::size_t i = 2; i < texts.size(); ++i)
            drawText (g, texts[i], c.label);

        constexpr float cell = 13.0f, gap = 2.0f, beatGap = 4.0f, rowPitch = 25.0f, cellHeight = 15.0f;
        for (std::size_t row = 0; row < gridView.rows.size(); ++row)
        {
            const auto colour = row < 2 ? c.drums : c.music;
            auto x = view.getX() + 64.0f;
            const auto y = view.getY() + 4.0f + rowPitch * (float) row;
            for (int i = 0; i < 32; ++i)
            {
                const juce::Rectangle<float> box { x, y, cell, cellHeight };
                g.setColour (cellColour (skin, gridView.rows[row][(std::size_t) i], colour));
                g.fillRoundedRectangle (box, skin.shape.cell.corner);
                if (gridView.page + i == gridView.playing)
                {
                    g.setColour (c.text);
                    g.drawRoundedRectangle (box.expanded (0.5f), skin.shape.cell.corner, 1.0f);
                }
                x += cell + ((i + 1) % 4 == 0 ? beatGap : gap);
            }
        }
    }

    //==============================================================================================================
    juce::String syncWords (const Skin& skin, const EditorModel& model)
    {
        juce::ignoreUnused (skin);
        if (model.standalone)
            return {};
        const auto tempo = shortTempo (model.tempo());
        const auto dot = middleDot();
        const auto previewing = model.preview || model.transport.internal;
        const auto hostPlaying = model.snapshot.playing && ! previewing;
        if (hostPlaying)
            return "synced" + dot + tempo;
        if (previewing)
            return "previewing" + dot + tempo;
        return "preview";
    }

    TransportPanel::TransportPanel (Controller& c, bool isStandalone)
        : controller (c), standalone (isStandalone)
    {
        setComponentID ("transport");
        if (standalone)
        {
            play.setComponentID ("transport_play");
            play.setTooltip ("Play or stop the kit on Wubansia's own clock");
            play.setRound (true);
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
            int order = 1;
            for (auto* b : { &play, &slower, &kitTempo, &faster })
            {
                b->setExplicitFocusOrder (order++);
                addAndMakeVisible (b);
            }
        }
        else
        {
            preview.setComponentID ("preview");
            preview.setTooltip ("Preview plays the kit on Wubansia's own clock while your host is stopped");
            preview.setClickingTogglesState (false);
            preview.setRole (LedButton::Role::transport);
            preview.setExplicitFocusOrder (1);
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
            preview.setToggleState (model.preview, juce::dontSendNotification);

        const auto next = syncWords (skinOf (*this), model);
        const auto lit = model.snapshot.playing || model.preview;
        if (next != syncLine || lit != lineLit)
        {
            syncLine = next;
            lineLit = lit;
            repaintArea (*this, { 0, 74, getWidth(), 24 });
        }
    }

    void TransportPanel::resized()
    {
        const auto size = (int) skinOf (*this).shape.playButton;
        const auto button = juce::Rectangle<int> (size, size).withCentre ({ getWidth() / 2, 10 + size / 2 });
        if (standalone)
        {
            play.setBounds (button);
            slower.setBounds (10, 84, 32, 24);
            kitTempo.setBounds (48, 84, 32, 24);
            faster.setBounds (86, 84, 32, 24);
        }
        else
            preview.setBounds (button);
    }

    std::vector<DrawnText> TransportPanel::drawnText() const
    {
        if (standalone)
            return {};
        return { fitText (skinOf (*this).type.transportLine, syncLine, { 6.0f, 78.0f, (float) getWidth() - 12.0f, 16.0f }, juce::Justification::centred) };
    }

    void TransportPanel::paint (juce::Graphics& g)
    {
        const auto& c = skinOf (*this).colours;
        for (const auto& t : drawnText())
            drawText (g, t, lineLit ? c.text : c.dim);
    }
}
