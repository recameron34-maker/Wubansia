#pragma once

// The Samples drawer (ADR 0024): every kit's sounds by category, to hear and to drag into a DAW as the
// kit's own WAV files. It covers the lanes, A/B and FX, at (20, 246), 1080 x 396 in the editor's face (the
// editor hides them while it shows, so Tab and screen readers stay out of them),
// and knows the plugin only through the Controller: sounds () for each kit's list (asked once per kit),
// audition () and stopAudition () to hear them, exportSound () for the file a drag or Show in Finder
// hands over, and samplesFolder () and setSamplesFolder () for where those copies go. Nothing it does
// changes the project: what it shows and the files picked live while the editor is open.
//
//     header  SAMPLES, a chip per kit (browse it; the playing kit plays on), the browsed kit, Stop, Close
//     rail    the seven categories with their counts
//     list    a row per sound: play or stop, name, length, END, how many files, a grip to drag it by
//     detail  the selected sound: its File box, length and format, a hint, the drag tile, Play, Show in Finder
//     footer  the samples folder and Change..., a status line, Content... (the install panel)
//
// It registers no listener: buttons call lambdas, and the editor's 30 Hz tick hands it the model.

#include "ContentPanel.h"
#include "Controls.h"
#include "EditorModel.h"
#include "LookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace wubansia::editor
{
    class SamplesPanel;

    // The name exportSound () gives a file: "Rave Stab v2 - FUT01 134 F#m 11A.wav", "Dub Chord Cm9 - FUT03 128 Gm
    // 6A.wav" for "Accent Dub Chord Cm9" (sounds::exportName). A copy only differs when a file of that name holds
    // other bytes, and then takes " (2)".
    juce::String exportFileName (const KitCard& kit, const SoundFile& file);

    // A kit in the drawer's header: a click browses its sounds. Lit in its kit's accent while browsed; its LED
    // marks the kit that plays. Screen readers hear "FUT01 Alloy Signal, shown, playing" (a title, not a toggle,
    // whose press action would untick it).
    class KitChip : public juce::Button
    {
    public:
        explicit KitChip (const juce::String& kitId, const juce::String& kitTitle = {});

        void show (bool isBrowsed, bool isPlayingKit);
        const juce::String& kit() const noexcept { return kitId; }
        bool isBrowsed() const noexcept { return browsed; }
        bool marksPlayingKit() const noexcept { return playing; }

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        void updateTitle();

        juce::String kitId, kitName;
        bool browsed = false, playing = false;
    };

    // A category in the rail, with how many sounds the browsed kit has in it; disabled when none. Screen readers
    // hear "Accents, 3 sounds, shown".
    class CategoryButton : public juce::Button
    {
    public:
        explicit CategoryButton (SoundCategory);

        void show (int numSounds, bool isSelected);
        SoundCategory category() const noexcept { return which; }
        int count() const noexcept { return sounds; }
        bool isSelected() const noexcept { return selected; }

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        void updateTitle();

        SoundCategory which;
        int sounds = 0;
        bool selected = false;
    };

    // One sound in the list. A press selects it and plays its file from the top (on the stop mark of the one
    // playing, it stops); a 4-pixel drag hands its file to the OS.
    class SoundRow : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int height = 28;

        struct Look
        {
            juce::String name; // "Rave Stab"
            juce::String length; // "0.71 s", or "8 bars" when it's whole bars at the kit's tempo
            juce::String spoken; // its accessible title: "Rave Stab, 0.71 seconds"
            bool endAligned = false; // shows END
            int files = 1; // shows "8 files" when more than one
            bool selected = false, playing = false, waiting = false, failed = false;
            bool dimmed = false; // the kit's samples aren't ready: no play mark, no drag

            friend bool operator== (const Look&, const Look&) = default;
        };

        SoundRow (SamplesPanel& owner, int index);

        void show (const Look&);
        const Look& look() const noexcept { return shown; }
        int index() const noexcept { return row; }

        // The play or stop mark at the row's left: a press there on the row that plays stops it.
        juce::Rectangle<int> markArea() const;

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override; // press: plays it

    private:
        SamplesPanel& owner;
        const int row;
        Look shown;
        bool hover = false, pressed = false, dragging = false;
    };

    // The rows, in the drawer's scrolling list. It takes the keyboard: Up and Down move and play, Return
    // plays again. Everything else goes on to the drawer (Esc) and the host (Space, shortcuts).
    class SoundList : public juce::Component
    {
    public:
        explicit SoundList (SamplesPanel& owner);

        bool keyPressed (const juce::KeyPress&) override;
        void focusGained (FocusChangeType) override;
        void focusLost (FocusChangeType) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        SamplesPanel& owner;
    };

    // The selected sound's file as a drag will give it: drag it out, or click it to show it (its press action
    // does the same, the keyboard's way to a file).
    class SoundDragTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit SoundDragTile (SamplesPanel& owner);

        void show (const juce::String& name, bool isFailed);
        const juce::String& fileName() const noexcept { return name; }
        bool hasFailed() const noexcept { return failed; }

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override; // Return shows the file, as a click does
        void enablementChanged() override { repaint(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override; // press: shows the file

    private:
        SamplesPanel& owner;
        juce::String name;
        bool failed = false, hover = false, pressed = false, dragging = false;
    };

    class SamplesPanel : public juce::Component
    {
    public:
        explicit SamplesPanel (Controller&);
        ~SamplesPanel() override;

        // The editor shows the drawer: it opens on the playing kit, following it again, at the category last
        // used (Accents the first time).
        void open (const EditorModel&);

        // The editor's 30 Hz tick: follows the playing kit until a chip is picked, and shows where the kit's
        // samples and the last audition stand. Does nothing while the drawer is hidden.
        void update (const EditorModel&);

        // Gives the list the keyboard (Up, Down, Return), if it shows and has rows: the editor calls it once the
        // drawer is visible, so the keys work from the moment it opens.
        void focusList();

        void resized() override;
        void paint (juce::Graphics&) override;
        bool keyPressed (const juce::KeyPress&) override; // Esc stops the audition (and clears the status line)

        // What it shows, for tests.
        const juce::String& browsedKit() const noexcept { return browsed; }
        SoundCategory category() const noexcept { return shownCategory; }
        int numRows() const noexcept { return rows.size(); }
        int selectedRow() const noexcept { return selected; }
        juce::String statusText() const;
        juce::String bannerText() const;
        juce::String folderText() const; // "Drags save to ~/Music/Wubansia/Samples"
        juce::String detailText() const; // the selected sound's name, "Accents · FUT01 · F#m · 11A", its length and format
        juce::String hintText() const; // "Ends on the beat: put its end on the downbeat (8 bars)"

        // Whose install steps the banner gives; the build's platform unless a test says otherwise.
        ContentPanel::Platform platform;

        // How the drawer hands a file to the OS and asks for a folder. Tests replace them; by default startDrag
        // performs the external drag of the one file, revealFile shows it in the file browser, and chooseFolder
        // opens a folder chooser and calls back with the folder picked.
        std::function<bool (const juce::File&, juce::Component&)> startDrag;
        std::function<void (const juce::File&)> revealFile;
        std::function<void (std::function<void (const juce::File&)>)> chooseFolder;

        // How the drawer says what it shows to a screen reader: by default an AccessibilityHandler announcement. Tests
        // replace it.
        std::function<void (const juce::String&)> announce;

        std::function<void()> chooseContentFolder; // the banner's button; the editor hands it the install panel's chooser
        std::function<void()> onClose; // the x
        std::function<void()> onShowContent; // Content...: the install panel instead

    private:
        friend class SoundRow;
        friend class SoundList;
        friend class SoundDragTile;

        struct Message
        {
            juce::String text;
            juce::Colour tone;
            juce::uint32 until = 0; // juce::Time::getMillisecondCounter (); 0 until the next action
        };

        // The browsed kit and what it holds.
        const std::vector<SoundInfo>& soundsOf (const juce::String& kit); // asks the Controller the first time
        const std::vector<SoundInfo>* listOf (const juce::String& kit) const; // nullptr until asked
        const KitCard* cardOf (const juce::String& kit) const;
        ContentStatus contentOf (const juce::String& kit) const;
        bool kitReady() const;
        const SoundInfo* selectedSound() const;
        int chosenFile (const SoundInfo&) const; // into its files
        SoundRef refOf (const SoundInfo&) const;
        juce::String tileName() const;

        // What the person does.
        void browse (const juce::String& kit);
        void pickKit (const juce::String& kit);
        void pickCategory (SoundCategory);
        void showCategory (SoundCategory wanted);
        void select (int row);
        void rowPressed (int row, bool onMark);
        bool rowDragged (int row, juce::Component& source);
        bool listKey (const juce::KeyPress&);
        void play();
        void stop();
        void fileChosen();
        bool dragOut (juce::Component& source);
        void reveal();
        juce::File exportSelected();
        void changeFolder();
        void folderChosen (const juce::File&);

        // What it shows.
        bool auditioning() const; // the last audition plays, or waits for the host
        juce::String auditionMessage() const;
        juce::String kitMessage() const; // "Looking for FUT03's samples..."
        juce::Colour statusTone() const;
        void setMessage (const juce::String& text, juce::Colour tone, int forMs = 0);
        void refresh();
        void refreshChips();
        void refreshRows();
        void refreshDetail();
        void layOutList();
        void layOutFooter();

        Controller& controller;
        LookAndFeel look; // the browsed kit's accent; outlives every component below
        IconButton stopButton { "Stop", IconButton::Icon::stop };
        IconButton closeButton { "Close", IconButton::Icon::close };
        juce::ComboBox fileBox { "File" };
        juce::TextButton playButton, revealButton, folderButton, contentButton, contentFolderButton;

        // What the editor last said.
        std::vector<KitCard> kits;
        juce::String playingKit;
        ContentStatus playingContent;
        AuditionState audition;
        bool standalone = false; // the Standalone app: no DAW, so no-sound means its audio output isn't running

        // What the person chose.
        juce::String browsed;
        bool chipPicked = false; // a chip picked another kit: the drawer stops following the playing kit
        bool handingOver = false; // startDrag is handing a drag to the OS
        SoundCategory wantedCategory = SoundCategory::accents, shownCategory = SoundCategory::accents;
        std::map<juce::String, std::vector<SoundInfo>> catalog; // by kit: sounds () once each
        std::vector<int> shownSounds; // into the browsed kit's sounds, one per row
        int selected = -1;
        std::map<juce::String, int> chosenFiles; // by sound id: the file picked in File

        // The last audition: its ticket (0 when turned down), the sound and file it played, why it was turned
        // down, and whether Stop or Esc put its message away.
        std::uint32_t ticket = 0;
        juce::String auditionKit, auditionSound, auditionLabel, refusal;
        bool auditionDismissed = true;

        Message message;
        bool tileFailed = false;
        SoundRef exportedRef;
        juce::File exported; // the last copy, so the tile names it as written
        juce::File samplesFolder;

        juce::StringArray fileBoxSound; // the kit and sound whose files the File box lists
        juce::String painted; // what paint () last drew of the text, to repaint only on a change
        juce::String spokenStatus, spokenBanner; // what announce () last said of each, so each is said once

        juce::OwnedArray<KitChip> chips;
        juce::StringArray chipKits;
        std::array<std::unique_ptr<CategoryButton>, kSoundCategories> categories;
        SoundList list { *this };
        juce::Viewport viewport;
        juce::OwnedArray<SoundRow> rows;
        SoundDragTile tile { *this };
        std::unique_ptr<juce::FileChooser> chooser;

        // Laid out by resized ().
        juce::Rectangle<int> headerArea, railArea, listArea, detailArea, footerArea, bannerArea, wellArea;
        juce::Rectangle<int> titleArea, nameArea, subArea, fileLabelArea, formatArea, hintArea, folderArea, statusArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplesPanel)
    };
}
