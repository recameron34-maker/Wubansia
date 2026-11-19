#pragma once

// The Phrases drawer (ADR 0031): the current kit's 64 library phrases, its kept and More phrases, and its own phrase
// files and lead line, to hear with the groove and drag into a DAW as MIDI. It sits where the Samples drawer does, at
// (20, 246), 1080 x 396 in the editor's face, over the lanes, A/B and FX (the editor hides them while it shows), and
// never shows with the Samples drawer. It knows the plugin only through the Controller: phrases () for the list, asked
// only when phrasesGeneration () moves on; auditionPhrase () and stopPhraseAudition () for a click, Return and Stop;
// morePhrases () and setPhraseKept () for More and the star; writeDragFile () for a drag. It registers no listener:
// the editor's 30 Hz tick hands it the model.
//
//     header   PHRASES, the kit line, More, Stop, Close                                       44 px
//     filters  All, Hooks, Call & answer ... Bounce, From the kit, Kept, each with its count   32 px
//     grid     four 256 x 72 tiles a row, 8 px apart, scrolling                                280 px
//     footer   the status line                                         For my synths: on       40 px
//
// It draws through editor::LookAndFeel and draw::, as the other panels do.

#include "Controls.h"
#include "EditorModel.h"
#include "LookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace wubansia::editor
{
    class PhrasesPanel;

    // A filter in the filter row: its name, then how many phrases it holds, lit while its phrases show. Screen readers
    // hear "Hooks, 12 phrases, shown".
    class PhraseFilter : public juce::Button
    {
    public:
        explicit PhraseFilter (const juce::String& name);

        void show (int numPhrases, bool isShown);
        int count() const noexcept { return phrases; }
        bool isShown() const noexcept { return shown; }
        juce::String label() const; // "From the kit 6"
        int fitWidth() const; // its label in full, with its padding
        void setPadding (int pixels); // each side of the label; the drawer narrows it when the row is full

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        void updateTitle();

        int phrases = 0;
        bool shown = false;
        int padding = 14;
    };

    // A tile's star: lit while the phrase is kept. Tab reaches the selected tile's only.
    class PhraseStar : public juce::Button
    {
    public:
        PhraseStar();
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    };

    // One phrase: its name, then "Hook · 8 bars · steady" (the kit's own "From the kit · 2 bars"), its roll, a star,
    // and a New badge on More's. A click plays it with the groove (on the one playing or waiting, it stops); a 4-pixel
    // drag writes its MIDI file and hands it to the OS, inside the mouse event. Screen readers hear a button, "Night Bus,
    // hook, 8 bars, steady", then ", playing", ", starts at bar 9" or ", kept"; its press plays or stops it.
    class PhraseTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int width = 256, height = 72;

        PhraseTile (PhrasesPanel& owner, int index);

        void show (const PhraseInfo& info, bool isSelected, bool isFailed, bool isPlaying, bool isWaiting, int startsAtBar);
        const PhraseInfo& phrase() const noexcept { return shownPhrase; }
        int index() const noexcept { return tile; }
        bool isSelected() const noexcept { return selected; }
        bool hasFailed() const noexcept { return failed; }
        bool isPlaying() const noexcept { return playing; }
        bool isWaiting() const noexcept { return waiting; }
        juce::String detailText() const; // "Hook · 8 bars · steady, mid"; the kit's own "From the kit · 2 bars"
        juce::String badgeText() const; // "New" on More's phrases until played or dragged

        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        void updateTitle();

        PhrasesPanel& owner;
        const int tile;
        PhraseInfo shownPhrase;
        PhraseStar star;
        int startBar = 0;
        bool selected = false, failed = false, hover = false, pressed = false, dragging = false;
        bool playing = false, waiting = false;
    };

    // The tiles, in the drawer's scrolling grid: one stop for Tab. It takes the arrows (between tiles, each naming the tile
    // it lands on to a screen reader) and Return (plays or stops the selected tile); Escape goes on to the drawer, and
    // Space, letters and shortcuts to the host.
    class PhraseGrid : public juce::Component
    {
    public:
        explicit PhraseGrid (PhrasesPanel& owner);

        bool keyPressed (const juce::KeyPress&) override;
        void focusGained (FocusChangeType) override;
        void focusLost (FocusChangeType) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        PhrasesPanel& owner;
    };

    class PhrasesPanel : public juce::Component
    {
    public:
        static constexpr int columns = 4, gap = 8;

        explicit PhrasesPanel (Controller&);

        // The editor shows the drawer: All, the current kit's phrases, the first one selected, and "Phrases" said to a
        // screen reader.
        void open (const EditorModel&);
        // The editor's 30 Hz tick: the kit line, the audition, the mutes, For my synths, and the list again when
        // phrasesGeneration moved on. Does nothing while the drawer is hidden.
        void update (const EditorModel&);
        // Gives the grid the keyboard, if it shows and has tiles: the editor calls it once the drawer is visible.
        void focusGrid();

        void resized() override;
        void paint (juce::Graphics&) override;
        bool keyPressed (const juce::KeyPress&) override; // Escape closes the drawer (onClose)

        // What it shows, for tests.
        int numTiles() const noexcept { return tiles.size(); }
        int selectedTile() const noexcept { return selected; }
        juce::String kitLine() const; // "FUT01 Alloy Signal · over F#m9 – Bm9 · 134 BPM · F#m 11A"
        juce::String shownFilter() const { return filter; } // "all", a kind id, "kit" or "kept"
        juce::String statusText() const; // the footer's status line
        juce::String synthsText() const; // "For my synths: on", the drag strip's switch

        // How the drawer hands a file to the OS and speaks. Tests replace them; by default startDrag is startExternalDrag
        // (ExternalDrag.h) and announce announceToScreenReader, as for the MIDI chips and the Samples drawer.
        std::function<bool (const juce::File&, juce::Component&)> startDrag;
        std::function<void (const juce::String&)> announce;
        std::function<void()> onClose; // Close and Escape

    private:
        friend class PhraseTile;
        friend class PhraseGrid;

        // What the person does.
        void select (int tile);
        bool gridKey (const juce::KeyPress&);
        bool dragOut (int tile, juce::Component& source);
        void play (int tile); // plays the tile, or stops it when it plays or waits
        void keep (int tile);
        void showFilter (const juce::String& key);
        void more();
        juce::File write (int tile); // the tile's file in the drag folder, or juce::File () and the status line says why

        // What it shows.
        const PhraseInfo* phraseAt (int tile) const;
        const PhraseInfo* auditioned() const; // the phrase the audition is of, if it plays or waits
        void take (const EditorModel&);
        void rebuildTiles();
        void refresh();
        void layOutFilters();
        void layOutGrid();

        Controller& controller;
        std::vector<PhraseInfo> list; // phrases () as last asked
        std::vector<int> shownRows; // the list's rows the filter shows, in order: one tile each
        juce::String filter = "all";
        std::uint64_t listedGeneration = 0;
        bool listed = false; // phrases () asked at least once
        juce::String listedKit;
        KitCard kit;
        PhraseAuditionState audition;
        std::uint32_t mutedLanes = 0;
        ContentState content = ContentState::ready;
        bool standalone = false, logicHost = false;
        bool forSynths = true;
        int selected = -1;
        int failedTile = -1; // the tile whose last write failed, while the status line says so
        juce::String failure; // the status line after a failed write; empty, the other lines
        // More: the kind asked for ("" all), the ids listed when it was pressed, and the line once its phrases came.
        bool moreWaiting = false;
        juce::String moreKind;
        std::set<juce::String> moreBefore;
        juce::String moreLine;
        juce::String announced; // the status line last said to a screen reader
        bool handingOver = false; // startDrag is handing a drag to the OS: the tiles stay as they are
        juce::String painted; // what paint () last drew of the text, to repaint only on a change

        juce::OwnedArray<PhraseFilter> filters; // in kFilters' order
        juce::TextButton moreButton { "More" };
        IconButton stopButton { "Stop", IconButton::Icon::stop };
        IconButton closeButton { "Close", IconButton::Icon::close };
        PhraseGrid grid { *this };
        juce::Viewport viewport;
        juce::OwnedArray<PhraseTile> tiles;

        // Laid out by resized ().
        juce::Rectangle<int> headerArea, filterArea, gridArea, footerArea, titleArea, statusArea, synthsArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhrasesPanel)
    };
}
