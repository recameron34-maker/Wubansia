#include "KitPanel.h"
#include "LookAndFeel.h"

namespace wubansia::editor
{
    juce::String contentBadge (const ContentStatus& content)
    {
        switch (content.state)
        {
            case ContentState::ready: return "READY";
            case ContentState::loading: return "LOADING " + juce::String (juce::jlimit (0, 100, content.percent)) + "%";
            case ContentState::missing: return "NO SAMPLES";
            case ContentState::lfsPointers: return "LFS POINTERS";
            case ContentState::mismatch: return "WRONG FILES";
            case ContentState::failed: return "LOAD ERROR";
        }
        return {};
    }

    KitPanel::Card::Card()
        : juce::Button ("kit")
    {
    }

    bool KitPanel::Card::show (const KitCard& card, const ContentStatus& content, bool isSelected)
    {
        const auto changed = card.id != data.id || card.title != data.title || card.direction != data.direction
                             || card.key != data.key || card.camelot != data.camelot || card.bpm != data.bpm
                             || content.state != status.state || content.percent != status.percent || isSelected != selected;
        data = card;
        status = content;
        selected = isSelected;
        if (changed)
        {
            setComponentID ("kit_" + card.id);
            setButtonText (card.id);
            setTitle (card.id + " " + card.title);
            setTooltip (card.title + " (" + card.id + "): " + card.direction + ", " + juce::String (card.bpm) + " BPM, "
                        + card.key + " (" + card.camelot + "). Samples: " + statusText().toLowerCase() + ".");
            repaint();
        }
        return changed;
    }

    juce::String KitPanel::Card::statusText() const
    {
        return contentBadge (status);
    }

    namespace
    {
        // The card's three text lines, inside its stripe and padding.
        juce::Rectangle<float> linesOf (juce::Rectangle<float> bounds)
        {
            return bounds.reduced (1.5f).withTrimmedLeft (19.0f).withTrimmedRight (10.0f).reduced (0.0f, 6.0f);
        }

        // The largest font from `from` down to `floor` points, a quarter point at a time, that fits the text into the width.
        juce::Font fitted (const juce::String& text, float width, float from = 11.0f, float floor = 8.0f, bool bold = false)
        {
            auto height = from;
            auto font = uiFont (height, bold);
            while (height > floor && juce::GlyphArrangement::getStringWidth (font, text) > width)
            {
                height -= 0.25f;
                font = uiFont (height, bold);
            }
            return font;
        }

        juce::Font idFont() { return uiFont (10.5f, true, 0.1f); }
    }

    juce::Rectangle<float> KitPanel::Card::titleArea() const
    {
        const auto lines = linesOf (getLocalBounds().toFloat());
        const auto idWidth = juce::GlyphArrangement::getStringWidth (idFont(), data.id) + 7.0f;
        return lines.withHeight (lines.getHeight() / 3.0f).withTrimmedLeft (idWidth);
    }

    juce::Font KitPanel::Card::titleFont() const
    {
        // 14 points where the title fits; 146 px cards fit FUT03's and FUT04's from 12.25 (Liberation Sans) or 10.5 (DejaVu
        // Sans), so the floor is 10.
        return fitted (data.title, titleArea().getWidth(), 14.0f, 10.0f, true);
    }

    juce::Rectangle<float> KitPanel::Card::directionArea() const
    {
        const auto lines = linesOf (getLocalBounds().toFloat());
        const auto lineHeight = lines.getHeight() / 3.0f;
        return lines.withTrimmedTop (lineHeight).withHeight (lineHeight);
    }

    juce::Font KitPanel::Card::directionFont() const
    {
        return fitted (data.direction, directionArea().getWidth());
    }

    void KitPanel::Card::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = accentForKit (data.id);
        auto area = getLocalBounds().toFloat().reduced (1.5f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);

