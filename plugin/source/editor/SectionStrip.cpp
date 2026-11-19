#include "SectionStrip.h"
#include "LookAndFeel.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr double ticksPerBar = 3840.0; // 960 PPQ, 4/4
        constexpr float labelWidth = 104.0f;

        int totalBars (const std::vector<SectionInfo>& sections)
        {
            auto bars = 0;
            for (const auto& s : sections)
                bars = juce::jmax (bars, s.startBar - 1 + s.bars);
            return bars;
        }

        // How hard a section hits, from its name, for the song map's block heights.
        float energyOf (const juce::String& name)
        {
            const auto n = name.toLowerCase();
            if (n.contains ("drop"))
                return 1.0f;
            if (n.contains ("switch"))
                return 0.86f;
            if (n.contains ("groove") || n.contains ("main"))
                return 0.74f;
            if (n.contains ("build") || n.contains ("rise"))
                return 0.64f;
            if (n.contains ("intro") || n.contains ("outro"))
                return 0.52f;
            if (n.contains ("break"))
                return 0.42f;
            return 0.6f;
        }

        juce::Rectangle<float> trackOf (juce::Rectangle<float> bounds)
        {
            return bounds.withTrimmedLeft (labelWidth);
        }

        // Where the blocks and beats go, inside the track's well.
        juce::Rectangle<float> innerTrackOf (juce::Rectangle<float> bounds)
        {
            return trackOf (bounds).reduced (5.0f, 4.0f);
        }
    }

    bool SectionStrip::State::operator== (const State& other) const
    {
        if (mode != other.mode || section != other.section || beat != other.beat || playheadPixel != other.playheadPixel
            || playing != other.playing || accent != other.accent || sections.size() != other.sections.size())
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

    void SectionStrip::update (const EditorModel& model)
    {
        State next;
        next.mode = model.playMode;
        next.sections = model.sections;
        next.playing = model.snapshot.playing;
        next.accent = model.accent;

        const auto ticks = model.snapshot.songTicks;
        if (next.playing && ticks >= 0.0)
        {
            if (next.mode == engine::PlayMode::loop)
                next.beat = (int) std::floor (std::fmod (ticks, ticksPerBar * 4.0) / 960.0) % 16;
            else if (next.mode == engine::PlayMode::song)
            {
                next.section = model.snapshot.section;
                if (const auto bars = totalBars (next.sections); bars > 0)
                {
                    const auto track = innerTrackOf (getLocalBounds().toFloat());
                    const auto proportion = juce::jlimit (0.0, 1.0, ticks / ticksPerBar / (double) bars);
                    next.playheadPixel = juce::roundToInt (track.getX() + proportion * track.getWidth());
                }
            }
        }

        if (! (next == shown))
        {
            shown = std::move (next);
            repaint();
        }
    }

    void SectionStrip::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto lit = shown.accent;
        auto label = bounds.withWidth (labelWidth - 8.0f);
        paint::recess (g, trackOf (bounds), 6.0f);
        const auto track = innerTrackOf (bounds);

        switch (shown.mode)
        {
            case engine::PlayMode::off:
            {
                paint::engraved (g, "Player off", label, juce::Justification::centredLeft, 9.5f);
                g.setFont (bodyFont (skinOf (*this), 11.0f, true, 0.1f));
                g.setColour (skinOf (*this).colours.faint);
                g.drawText ("ONLY MIDI IN PLAYS THE KIT" + middleDot() + "CHOOSE LOOP OR SONG TO PLAY ITS PARTS", track, juce::Justification::centred, true);
                return;
            }

            case engine::PlayMode::loop:
            {
                paint::engraved (g, "Loop" + middleDot() + "4 bars", label, juce::Justification::centredLeft, 9.5f);
                constexpr auto barGap = 10.0f, beatGap = 4.0f;
                const auto cell = (track.getWidth() - barGap * 3.0f - beatGap * 12.0f) / 16.0f;
                auto x = track.getX();
                for (int beat = 0; beat < 16; ++beat)
                {
                    const auto rect = juce::Rectangle<float> (x, track.getY(), cell, track.getHeight());
                    const auto on = beat == shown.beat;
                    const auto downbeat = beat % 4 == 0;
                    if (on)
                    {
                        g.setColour (lit.withAlpha (0.25f));
                        g.fillRoundedRectangle (rect.expanded (3.0f), 5.0f);
                        g.setGradientFill (juce::ColourGradient::vertical (lit.brighter (0.3f), rect.getY(), lit.darker (0.2f), rect.getBottom()));
                    }
                    else
                        g.setColour (lit.withAlpha (downbeat ? 0.16f : 0.08f));
                    g.fillRoundedRectangle (rect, 3.0f);

                    if (downbeat)
                    {
                        g.setFont (bodyFont (skinOf (*this), 9.0f, true));
                        g.setColour (on ? juce::Colour (0xff0b0d10) : lit.withAlpha (0.55f));
                        g.drawText (juce::String (beat / 4 + 1), rect.reduced (5.0f, 0.0f), juce::Justification::centredLeft, false);
                    }
                    x += cell + (beat % 4 == 3 ? barGap : beatGap);
                }
                return;
            }

            case engine::PlayMode::song:
                break;
        }

        const auto bars = totalBars (shown.sections);
        paint::engraved (g, "Song" + middleDot() + juce::String (bars) + " bars", label, juce::Justification::centredLeft, 9.5f);
        if (bars <= 0)
            return;

        for (int i = 0; i < (int) shown.sections.size(); ++i)
        {
            const auto& section = shown.sections[(std::size_t) i];
            const auto x0 = track.getX() + track.getWidth() * (float) (section.startBar - 1) / (float) bars;
            const auto x1 = track.getX() + track.getWidth() * (float) (section.startBar - 1 + section.bars) / (float) bars;
            const auto height = track.getHeight() * energyOf (section.name);
            const auto block = juce::Rectangle<float> (x0 + 1.0f, track.getBottom() - height, juce::jmax (1.0f, x1 - x0 - 2.0f), height);

            const auto on = shown.playing && i == shown.section;
            const auto past = shown.playing && shown.section >= 0 && i < shown.section;
            if (on)
            {
                g.setColour (lit.withAlpha (0.28f));
                g.fillRoundedRectangle (block.expanded (2.5f), 4.0f);
                g.setGradientFill (juce::ColourGradient::vertical (lit.brighter (0.25f), block.getY(), lit.darker (0.3f), block.getBottom()));
            }
            else
                g.setColour (past ? lit.withAlpha (0.30f) : lit.withAlpha (0.13f));
            g.fillRoundedRectangle (block, 2.5f);

            const auto name = section.name.toUpperCase();
            g.setFont (bodyFont (skinOf (*this), 9.5f, true, 0.08f));
            const auto nameWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
            if (nameWidth + 8.0f <= x1 - x0)
            {
                g.setColour (on ? juce::Colour (0xff0b0d10) : skinOf (*this).colours.text.withAlpha (past ? 0.75f : 0.6f));
                g.drawText (name, juce::Rectangle<float> (x0, track.getY(), x1 - x0, track.getHeight()).withTrimmedTop (track.getHeight() - juce::jmax (height, 12.0f)),
                    juce::Justification::centred, false);
            }
        }

        if (shown.playheadPixel >= 0)
        {
            const auto x = (float) shown.playheadPixel;
            g.setColour (lit.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (x - 2.5f, track.getY() - 3.0f, 5.0f, track.getHeight() + 6.0f));
            g.setColour (juce::Colours::white);
            g.fillRect (juce::Rectangle<float> (x - 0.75f, track.getY() - 3.0f, 1.5f, track.getHeight() + 6.0f));
        }
    }
}
