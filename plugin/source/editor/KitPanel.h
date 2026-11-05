#pragma once

// The kit picker: one card per embedded kit, in the Controller's order, each in its kit's own accent
// with its tempo, key, Camelot code and whether its samples are ready. Clicking a card selects it.

#include "EditorModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor
{
    class KitPanel : public juce::Component
    {
    public:
        class Card : public juce::Button
        {
        public:
            Card();

            // Returns true when anything it shows changed.
            bool show (const KitCard& card, const ContentStatus& content, bool selected);

            const KitCard& card() const noexcept { return data; }
            bool isSelected() const noexcept { return selected; }
            juce::String statusText() const;

            // The direction line, which shrinks its font to read in full rather than be cut.
            juce::Rectangle<float> directionArea() const;
            juce::Font directionFont() const;

            void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

        private:
            KitCard data;
            ContentStatus status;
            bool selected = false;
        };

        explicit KitPanel (Controller&);

        void update (const EditorModel&);
        void resized() override;

        int numCards() const noexcept { return cards.size(); }
        Card* card (int index) const noexcept { return cards[index]; }

    private:
        Controller& controller;
        juce::OwnedArray<Card> cards;
        juce::StringArray ids;
    };

    // The status a card or the content panel names: "READY", "LOADING 42 %", "NO SAMPLES"...
    juce::String contentBadge (const ContentStatus&);
}
