#pragma once

// The display and the transport beside it (ADR 0032). The display's header line names the chord at the playhead (or
// the key), the Camelot code and the tempo on the left, and where playback is on the right (or, while the kit isn't
// ready, its status); below it each look shows something real: night bus the kit's step grid, mission HUD the lead's
// notes in orbit, deep glass the drums' and music's levels, machine room the plugin's output. The transport is the
// look's round button (Preview in a host, Play/Stop and the tempo in the Standalone app) with the sync state under it.

#include "Controls.h"
#include "EditorModel.h"
#include "editor/DrawnText.h"

#include <array>
#include <vector>

namespace wubansia::editor
{
    class Lcd : public juce::Component, public DrawnTextSource
    {
    public:
        Lcd();

        void update (const EditorModel&);
        void paint (juce::Graphics&) override;
        std::vector<DrawnText> drawnText() const override;

        // What the header line says, for tests.
        struct Lines
        {
            juce::String left; // "F#m9 · 11A · 134 BPM"
            juce::String right; // "DROP · BAR 97", "LOOP · BAR 2", "STOPPED", "PLAYER OFF · MIDI IN", or the kit's status
            bool ready = true; // the kit's samples are in: the right side says where playback is
            bool operator== (const Lines&) const = default;
        };
        const Lines& lines() const noexcept { return shown; }

        // The looks' displays, what each draws, for the tests.
        struct GridView // night bus: the kick, snare and bass lanes' two-bar page holding the playhead
        {
            int page = 0; // its first 16th
            int playing = -1; // the 16th playing; -1 stopped
            std::array<std::array<StepCell, 32>, 3> rows {};
            bool operator== (const GridView&) const = default;
        };
        GridView grid() const { return gridView; }

        struct Satellite // mission HUD: a lead note of the playing bar
        {
            enum class State { played, sounding, upcoming };
            int step = 0; // its 16th in the bar
            int ring = 0; // 0 inner
            int key = 0;
            State state = State::upcoming;
            bool operator== (const Satellite&) const = default;
        };
        std::vector<Satellite> satellites() const { return orbit; }
        juce::StringArray orbitLines() const { return orbitText; }

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

        // Where the view sits under the header line (local coordinates).
        static juce::Rectangle<int> viewArea() { return { 17, 32, 556, 76 }; }
        static juce::Rectangle<int> headerArea() { return { 17, 8, 556, 18 }; }

        // Machine room's feed: the newest output samples, oldest first (the editor drains Controller::readScope).
        void pushScope (const float* samples, int count);

    private:
        void updateGrid (const EditorModel&);
        void updateOrbit (const EditorModel&);
        void updateLevels (const EditorModel&);
        void updateScope (const EditorModel&);
        void paintGrid (juce::Graphics&, const Skin&) const;
        void paintOrbit (juce::Graphics&, const Skin&) const;
        void paintLevels (juce::Graphics&, const Skin&) const;
        void paintScope (juce::Graphics&, const Skin&) const;

        Lines shown;
        DisplayKind kind = DisplayKind::stepGrid;
        bool playerOff = false;
        juce::StringArray gridLabels;
        GridView gridView;
        std::vector<Satellite> orbit;
        juce::StringArray orbitText;
        juce::String orbitChord;
        LevelHistory levels;
        bool levelsMoving = false;
        ScopeTrace trace;
        std::vector<float> scopeHistory; // the newest 4,096 samples
        int scopeFill = 0; // how many of them are real
        float lowPassState = 0.0f;
        bool scopeArrived = false;
    };

    class TransportPanel : public juce::Component, public DrawnTextSource
    {
    public:
        TransportPanel (Controller&, bool standalone);

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;
        std::vector<DrawnText> drawnText() const override;

        bool isStandalone() const noexcept { return standalone; }
        // The sync state under the button, in the look's words ("synced · 134", "STANDBY"); empty in the Standalone app.
        const juce::String& line() const noexcept { return syncLine; }

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
        juce::String syncLine;
        bool lineLit = false;
    };

    // The words a look writes under the transport button (ADR 0032, section 3.6).
    juce::String syncWords (const Skin&, const EditorModel&);
}
