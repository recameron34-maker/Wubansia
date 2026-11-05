#pragma once

// The backlit display and the transport beside it. The display shows the transport (playing or
// stopped, and who drives it), the tempo and bar.beat in seven-segment digits, the kit, its key and
// Camelot code, and the mode or Song section. The transport is Play/Stop and the tempo in Standalone,
// and Preview in a host.

#include "Controls.h"
#include "EditorModel.h"

namespace wubansia::editor
{
    class Lcd : public juce::Component
    {
    public:
        Lcd();

        void update (const EditorModel&);
        void paint (juce::Graphics&) override;

        // What it shows (for tests): "134.0", "  3.2" or "---.-", "FUT01 ALLOY SIGNAL"...
        struct Lines
        {
            juce::String transport, sync, tempo, position, kitId, kitTitle, key, direction, mode, status;
            bool playing = false;
            juce::Colour accent;

            bool operator== (const Lines&) const = default;
        };
        const Lines& lines() const noexcept { return shown; }

    private:
        Lines shown;
    };

    class TransportPanel : public juce::Component
    {
    public:
        TransportPanel (Controller&, bool standalone);

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        bool isStandalone() const noexcept { return standalone; }

    private:
        void nudgeTempo (double delta);

        Controller& controller;
        const bool standalone;
        IconButton play { "Play", IconButton::Icon::play };
        IconButton slower { "Slower", IconButton::Icon::minus };
        IconButton faster { "Faster", IconButton::Icon::plus };
        IconButton kitTempo { "Kit tempo", IconButton::Icon::reset };
        LedButton preview { "Preview" };
        TransportState transport;
        int kitBpm = 0;
    };
}
