#include "AbPanel.h"

#include "LookAndFeel.h"
#include "ParameterIds.h"
#include "editor/RepaintLedger.h"

namespace wubansia::editor
{
    namespace
    {
        constexpr std::array<const char*, engine::kAbSwitches> rowNames { "Kick", "Bass", "Stab" };
        constexpr std::array<engine::Lane, engine::kAbSwitches> rowLanes { engine::Lane::kick, engine::Lane::bass, engine::Lane::chords };

        juce::String side (const char* letter, const juce::String& label) { return juce::String (letter) + middleDot() + label; }
    }

    AbDisplayWords abDisplay (const juce::String& a, const juce::String& b, const juce::String& laneLabel, const TextStyle& style, float room)
    {
        const auto fits = [&] (const juce::String& text)
        { return juce::GlyphArrangement::getStringWidth (style.font(), style.apply (text)) <= room; };
        if (fits (side ("A", a)) && fits (side ("B", b)))
            return { laneLabel + middleDot() + "A/B", side ("A", a), side ("B", b) };

        // The words both start with go up to the title.
        const auto wordsA = juce::StringArray::fromTokens (a, " ", "");
        const auto wordsB = juce::StringArray::fromTokens (b, " ", "");
        int shared = 0;
        while (shared < wordsA.size() - 1 && shared < wordsB.size() - 1 && wordsA[shared] == wordsB[shared])
            ++shared;
        if (shared == 0)
            return { laneLabel + middleDot() + "A/B", side ("A", a), side ("B", b) };
        juce::StringArray head, restA, restB;
        for (int i = 0; i < shared; ++i)
            head.add (wordsA[i]);
        for (int i = shared; i < wordsA.size(); ++i)
            restA.add (wordsA[i]);
        for (int i = shared; i < wordsB.size(); ++i)
            restB.add (wordsB[i]);
        return { head.joinIntoString (" ") + middleDot() + "A/B", side ("A", restA.joinIntoString (" ")), side ("B", restB.joinIntoString (" ")) };
    }

    AbPanel::AbPanel()
    {
        setComponentID ("ab");
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            switches[i] = std::make_unique<AbSwitch> (juce::String (rowNames[i]) + " A/B");
            switches[i]->setComponentID (ParameterIds::ab[i]);
            switches[i]->setExplicitFocusOrder ((int) i + 1);
            addAndMakeVisible (*switches[i]);
        }
    }

    void AbPanel::update (const EditorModel& model)
    {
        const auto& skin = skinOf (*this);
        std::array<bool, engine::kAbSwitches> now {};
        int n = 0;
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            now[i] = model.hasAb ((int) i);
            n += now[i] ? 1 : 0;
        }
        std::array<Display, engine::kAbSwitches> next;
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            auto& s = *switches[i];
            s.setLabels (model.kit.abA[i], model.kit.abB[i]);
            // A switch the kit lacks leaves the panel: hidden (out of Tab's and screen readers' reach) and disabled; its
            // parameter is still there for the host (ADR 0032).
            s.setEnabled (now[i]);
            s.setVisible (now[i]);
            if (! now[i])
                continue;
            if (n == 1)
            {
                const auto words = abDisplay (model.kit.abA[i], model.kit.abB[i], model.laneLabel (rowLanes[i]), skin.type.abSideOn, 129.0f);
                next[i] = { skin.type.abTitle.apply (words.title), skin.type.abSideOn.apply (words.a), skin.type.abSideOn.apply (words.b) };
                s.setDisplay (words.a, words.b, false);
            }
            else
            {
                next[i] = { skin.type.rowLabel.apply (rowNames[i]), skin.type.abRow.apply (side ("A", model.kit.abA[i])),
                    skin.type.abRow.apply (side ("B", model.kit.abB[i])) };
                s.setDisplay (side ("A", model.kit.abA[i]), side ("B", model.kit.abB[i]), true);
            }
        }
        if (now != available || n != count || next[0].title != shown[0].title || next[1].title != shown[1].title || next[2].title != shown[2].title)
        {
            available = now;
            count = n;
            shown = next;
            resized();
            repaintArea (*this);
        }
        shown = next;
    }

    AbPanel::Display AbPanel::display (int index) const { return shown[(std::size_t) juce::jlimit (0, 2, index)]; }

    void AbPanel::resized()
    {
        if (count == 1)
        {
            for (std::size_t i = 0; i < switches.size(); ++i)
                if (available[i])
                    switches[i]->setBounds (14, 44, 312, 44);
            return;
        }
        int y = 12;
        for (std::size_t i = 0; i < switches.size(); ++i)
            if (available[i])
            {
                switches[i]->setBounds (62, y, 264, 26);
                y += 30;
            }
    }

    std::vector<DrawnText> AbPanel::drawnText() const
    {
        const auto& skin = skinOf (*this);
        std::vector<DrawnText> out;
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            if (! available[i])
                continue;
            if (count == 1)
                out.push_back (fitText (skin.type.abTitle, shown[i].title, { 14.0f, 18.0f, 312.0f, 16.0f }));
            else
                out.push_back (fitText (skin.type.rowLabel, rowNames[i], { 14.0f, (float) switches[i]->getY(), 44.0f, 26.0f }));
        }
        return out;
    }

    void AbPanel::paint (juce::Graphics& g)
    {
        const auto& c = skinOf (*this).colours;
        for (const auto& t : drawnText())
            drawText (g, t, c.label);
    }
}
