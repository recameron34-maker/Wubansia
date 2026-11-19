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

        // The looks' displays (ADR 0032), what each draws, for the tests.
        struct GridView // night bus: the kick, snare and bass lanes' two-bar page holding the playhead
        {
            int page = 0; // its first 16th
            int playing = -1; // the 16th playing; -1 stopped
            std::array<std::array<StepCell, 32>, 3> rows {};
        };
        GridView grid() const;

        struct Satellite // mission HUD: a lead note of the playing bar
        {
            enum class State { played, sounding, upcoming };
            int step = 0; // its 16th in the bar
            int ring = 0; // 0 inner
            int key = 0;
            State state = State::upcoming;
        };
        std::vector<Satellite> satellites() const;
        juce::StringArray orbitLines() const;

        struct LevelHistory // deep glass: the drums' and the music's level, in dB, oldest first, one per tick
        {
            std::vector<float> drums, music;
            std::vector<bool> kicks;
        };
        const LevelHistory& history() const noexcept { return levels; }

        struct ScopeTrace // machine room: 556 columns each of the output and its low band, in px from the centre line
        {
            std::vector<float> out, sub;
        };
        const ScopeTrace& scopeTrace() const noexcept { return trace; }

    private:
        Lines shown;
        LevelHistory levels;
        ScopeTrace trace;
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
