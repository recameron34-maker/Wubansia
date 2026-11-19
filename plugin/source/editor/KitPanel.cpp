#include "KitPanel.h"
#include "LookAndFeel.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"

#include <cmath>

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
            repaintArea (*this);
        }
        return changed;
    }

    juce::String KitPanel::Card::statusText() const
    {
        return contentBadge (status);
    }

    namespace
    {
        constexpr float dotSize = 8.0f, dotGap = 8.0f;

        juce::String pillWords (const KitCard& card, bool selected) { return selected ? card.id + " " + card.title : card.id; }

        const TextStyle& pillStyle (const Skin& skin, bool selected) { return selected ? skin.type.pillSelected : skin.type.pill; }

        float pad (bool selected) { return selected ? 16.0f : 14.0f; }
    }

    int KitPanel::Card::fitWidth() const
    {
        const auto& skin = skinOf (*this);
        const auto& style = pillStyle (skin, selected);
        const auto words = style.apply (pillWords (data, selected));
        return (int) std::ceil (pad (selected) * 2.0f + dotSize + dotGap + juce::GlyphArrangement::getStringWidth (style.font(), words) + 2.0f);
    }

    juce::Colour KitPanel::Card::dotColour (const Skin& skin) const
    {
        switch (status.state)
        {
            case ContentState::ready: return selected ? skin.colours.accent : skin.colours.faint;
            case ContentState::loading: return skin.colours.busy;
            case ContentState::missing:
            case ContentState::lfsPointers:
            case ContentState::mismatch:
            case ContentState::failed: break;
        }
        return skin.colours.warning;
    }

    std::vector<DrawnText> KitPanel::Card::drawnText() const
    {
        const auto& skin = skinOf (*this);
        const auto area = getLocalBounds().toFloat().withTrimmedLeft (pad (selected) + dotSize + dotGap).withTrimmedRight (pad (selected) - 2.0f);
        return { fitText (pillStyle (skin, selected), pillWords (data, selected), area) };
    }

    void KitPanel::Card::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto area = getLocalBounds().toFloat().reduced (0.5f).translated (0.0f, isButtonDown ? 0.5f : 0.0f);
        const auto fill = selected ? over (c.pillOn, c.face) : over (c.pillOff, c.face).brighter (isMouseOver ? 0.06f : 0.0f);
        paint::box (g, area, selected ? skin.shape.pillSelected : skin.shape.pill, fill, selected ? c.pillOnEdge : c.pillOffEdge);
        const auto lit = status.state != ContentState::loading || blinkOn;
        paint::dot (g, { area.getX() + pad (selected) + dotSize * 0.5f, area.getCentreY() }, dotSize * 0.5f, dotColour (skin), lit);
        for (const auto& t : drawnText())
            drawText (g, t, selected ? c.pillOnText : c.pillOffText);
    }

    int KitPanel::fitWidth() const
    {
        int width = 0;
        for (auto* card : cards)
            width += card->fitWidth() + (width > 0 ? 8 : 0);
        return width;
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
                card->setExplicitFocusOrder (i + 1);
                card->onClick = [this, card] { controller.selectKit (card->card().id); };
                addAndMakeVisible (card);
            }
            resized();
        }

        auto relayout = false;
        const auto blink = (++ticks / 8) % 2 == 0; // 2 Hz at the editor's 30 Hz
        for (int i = 0; i < cards.size(); ++i)
        {
            const auto& card = model.kits[(std::size_t) i];
            const auto selected = i == model.currentIndex;
            const auto wasSelected = cards[i]->isSelected();
            cards[i]->show (card, selected ? model.content : card.content, selected);
            relayout = relayout || wasSelected != selected;
            const auto loading = cards[i]->status.state == ContentState::loading;
            if (loading && cards[i]->blinkOn != blink)
            {
                cards[i]->blinkOn = blink;
                repaintArea (*cards[i]);
            }
            else if (! loading)
                cards[i]->blinkOn = true;
        }
        if (relayout)
            resized();
    }

    void KitPanel::resized()
    {
        // Each pill as wide as its words in the look (ADR 0032), 8 apart, in the catalog's order.
        auto area = getLocalBounds();
        for (auto* card : cards)
        {
            card->setBounds (area.removeFromLeft (std::min (card->fitWidth(), area.getWidth())));
            area.removeFromLeft (8);
        }
    }
}
