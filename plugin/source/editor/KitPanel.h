#pragma once

// The kit picker: one pill per embedded kit, in the Controller's order (ADR 0032). The selected one reads
// "<ID> <Title>", the others their ID; each has a status dot for its samples. Clicking a pill selects it.

#include "EditorModel.h"
#include "editor/DrawnText.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor
{
    class KitPanel : public juce::Component
    {
    public:
        class Card : public juce::Button, public DrawnTextSource
        {
        public:
            Card();

            // Returns true when anything it shows changed.
            bool show (const KitCard& card, const ContentStatus& content, bool selected);

            const KitCard& card() const noexcept { return data; }
            bool isSelected() const noexcept { return selected; }
            juce::String statusText() const;

            // The title, right of the ID, and the direction line under it: each shrinks its font to read in full rather
            // than be cut.
            // The pill's width for its words in the look (ADR 0032): the selected one shows "<ID> <Title>".
            int fitWidth() const;
            // The status dot: ready (the accent), loading (busy, blinking) or a problem (warning).
            juce::Colour dotColour (const Skin&) const;

            std::vector<DrawnText> drawnText() const override;

            void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

        private:
            friend class KitPanel;
            KitCard data;
            ContentStatus status;
            bool selected = false;
            bool blinkOn = true; // loading: the dot blinks at 2 Hz
        };

        explicit KitPanel (Controller&);

        void update (const EditorModel&);
        void resized() override;
        void lookAndFeelChanged() override { resized(); }

        int numCards() const noexcept { return cards.size(); }
        // The pills' total width with their gaps, as resized () lays them out.
        int fitWidth() const;
        Card* card (int index) const noexcept { return cards[index]; }

    private:
        Controller& controller;
        juce::OwnedArray<Card> cards;
        int ticks = 0;
        juce::StringArray ids;
    };

    // The status a card or the content panel names: "READY", "LOADING 42 %", "NO SAMPLES"...
    juce::String contentBadge (const ContentStatus&);
}
