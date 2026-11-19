#pragma once

// The v0.1 editor (P10): a native JUCE editor, 1120 x 700 at 100 % and zoomable, drawn as a dark
// lit-hardware groovebox with one accent per kit. It knows the plugin only through the Controller
// (Controller.h) and the parameter IDs (ParameterIds.h): its controls bind by polling on one 30 Hz
// timer, and no parameter listener is ever registered (editor/PollingAttachment.h).

#include "Controller.h"
#include "editor/AbPanel.h"
#include "editor/ContentPanel.h"
#include "editor/DragChip.h"
#include "editor/EditorModel.h"
#include "editor/FxPanel.h"
#include "editor/KitPanel.h"
#include "editor/LaneStrip.h"
#include "editor/Lcd.h"
#include "editor/LookAndFeel.h"
#include "editor/ModePanel.h"
#include "editor/PhrasesPanel.h"
#include "editor/PollingAttachment.h"
#include "editor/SamplesPanel.h"
#include "editor/SectionStrip.h"
#include "editor/skin/Skin.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>
#include <vector>

namespace wubansia
{
    class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        static constexpr int baseWidth = 1120;
        static constexpr int baseHeight = 700;
        static constexpr int timerHz = 30;

        PluginEditor (juce::AudioProcessor&, Controller&);

        ~PluginEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        // The timer's work, 30 times a second: poll every parameter, read the Controller once, update
        // every panel. Tests call it directly instead of waiting for the timer.
        void tick();

        // 1.0 is 100 % (1120 x 700); the zoom menu offers 100-200 %. New editors open at the last zoom chosen.
        void setZoom (float zoom);
        float getZoom() const noexcept { return zoom; }
        static constexpr std::array<int, 5> zoomPercents { 100, 125, 150, 175, 200 };

        const editor::EditorModel& model() const noexcept { return state; }

        // The looks (ADR 0032): the one the editor wears now, how many times its backdrop has been drawn, and, for
        // tests, a control to ring as if it had the keyboard focus (nullptr: the real focus).
        const editor::Skin& skin() const noexcept;
        int backdropRenders() const noexcept;
        void setFocusOverrideForTesting (juce::Component*);

        // For tests: whether the editor runs in Logic Pro (juce::PluginHostType, asked once when it opens).
        void setLogicHostForTesting (bool isLogic) noexcept { logicHost = isLogic; }
        bool isContentPanelShown() const noexcept { return contentPanel.isVisible(); }
        bool isSamplesPanelShown() const noexcept { return samplesPanel.isVisible(); }
        bool isPhrasesPanelShown() const noexcept { return phrasesPanel.isVisible(); }

    private:
        class Backdrop : public juce::Component
        {
        public:
            void paint (juce::Graphics&) override;

            juce::Rectangle<float> logo;
            std::vector<std::pair<juce::Rectangle<float>, juce::String>> plates;
        };

        void timerCallback() override { tick(); }
        void bindParameters (juce::AudioProcessor&);
        void showParameterMenu (juce::RangedAudioParameter&, juce::Component& control);
        void layOut();
        void refreshModel();
        void showContentPanel (bool show);
        // A drawer, the Samples drawer (ADR 0024) or the Phrases drawer (ADR 0031): never both, never with the install panel.
        void showSamplesPanel (bool show);
        void showPhrasesPanel (bool show);
        void coverFace (bool covered); // hides the lanes, A/B and FX under a drawer, or shows them again

        Controller& controller;
        const bool standalone;
        bool logicHost; // asked once: PluginHostType reads the host's path
        float zoom = 1.0f;

        editor::LookAndFeel lookAndFeel; // outlives every component below
        juce::Component content; // everything, at 1120 x 700, scaled by the zoom
        Backdrop backdrop;
        editor::KitPanel kitPanel;
        editor::LedButton phrasesButton { "Phrases" };
        editor::LedButton samplesButton { "Samples" };
        juce::ComboBox zoomBox { "Zoom" };
        editor::Lcd lcd;
        editor::TransportPanel transport;
        editor::ModePanel modePanel;
        editor::SectionStrip sectionStrip;
        std::array<std::unique_ptr<editor::LaneStrip>, engine::kLanes> lanes;
        editor::AbPanel abPanel;
        editor::FxPanel fxPanel;
        editor::DragStrip dragStrip;
        editor::ContentPanel contentPanel;
        editor::SamplesPanel samplesPanel;
        editor::PhrasesPanel phrasesPanel;
        juce::TooltipWindow tooltips { this, 650 };

        // After the controls they bind, so they go first.
        std::vector<std::unique_ptr<editor::PollingAttachment>> attachments;
        juce::RangedAudioParameter* playModeParameter = nullptr;
        juce::RangedAudioParameter* voxParameter = nullptr;
        std::array<juce::RangedAudioParameter*, engine::kLanes> muteParameters {};

        editor::EditorModel state;
        juce::String contentDismissedFor; // "<kit>/<state>" the person closed the content panel for
        bool contentOpenedByHand = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
    };
}
