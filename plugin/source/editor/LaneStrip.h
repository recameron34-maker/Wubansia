#pragma once

// One lane: the kit's name for it with a hit LED, a 'recorded' or 'multisample' tag on phrase lanes,
// the level fader beside a peak meter, the mute pad and a drag chip for the lane's MIDI. A lane the kit
// doesn't have is dimmed.

#include "Controls.h"
#include "DragChip.h"
#include "EditorModel.h"

namespace wubansia::editor
{
    class LaneStrip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        LaneStrip (engine::Lane lane, Controller&);

        // muted: the lane's mute parameter, which the strip shows around its controls.
        void update (const EditorModel&, bool muted);
        void resized() override;
        void paint (juce::Graphics&) override;

        engine::Lane lane() const noexcept { return laneId; }
        const juce::String& phraseTag() const noexcept { return tag; } // "recorded", "multisample" or ""
        bool isDimmed() const noexcept { return dimmed; }
        float ledLevel() const noexcept { return led; }
        float meterDb() const noexcept { return peakDb; }

        // The looks (ADR 0032): what the second line under the name says ("B · REC", "CH 2 · REC"), and machine room's
        // 16 cells of the playing bar with the one playing (-1 stopped).
        juce::String secondLine() const;
        std::array<StepCell, 16> stepCells() const;
        int playingCell() const;

        Fader level; // <lane>_level
        PadButton mute; // <lane>_mute
        DragChip chip;

        static constexpr float meterFloorDb = -54.0f;

    private:
        void repaintIfChanged (bool changed, juce::Rectangle<int> area);

        const engine::Lane laneId;
        juce::String label, tag, valueText;
        bool dimmed = false, muted = false, midiTarget = false;
        float led = 0.0f, peakDb = meterFloorDb, holdDb = meterFloorDb;
        int holdTicks = 0;
        juce::Colour accent;
        juce::Rectangle<int> labelArea, ledArea, tagArea, meterArea, valueArea;
    };
}