        // The card: a dark glass tile, lit in its kit's colour when selected.
        if (selected)
        {
            g.setColour (accent.withAlpha (0.18f));
            g.fillRoundedRectangle (area.expanded (2.5f), 9.0f);
        }
        g.setGradientFill (juce::ColourGradient::vertical (selected ? accent.withAlpha (0.20f).overlaidWith (juce::Colour (0xff1c1f25)) : juce::Colour (isMouseOver ? 0xff22262c : 0xff1b1e23),
            area.getY(), juce::Colour (0xff121417), area.getBottom()));
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (selected ? accent.withAlpha (0.9f) : juce::Colour (isMouseOver ? 0xff3b4049 : 0xff2b2f36));
        g.drawRoundedRectangle (area, 7.0f, selected ? 1.4f : 1.0f);

        // The accent stripe.
        const auto stripe = area.withWidth (3.5f).reduced (0.0f, 9.0f).translated (7.0f, 0.0f);
        g.setColour (selected ? accent : accent.withAlpha (0.45f));
        g.fillRoundedRectangle (stripe, 1.75f);
        if (selected)
        {
            g.setColour (accent.withAlpha (0.3f));
            g.fillRoundedRectangle (stripe.expanded (2.0f), 3.0f);
        }

        auto lines = linesOf (getLocalBounds().toFloat()).translated (0.0f, isButtonDown ? 0.6f : 0.0f);
        const auto lineHeight = lines.getHeight() / 3.0f;
        const auto ready = status.state == ContentState::ready;
        const auto loading = status.state == ContentState::loading;

        // The ID and title.
        auto line1 = lines.removeFromTop (lineHeight);
        const auto title = titleArea().translated (0.0f, isButtonDown ? 0.6f : 0.0f);
        g.setFont (idFont());
        g.setColour (selected ? accent : accent.withAlpha (0.75f));
        g.drawText (data.id, line1.withRight (title.getX()), juce::Justification::centredLeft, false);
        g.setFont (titleFont());
        g.setColour (selected ? colours::text : colours::text.withAlpha (0.82f));
        g.drawText (data.title, title, juce::Justification::centredLeft, true);

        // The direction, or what stands in the way of playing.
        const auto line2 = lines.removeFromTop (lineHeight);
        if (ready)
        {
            g.setFont (directionFont());
            g.setColour (colours::textDim.withAlpha (selected ? 1.0f : 0.8f));
            g.drawText (data.direction, line2, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setFont (uiFont (10.0f, true, 0.1f));
            g.setColour (loading ? colours::busy : colours::warning.brighter (0.25f));
            g.drawText (statusText(), line2, juce::Justification::centredLeft, true);
        }

        // Tempo, key and Camelot code, and a status light: the kit's colour when its samples are ready.
        draw::led (g, { lines.getRight() - 3.0f, lines.getCentreY() }, 3.0f, ready ? accent : (loading ? colours::busy : colours::warning),
            ready ? (selected ? 1.0f : 0.55f) : 1.0f);
        g.setFont (uiFont (11.0f, true));
        g.setColour (colours::text.withAlpha (selected ? 0.9f : 0.7f));
        g.drawText (juce::String (data.bpm) + " BPM" + middleDot() + shortKey (data.key) + middleDot() + data.camelot,
            lines.withTrimmedRight (12.0f), juce::Justification::centredLeft, true);
    }

    KitPanel::KitPanel (Controller& c)
        : controller (c)
    {
        setComponentID ("kits");
    }

    void KitPanel::update (const EditorModel& model)
    {
        juce::StringArray newIds;
        for (const auto& card : model.kits)
            newIds.add (card.id);

        if (newIds != ids)
        {
            ids = newIds;
            cards.clear();
            for (int i = 0; i < ids.size(); ++i)
            {
                auto* card = cards.add (new Card());
                card->onClick = [this, card] { controller.selectKit (card->card().id); };
                addAndMakeVisible (card);
            }
            resized();
        }

        for (int i = 0; i < cards.size(); ++i)
        {
            const auto& card = model.kits[(std::size_t) i];
            const auto selected = i == model.currentIndex;
            cards[i]->show (card, selected ? model.content : card.content, selected);
        }
    }

    void KitPanel::resized()
    {
        if (cards.isEmpty())
            return;

        constexpr auto gap = 8;
        const auto count = cards.size();
        const auto width = (getWidth() - gap * (count - 1)) / count;
        auto area = getLocalBounds();
        for (auto* card : cards)
        {
            card->setBounds (area.removeFromLeft (width));
            area.removeFromLeft (gap);
        }
    }
}
