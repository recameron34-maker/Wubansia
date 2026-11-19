#include "Lcd.h"

#include "KitPanel.h"
#include "LookAndFeel.h"
#include "editor/ChordName.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"
#include "services/PatternView.h"

#include <algorithm>
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

    namespace
    {
        constexpr std::array<juce::Point<float>, 3> orbitRadii { { { 54.0f, 20.0f }, { 92.0f, 34.0f }, { 130.0f, 48.0f } } };
        const juce::Point<float> orbitCentre { 160.0f, 66.0f };

        juce::Rectangle<float> discArea (const Skin& skin, const juce::String& chord)
        {
            const auto w = juce::GlyphArrangement::getStringWidth (skin.type.displayLabel.font(), chord);
            const auto r = std::max (11.0f, w * 0.5f + 4.0f);
            return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (orbitCentre);
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

    void Lcd::updateOrbit (const EditorModel& model)
    {
        // Mission HUD: the lead's notes of the playing bar around the chord, each on its octave's ring (ADR 0032).
        const auto dot = middleDot();
        const auto cur = model.patternStep;
        const auto bar = cur >= 0 ? cur / 16 * 16 : 0;
        const auto flats = model.pattern != nullptr ? model.pattern->flats : keyUsesFlats (model.kit.key);
        std::vector<Satellite> next;
        int lowestOctave = 99, low = 999;
        std::vector<int> keys;
        if (const auto& p = model.pattern; p != nullptr && p->steps > 0)
        {
            for (const auto& n : p->lanes[(std::size_t) engine::Lane::lead])
                lowestOctave = std::min (lowestOctave, n.key / 12 - 1);
            for (const auto& n : p->notesIn (engine::Lane::lead, bar, bar + 16))
            {
                if (n.step < bar)
                    continue; // held in from the bar before: not this bar's
                Satellite sat;
                sat.step = n.step - bar;
                sat.key = n.key;
                sat.ring = juce::jlimit (0, 2, n.key / 12 - 1 - lowestOctave);
                sat.state = cur < 0 || n.step > cur ? Satellite::State::upcoming
                            : (cur < n.end() ? Satellite::State::sounding : Satellite::State::played);
                next.push_back (sat);
                low = std::min (low, (int) n.key);
                if (std::find (keys.begin(), keys.end(), (int) n.key) == keys.end())
                    keys.push_back (n.key);
            }
        }
        std::sort (keys.begin(), keys.end());
        juce::StringArray top;
        for (auto i = std::max<std::size_t> (0, keys.size() >= 4 ? keys.size() - 4 : 0); i < keys.size(); ++i)
            top.add (noteName (keys[i], flats));

        juce::String where;
        if (model.playMode == engine::PlayMode::off)
            where = "PLAYER OFF" + dot + "MIDI IN";
        else if (cur < 0)
            where = "STANDBY" + dot + (model.playMode == engine::PlayMode::song ? "SONG" : "LOOP");
        else if (model.playMode == engine::PlayMode::loop)
            where = "LOOP" + dot + "BAR " + juce::String (cur / 16 % 4 + 1) + " OF 4";
        else if (const auto s = model.snapshot.section; s >= 0 && s < (int) model.sections.size())
        {
            const auto& section = model.sections[(std::size_t) s];
            where = section.name.toUpperCase() + dot + "BAR " + juce::String (cur / 16 + 2 - section.startBar) + " OF " + juce::String (section.bars);
        }
        else
            where = "SONG" + dot + "BAR " + juce::String (cur / 16 + 1);
        const auto bpm = model.tempo();
        const juce::StringArray lines { model.laneLabel (engine::Lane::lead).toUpperCase() + dot + "LOW " + (low < 999 ? noteName (low, flats) : juce::String ("-")),
            "TOP " + (top.isEmpty() ? juce::String ("-") : top.joinIntoString (dot)), where,
            shortKey (model.kit.key) + dot + model.kit.camelot + dot + (bpm > 0.0 ? shortTempo (bpm) : juce::String ("---")) + " BPM" };
        const auto chord = model.chord.isNotEmpty() ? model.chord : shortKey (model.kit.key);
        if (next != orbit || lines != orbitText || chord != orbitChord)
        {
            orbit = std::move (next);
            orbitText = lines;
            orbitChord = chord;
            repaintArea (*this, viewArea().expanded (4));
        }
    }
    void Lcd::updateLevels (const EditorModel& model)
    {
        // Deep glass: the drums' and the music's levels over the last two bars at the playing tempo, one sample a tick;
        // stopped, the curves sink to the floor in place, and once flat they stop asking to be painted.
        constexpr float floorDb = -48.0f;
        const auto& s = model.snapshot;
        const auto bpm = model.tempo();
        const auto wanted = s.playing && bpm > 0.0 ? juce::jlimit (8, 120, (int) std::lround (8.0 * 60.0 / bpm * 30.0)) : 120;
        auto& h = levels;
        const auto resize = [wanted] (auto& v, auto fill)
        {
            while ((int) v.size() > wanted)
                v.erase (v.begin());
            while ((int) v.size() < wanted)
                v.insert (v.begin(), fill);
        };
        if (s.playing)
        {
            resize (h.drums, floorDb);
            resize (h.music, floorDb);
            resize (h.kicks, false);
            const auto dbOf = [floorDb] (float peak) { return peak > 0.0f ? std::max (floorDb, 20.0f * std::log10 (peak)) : floorDb; };
            float drums = floorDb, music = floorDb;
            for (int lane = 0; lane < engine::kLanes; ++lane)
                (lane < static_cast<int> (engine::Lane::bass) ? drums : music)
                    = std::max (lane < static_cast<int> (engine::Lane::bass) ? drums : music, dbOf (s.lanePeak[(std::size_t) lane]));
            h.drums.erase (h.drums.begin());
            h.music.erase (h.music.begin());
            h.kicks.erase (h.kicks.begin());
            h.drums.push_back (drums);
            h.music.push_back (music);
            h.kicks.push_back ((s.laneHits & engine::laneBit (engine::Lane::kick)) != 0);
            levelsMoving = true;
            repaintArea (*this, viewArea());
            return;
        }
        if (h.drums.empty())
        {
            resize (h.drums, floorDb);
            resize (h.music, floorDb);
            resize (h.kicks, false);
        }
        auto moved = false;
        for (auto* curve : { &h.drums, &h.music })
            for (auto& v : *curve)
                if (v > floorDb)
                {
                    v = std::max (floorDb, v - 2.0f);
                    moved = true;
                }
        if (moved || levelsMoving)
            repaintArea (*this, viewArea());
        levelsMoving = moved;
    }
    namespace
    {
        constexpr int kScopeHistory = 4096, kScopeWindow = 2048, kScopeColumns = 556;
        constexpr float kScopeScale = 34.0f; // full scale, in px either side of the centre line
    }

    void Lcd::updateScope (const EditorModel&)
    {
        if (trace.out.size() != (std::size_t) kScopeColumns)
        {
            trace.out.assign ((std::size_t) kScopeColumns, 0.0f);
            trace.sub.assign ((std::size_t) kScopeColumns, 0.0f);
        }
        if (scopeArrived)
        {
            scopeArrived = false;
            repaintArea (*this, viewArea().expanded (2));
        }
    }

    void Lcd::pushScope (const float* samples, int count)
    {
        if (scopeHistory.size() != (std::size_t) kScopeHistory)
        {
            scopeHistory.assign ((std::size_t) kScopeHistory, 0.0f);
            subHistory.assign ((std::size_t) kScopeHistory, 0.0f);
        }
        if (count <= 0)
            return;
        // Only the newest 4,096 matter; the low band still runs over every sample so it stays continuous.
        const auto a = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / 48000.0));
        const auto skip = std::max (0, count - kScopeHistory);
        for (int i = 0; i < skip; ++i)
            lowPassState += a * (samples[i] - lowPassState);
        const auto n = count - skip;
        const auto keep = (std::size_t) (kScopeHistory - n);
        std::move (scopeHistory.begin() + n, scopeHistory.end(), scopeHistory.begin());
        std::move (subHistory.begin() + n, subHistory.end(), subHistory.begin());
        for (int i = 0; i < n; ++i)
        {
            const auto x = samples[skip + i];
            lowPassState += a * (x - lowPassState);
            scopeHistory[keep + (std::size_t) i] = x;
            subHistory[keep + (std::size_t) i] = lowPassState;
        }
        scopeFill = std::min (kScopeHistory, scopeFill + n);
        scopeArrived = true;
        traceScope();
    }

    // The trigger: the steepest rising zero crossing of the low band within the first 2,048 samples (the earliest
    // when two are as steep), so a bass note with a second crossing in its period still stands still. The 2,048
    // samples from there become 556 columns, each the mean of its samples.
    void Lcd::traceScope()
    {
        int start = std::max (0, kScopeHistory - std::max (scopeFill, kScopeWindow));
        float best = 0.0f;
        for (int i = std::max (1, kScopeHistory - scopeFill); i < kScopeWindow; ++i)
        {
            const auto prev = subHistory[(std::size_t) i - 1], now = subHistory[(std::size_t) i];
            if (prev < 0.0f && now >= 0.0f && now - prev > best * 1.02f)
            {
                best = now - prev;
                start = i;
            }
        }
        start = std::min (start, kScopeHistory - kScopeWindow);
        trace.out.assign ((std::size_t) kScopeColumns, 0.0f);
        trace.sub.assign ((std::size_t) kScopeColumns, 0.0f);
        for (int c = 0; c < kScopeColumns; ++c)
        {
            const auto from = start + c * kScopeWindow / kScopeColumns;
            const auto to = std::max (from + 1, start + (c + 1) * kScopeWindow / kScopeColumns);
            float out = 0.0f, sub = 0.0f;
            for (int i = from; i < to; ++i)
            {
                out += scopeHistory[(std::size_t) i];
                sub += subHistory[(std::size_t) i];
            }
            const auto count = (float) (to - from);
            trace.out[(std::size_t) c] = juce::jlimit (-1.0f, 1.0f, out / count) * kScopeScale;
            trace.sub[(std::size_t) c] = juce::jlimit (-1.0f, 1.0f, sub / count) * kScopeScale;
        }
    }
    void Lcd::paintOrbit (juce::Graphics& g, const Skin& skin) const
    {
        const auto& c = skin.colours;
        const auto view = viewArea().toFloat();

        // The HUD's grid and corner brackets.
        g.setColour (c.grid);
        for (const auto x : { 148.0f, 295.0f, 442.0f })
            g.fillRect (x, view.getY(), 1.0f, view.getHeight());
        for (const auto y : { 30.0f, 60.0f, 90.0f })
            g.fillRect (view.getX(), y, view.getWidth(), 1.0f);
        g.setColour (c.accent);
        const auto bracket = [&g] (float x, float y, float dx, float dy)
        {
            g.fillRect (dx > 0 ? x : x - 12.0f, y, 12.0f, 2.0f);
            g.fillRect (x, dy > 0 ? y : y - 12.0f, 2.0f, 12.0f);
        };
        bracket (6.0f, 6.0f, 1.0f, 1.0f);
        bracket (582.0f, 112.0f, -1.0f, -1.0f);

        // Three rings, the playing 16th's tick on the outer one, the satellites, the chord in the middle.
        const float alphas[] = { 0.9f, 0.55f, 0.3f };
        for (std::size_t i = 0; i < orbitRadii.size(); ++i)
        {
            const auto r = orbitRadii[i];
            g.setColour (c.accent.withAlpha (alphas[i]));
            g.drawEllipse (juce::Rectangle<float> (r.x * 2.0f, r.y * 2.0f).withCentre (orbitCentre), 1.2f);
        }
        const auto at = [] (int step, int ring)
        {
            const auto a = -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * (float) (step % 16) / 16.0f;
            const auto r = orbitRadii[(std::size_t) juce::jlimit (0, 2, ring)];
            return juce::Point<float> (orbitCentre.x + r.x * std::cos (a), orbitCentre.y + r.y * std::sin (a));
        };
        for (const auto& sat : orbit)
        {
            const auto p = at (sat.step, sat.ring);
            if (sat.state == Satellite::State::sounding)
            {
                g.setColour (c.accent2.withAlpha (0.25f));
                g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (p));
                g.setColour (c.accent2);
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));
            }
            else
            {
                const auto played = sat.state == Satellite::State::played;
                g.setColour (c.text.withAlpha (played ? 0.4f : 0.7f));
                const auto d = played ? 5.0f : 6.0f;
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
            }
        }
        g.setColour (c.accent);
        g.fillEllipse (discArea (skin, orbitChord));
        const auto texts = drawnText();
        for (std::size_t i = 2; i < texts.size(); ++i)
            drawText (g, texts[i], i == 2 ? c.accentInk : (i == 5 ? c.accent2 : c.dim));
    }
    void Lcd::paintLevels (juce::Graphics& g, const Skin& skin) const
    {
        const auto& c = skin.colours;
        const auto view = viewArea().toFloat();
        const auto& h = levels;
        if (h.drums.size() < 2)
            return;
        const auto n = (int) h.drums.size();
        const auto pointOf = [&] (const std::vector<float>& v, int i)
        {
            const auto x = view.getX() + view.getWidth() * (float) i / (float) (n - 1);
            const auto y = view.getY() + juce::jmap (v[(std::size_t) i], -48.0f, 0.0f, 70.0f, 6.0f);
            return juce::Point<float> (x, y);
        };
        // Catmull-Rom through the samples, the newest at the right.
        const auto curve = [&] (const std::vector<float>& v)
        {
            juce::Path p;
            p.startNewSubPath (pointOf (v, 0));
            for (int i = 0; i + 1 < n; ++i)
            {
                const auto p0 = pointOf (v, std::max (0, i - 1)), p1 = pointOf (v, i), p2 = pointOf (v, i + 1), p3 = pointOf (v, std::min (n - 1, i + 2));
                p.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
            }
            return p;
        };
        g.setColour (c.music.withAlpha (0.8f));
        g.strokePath (curve (h.music), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.drums);
        g.strokePath (curve (h.drums), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        for (int i = 0; i < n; ++i)
            if (h.kicks[(std::size_t) i])
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pointOf (h.drums, i)));
    }
    void Lcd::paintScope (juce::Graphics& g, const Skin& skin) const
    {
        const auto& c = skin.colours;
        const auto view = viewArea().toFloat();
        const auto mid = view.getCentreY();
        // Graticule: the centre line and quarter divisions.
        g.setColour (c.grid);
        for (int i = 1; i < 8; ++i)
            g.fillRect (view.getX() + view.getWidth() * (float) i / 8.0f, view.getY(), 1.0f, view.getHeight());
        g.fillRect (view.getX(), mid - 17.0f, view.getWidth(), 1.0f);
        g.fillRect (view.getX(), mid + 17.0f, view.getWidth(), 1.0f);
        g.setColour (c.grid.withMultipliedAlpha (2.0f));
        g.fillRect (view.getX(), mid, view.getWidth(), 1.0f);
        if (trace.out.size() != (std::size_t) kScopeColumns)
            return;
        const auto line = [&] (const std::vector<float>& ys)
        {
            juce::Path p;
            for (std::size_t i = 0; i < ys.size(); ++i)
            {
                const auto x = view.getX() + (float) i + 0.5f;
                const auto y = mid - ys[i];
                if (i == 0)
                    p.startNewSubPath (x, y);
                else
                    p.lineTo (x, y);
            }
            return p;
        };
        const auto sub = line (trace.sub), out = line (trace.out);
        g.setColour (c.accent2.withAlpha (0.35f));
        g.strokePath (sub, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.setColour (c.accent2);
        g.strokePath (sub, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
        g.setColour (c.accent.withAlpha (0.3f));
        g.strokePath (out, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved));
        g.setColour (c.accent);
        g.strokePath (out, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));
        const auto texts = drawnText();
        for (std::size_t i = 2; i < texts.size(); ++i)
            drawText (g, texts[i], i == 2 ? c.accent : c.accent2);
    }

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
            out.push_back (fitText (skin.type.displayLabel, orbitChord, discArea (skin, orbitChord), juce::Justification::centred));
            const auto view = viewArea().toFloat();
            for (int i = 0; i < orbitText.size(); ++i)
                out.push_back (fitText (skin.type.displayLine, orbitText[i], { view.getX() + 313.0f, view.getY() + 2.0f + 18.0f * (float) i, 243.0f, 16.0f }));
        }
        else if (kind == DisplayKind::scope)
        {
            const auto view = viewArea().toFloat();
            out.push_back (fitText (skin.type.displayLabel, "OUT", { view.getX() + 2.0f, view.getY(), 40.0f, 12.0f }));
            out.push_back (fitText (skin.type.displayLabel, "SUB", { view.getX() + 2.0f, view.getBottom() - 12.0f, 40.0f, 12.0f }));
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
        if (model.standalone)
            return {};
        const auto tempo = shortTempo (model.tempo());
        const auto dot = middleDot();
        const auto previewing = model.preview || model.transport.internal;
        const auto hostPlaying = model.snapshot.playing && ! previewing;
        const auto ticks = model.snapshot.songTicks;
        const auto position = ticks >= 0.0 ? juce::String ((int) (ticks / ticksPerBar) + 1) + "." + juce::String ((int) std::fmod (ticks / ticksPerBeat, 4.0) + 1)
                                           : juce::String ("-.-");
        switch (skin.id)
        {
            case SkinId::missionHud:
                return hostPlaying ? "POS " + position : previewing ? "PREVIEW" + dot + "POS " + position : juce::String ("STANDBY");
            case SkinId::deepGlass:
                return hostPlaying ? "in sync" + dot + tempo : previewing ? "previewing" + dot + tempo : juce::String ("preview");
            case SkinId::machineRoom:
                return hostPlaying ? juce::String ("HOST SYNC") : juce::String ("PREVIEW");
            case SkinId::nightBus:
                break;
        }
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
