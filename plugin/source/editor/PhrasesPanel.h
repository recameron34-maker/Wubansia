#pragma once

// The Phrases drawer (ADR 0031): the current kit's own phrases and its lead line, to drag into a DAW as MIDI. It sits
// where the Samples drawer does, at (20, 246), 1080 x 396 in the editor's face, over the lanes, A/B and FX (the editor
// hides them while it shows), and never shows with the Samples drawer. It knows the plugin only through the
// Controller: phrases () for the list, asked only when phrasesGeneration () moves on, and writeDragFile () for the file
// a drag or a click hands over. It registers no listener: the editor's 30 Hz tick hands it the model.
//
//     header   PHRASES, the kit line, Close                                        44 px
//     filters  From the kit <n>                                                    32 px
//     grid     four 256 x 72 tiles a row, 8 px apart, scrolling                    280 px
//     footer   the status line                                 For my synths: on   40 px
//
// It draws through editor::LookAndFeel and draw::, as the other panels do. Part 1 lists the kit's own phrases; step
// `phrases` adds the library, its audition, More and Keep.

#include "Controls.h"
#include "EditorModel.h"
#include "LookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace wubansia::editor
{
    class PhrasesPanel;

    // A filter in the filter row: its name, then how many phrases it holds, lit while its phrases show. Screen readers
    // hear "From the kit, 6 phrases, shown".
    class PhraseFilter : public juce::Button
    {
    public:
        explicit PhraseFilter (const juce::String& name);

        void show (int numPhrases, bool isShown);
        int count() const noexcept { return phrases; }
        bool isShown() const noexcept { return shown; }
        juce::String label() const; // "From the kit 6"
        int fitWidth() const; // its label in full, with its padding

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        void updateTitle();

        int phrases = 0;
        bool shown = false;
    };

    // One phrase: its name, then "From the kit · 2 bars". A press selects it; a 4-pixel drag writes its MIDI file and
    // hands it to the OS, inside the mouse event; a click without a drag shows the file. Screen readers hear a button,
    // "E Piano Stab Rhythm, from the kit, 2 bars", whose press shows the file.
    class PhraseTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int width = 256, height = 72;

        PhraseTile (PhrasesPanel& owner, int index);

        void show (const PhraseInfo& info, bool isSelected, bool isFailed);
        const PhraseInfo& phrase() const noexcept { return shownPhrase; }
        int index() const noexcept { return tile; }
        bool isSelected() const noexcept { return selected; }
        bool hasFailed() const noexcept { return failed; }
        juce::String detailText() const; // "From the kit · 2 bars"

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        PhrasesPanel& owner;
        const int tile;
        PhraseInfo shownPhrase;
        bool selected = false, failed = false, hover = false, pressed = false, dragging = false;
    };

    // The tiles, in the drawer's scrolling grid: one stop for Tab. It takes the arrows (between tiles) and Return (shows
    // the selected tile's file); Escape goes on to the drawer, and Space, letters and shortcuts to the host.
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

        // The editor shows the drawer: it lists the current kit's phrases, the first one selected, and says "Phrases" to
        // a screen reader.
        void open (const EditorModel&);

        // The editor's 30 Hz tick: the kit line, For my synths, and the list again when phrasesGeneration moved on (a kit
        // change). Does nothing while the drawer is hidden.
        void update (const EditorModel&);

        // Gives the grid the keyboard, if it shows and has tiles: the editor calls it once the drawer is visible.
        void focusGrid();

        void resized() override;
        void paint (juce::Graphics&) override;
        bool keyPressed (const juce::KeyPress&) override; // Escape closes the drawer (onClose)

        // What it shows, for tests.
        int numTiles() const noexcept { return tiles.size(); }
        int selectedTile() const noexcept { return selected; }
        juce::String kitLine() const; // "FUT01 Alloy Signal · 134 BPM · F#m 11A"
        juce::String statusText() const; // the footer's status line
        juce::String synthsText() const; // "For my synths: on", the drag strip's switch

        // How the drawer hands a file to the OS and speaks. Tests replace them; by default startDrag is startExternalDrag
        // (ExternalDrag.h), revealFile dragout::revealToUser and announce announceToScreenReader, as for the MIDI chips
        // and the Samples drawer.
        std::function<bool (const juce::File&, juce::Component&)> startDrag;
        std::function<void (const juce::File&)> revealFile;
        std::function<void (const juce::String&)> announce;

        std::function<void()> onClose; // Close and Escape

    private:
        friend class PhraseTile;
        friend class PhraseGrid;

        // What the person does.
        void select (int tile);
        bool gridKey (const juce::KeyPress&);
        bool dragOut (int tile, juce::Component& source);
        void reveal (int tile);
        juce::File write (int tile); // the tile's file in the drag folder, or juce::File () and the status line says why

        // What it shows.
        void take (const EditorModel&);
        void rebuildTiles();
        void refresh();
        void layOutGrid();

        Controller& controller;
        std::vector<PhraseInfo> list; // phrases () as last asked
        std::uint64_t listedGeneration = 0;
        bool listed = false; // phrases () asked at least once
        KitCard kit;
        bool forSynths = true;
        int selected = -1;
        int failedTile = -1; // the tile whose last write failed, while the status line says so
        juce::String failure; // the status line after a failed write; empty, the idle line
        bool handingOver = false; // startDrag is handing a drag to the OS: the tiles stay as they are
        juce::String painted; // what paint () last drew of the text, to repaint only on a change

        PhraseFilter fromKit { "From the kit" };
        IconButton closeButton { "Close", IconButton::Icon::close };
        PhraseGrid grid { *this };
        juce::Viewport viewport;
        juce::OwnedArray<PhraseTile> tiles;

        // Laid out by resized ().
        juce::Rectangle<int> headerArea, filterArea, gridArea, footerArea, titleArea, statusArea, synthsArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhrasesPanel)
    };
}
