#include "SectionStrip.h"

#include "LookAndFeel.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"

#include <cmath>
#include <utility>

namespace wubansia::editor
{
    namespace
    {
        constexpr double ticksPerBar = 3840.0; // 960 PPQ, 4/4
        constexpr float boxGap = 4.0f, minBox = 56.0f;

        int totalBars (const std::vector<SectionInfo>& sections)
        {
            auto bars = 0;
            for (const auto& s : sections)
                bars = juce::jmax (bars, s.startBar - 1 + s.bars);
            return bars;
        }
    }

    bool SectionStrip::State::operator== (const State& other) const
    {
        if (mode != other.mode || section != other.section || beat != other.beat || playheadPixel != other.playheadPixel
            || playing != other.playing || sections.size() != other.sections.size())
            return false;
        for (std::size_t i = 0; i < sections.size(); ++i)
            if (sections[i].name != other.sections[i].name || sections[i].startBar != other.sections[i].startBar
                || sections[i].bars != other.sections[i].bars)
                return false;
        return true;
    }

    SectionStrip::SectionStrip()
    {
        setComponentID ("sections");
        setInterceptsMouseClicks (false, false);
    }

    std::vector<juce::Rectangle<float>> SectionStrip::boxes() const
    {
        // Song: one box per section in proportion to its bars, none narrower than 56 px; Loop: four bars; Off: one.
        std::vector<juce::Rectangle<float>> out;
        const auto area = getLocalBounds().toFloat();
        if (shown.mode == engine::PlayMode::off || (shown.mode == engine::PlayMode::song && shown.sections.empty()))
            return { area };
        const auto count = shown.mode == engine::PlayMode::loop ? 4 : (int) shown.sections.size();
        const auto room = area.getWidth() - boxGap * (float) (count - 1);
        std::vector<float> widths ((std::size_t) count, room / (float) count);
        if (shown.mode == engine::PlayMode::song)
        {
            const auto bars = (float) juce::jmax (1, totalBars (shown.sections));
            // Sections too short for 56 px take 56; the rest share what's left in proportion.
            std::vector<bool> fixed ((std::size_t) count, false);
            for (auto pass = 0; pass < count; ++pass)
            {
                float fixedWidth = 0.0f, freeBars = 0.0f;
                for (int i = 0; i < count; ++i)
                    if (fixed[(std::size_t) i])
                        fixedWidth += minBox;
                    else
                        freeBars += (float) shown.sections[(std::size_t) i].bars;
                auto changed = false;
                for (int i = 0; i < count; ++i)
                {
                    if (fixed[(std::size_t) i])
                    {
                        widths[(std::size_t) i] = minBox;
                        continue;
                    }
                    const auto share = freeBars > 0.0f ? (room - fixedWidth) * (float) shown.sections[(std::size_t) i].bars / freeBars : 0.0f;
                    widths[(std::size_t) i] = share;
                    if (share < minBox)
                    {
                        fixed[(std::size_t) i] = true;
                        changed = true;
                    }
                }
                if (! changed)
                    break;
            }
            juce::ignoreUnused (bars);
        }
        auto x = area.getX();
        for (const auto w : widths)
        {
            out.push_back ({ x, area.getY(), w, area.getHeight() });
            x += w + boxGap;
        }
        return out;
    }

    void SectionStrip::update (const EditorModel& model)
    {
        State next;
        next.mode = model.playMode;
        next.sections = model.sections;
        next.playing = model.snapshot.playing;
        const auto ticks = model.snapshot.songTicks;
        if (next.playing && ticks >= 0.0)
        {
            if (next.mode == engine::PlayMode::loop)
                next.beat = (int) std::floor (std::fmod (ticks, ticksPerBar * 4.0) / 960.0) % 16;
            else if (next.mode == engine::PlayMode::song)
            {
                next.section = model.snapshot.section;
                if (next.section >= 0 && next.section < (int) next.sections.size())
                {
                    const auto& s = next.sections[(std::size_t) next.section];
                    const auto into = juce::jlimit (0.0, 1.0, (ticks / ticksPerBar - (double) (s.startBar - 1)) / (double) juce::jmax (1, s.bars));
                    const auto saved = std::exchange (shown, next);
                    const auto box = boxes()[(std::size_t) next.section].reduced (3.0f, 0.0f);
                    shown = saved;
                    next.playheadPixel = juce::roundToInt (box.getX() + (float) into * box.getWidth());
                }
            }
        }
        if (! (next == shown))
        {
            shown = std::move (next);
            repaintArea (*this);
        }
    }

    std::vector<DrawnText> SectionStrip::drawnText() const
    {
        const auto& skin = skinOf (*this);
        std::vector<DrawnText> out;
        const auto areas = boxes();
        const auto label = [&] (const juce::String& text, juce::Rectangle<float> box)
        {
            auto t = fitText (skin.type.section, text, box.reduced (8.0f, 0.0f));
            if (! t.cut)
                out.push_back (t); // too narrow even at 9 px: the box shows no name
        };
        if (shown.mode == engine::PlayMode::off)
            label ("PLAYER OFF" + middleDot() + "ONLY MIDI IN PLAYS THE KIT", areas.front());
        else if (shown.mode == engine::PlayMode::loop)
            for (int i = 0; i < 4; ++i)
                label ("BAR " + juce::String (i + 1), areas[(std::size_t) i].withTrimmedRight (40.0f));
        else
            for (std::size_t i = 0; i < shown.sections.size() && i < areas.size(); ++i)
                label (shown.sections[i].name, areas[i]);
        return out;
    }

    void SectionStrip::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto areas = boxes();
        const auto litIndex = shown.mode == engine::PlayMode::loop ? (shown.beat >= 0 ? shown.beat / 4 : -1) : shown.section;
        for (std::size_t i = 0; i < areas.size(); ++i)
        {
            const auto lit = (int) i == litIndex;
            const auto before = litIndex >= 0 && (int) i < litIndex;
            auto fill = over (lit ? c.sectionLit : c.sectionOff, c.face);
            if (before)
                fill = over (c.sectionOff, c.face).interpolatedWith (over (c.sectionLit, c.face), 0.6f);
            paint::box (g, areas[i].reduced (0.5f), skin.shape.section, fill, lit ? c.accent : juce::Colours::transparentBlack);
            if (shown.mode == engine::PlayMode::loop)
                for (int beat = 0; beat < 4; ++beat)
                {
                    const auto tick = juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ areas[i].getRight() - 34.0f + 9.0f * (float) beat, areas[i].getCentreY() });
                    g.setColour (lit && shown.beat % 4 == beat ? c.accent : c.controlEdge);
                    g.fillEllipse (tick);
                }
        }
        auto litTexts = drawnText();
        for (std::size_t i = 0; i < litTexts.size(); ++i)
        {
            const auto centre = litTexts[i].area.getCentreX();
            auto lit = false;
            if (litIndex >= 0 && litIndex < (int) areas.size())
                lit = areas[(std::size_t) litIndex].getX() <= centre && centre <= areas[(std::size_t) litIndex].getRight();
            drawText (g, litTexts[i], lit ? c.sectionLitText : c.sectionOffText);
        }
        if (shown.playheadPixel >= 0)
        {
            g.setColour (c.text);
            g.fillRect ((float) shown.playheadPixel - 1.0f, 4.0f, 2.0f, (float) getHeight() - 8.0f);
        }
    }
}
