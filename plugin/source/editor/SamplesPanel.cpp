#include "SamplesPanel.h"

#include "dragout/DragFiles.h"
#include "sounds/SampleExport.h"
#include "sounds/SoundCatalog.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace wubansia::editor
{
    namespace
    {
        // The drawer's grid inside its 1080 x 396 (ADR 0024): a 36 px header, a 28 px footer, and between them
        // the 150 px rail, the 530 px list and the 400 px detail pane.
        constexpr int headerHeight = 36, footerHeight = 28, railWidth = 150, listWidth = 530;
        constexpr int bannerHeight = 128;

        // Focus order (accessibility and Tab): chips, categories, the rows, File, the tile, Play, Show in Finder,
        // then Stop, Close, Change..., Content... and the banner's button.
        constexpr int chipOrder = 1, categoryOrder = 20, listOrder = 30;

        ContentPanel::Platform buildPlatform()
        {
           #if JUCE_MAC
            return ContentPanel::Platform::macOS;
           #elif JUCE_WINDOWS
            return ContentPanel::Platform::windowsOS;
           #else
            return ContentPanel::Platform::linuxOS;
           #endif
        }

        juce::String fromUtf8 (const std::string& s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // Paths read better with the home folder as "~".
        juce::String tidy (const juce::File& file)
        {
            const auto path = file.getFullPathName();
            const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            return home.isNotEmpty() && path.startsWith (home) ? "~" + path.substring (home.length()) : path;
        }

        juce::String withoutFullStop (juce::String text)
        {
            text = text.trim();
            while (text.endsWithChar ('.'))
                text = text.dropLastCharacters (1);
            return text;
        }

        // A content check's problem without its leading code ("missing: ", "lfs_pointer: ", "mismatch: ", "rate: ",
        // "unreadable: "...): a lowercase word, or words joined by '_', then ": ". Anything else is left as it is.
        juce::String withoutCode (const juce::String& problem)
        {
            const auto colon = problem.indexOf (": ");
            const auto code = problem.substring (0, juce::jmax (0, colon));
            if (colon <= 0 || ! code.containsOnly ("abcdefghijklmnopqrstuvwxyz_"))
                return problem;
            return problem.substring (colon + 2);
        }

        // A problem as a sentence: "no folder holds FUT02" becomes "No folder holds FUT02."; a path keeps its case.
        juce::String sentence (const juce::String& problem)
        {
            auto text = withoutFullStop (problem);
            if (text.isEmpty())
                return {};
            const auto firstWord = text.upToFirstOccurrenceOf (" ", false, false);
            if (juce::CharacterFunctions::isLowerCase (text[0]) && ! firstWord.containsAnyOf ("/\\."))
                text = text.substring (0, 1).toUpperCase() + text.substring (1);
            return text + ".";
        }

        juce::String barsText (int bars) { return juce::String (bars) + (bars == 1 ? " bar" : " bars"); }
        juce::String secondsText (double seconds) { return juce::String (seconds, 2); }

        // How many notes were recorded: the files' names less their layer (" v2") and round robin (" rr1").
        int recordedNotes (const SoundInfo& sound)
        {
            const auto strip = [] (juce::String name, const juce::String& suffix)
            {
                const auto at = name.lastIndexOf (suffix);
                const auto digits = at > 0 ? name.substring (at + suffix.length()) : juce::String();
                return digits.isNotEmpty() && digits.containsOnly ("0123456789") ? name.substring (0, at) : name;
            };
            std::set<juce::String> notes;
            for (const auto& file : sound.files)
                notes.insert (strip (strip (file.label, " rr"), " v"));
            return static_cast<int> (notes.size());
        }

        // The one hint that matters most (spec 3.5): how an end-aligned sound sits, a lead-in, a loop's bars, or
        // the notes to pick from.
        juce::String hintFor (const SoundInfo& sound, const KitCard& kit)
        {
            if (sound.endAligned)
                return "Ends on the beat: put its end on the downbeat" + (sound.bars > 0 ? " (" + barsText (sound.bars) + ")" : juce::String());
            if (sound.leadInMs > 0)
            {
                const auto ms = juce::String (sound.leadInMs);
                return "Starts " + ms + " ms before its beat: slide it " + ms + " ms earlier";
            }
            if (sound.loopBars > 0)
            {
                auto hint = barsText (sound.bars > 0 ? sound.bars : sound.loopBars) + " at " + juce::String (kit.bpm) + " BPM";
                if (sound.bars > sound.loopBars)
                    hint << ": a " << sound.loopBars << "-bar loop, then a " << (sound.bars - sound.loopBars) << "-bar echo tail";
                return hint;
            }
            if (const auto notes = recordedNotes (sound); notes > 1)
                return juce::String (notes) + " recorded notes: pick one in File";
            return {};
        }

        const char* categoryId (SoundCategory category)
        {
            switch (category)
            {
                case SoundCategory::accents: return "accents";
                case SoundCategory::fx: return "fx";
                case SoundCategory::vox: return "vox";
                case SoundCategory::synths: return "synths";
                case SoundCategory::bass: return "bass";
                case SoundCategory::drums: return "drums";
                case SoundCategory::loops: return "loops";
            }
            return "";
        }

        const char* categoryTip (SoundCategory category)
        {
            switch (category)
            {
                case SoundCategory::accents: return "The kit's accent synths and hits: stabs, chords, sweeps, bells";
                case SoundCategory::fx: return "Risers, fills and drops";
                case SoundCategory::vox: return "The vox synth and its chops";
                case SoundCategory::synths: return "Leads, plucks, keys, stabs and pads";
                case SoundCategory::bass: return "The basses, every recorded note and layer";
                case SoundCategory::drums: return "Kicks, claps, hats and percussion";
                case SoundCategory::loops: return "The kit's recorded phrases, whole bars at its tempo";
            }
            return "";
        }

        // A row's state for screen readers, as the list shows it.
        class RowAccessibility final : public juce::AccessibilityHandler
        {
        public:
            RowAccessibility (SoundRow& r, juce::AccessibilityActions actions)
                : juce::AccessibilityHandler (r, juce::AccessibilityRole::listItem, std::move (actions)), row (r)
            {
            }

            juce::AccessibleState getCurrentState() const override
            {
                auto state = juce::AccessibilityHandler::getCurrentState().withSelectable();
                return row.look().selected ? state.withSelected() : state;
            }

        private:
            SoundRow& row;
        };
    }

    juce::String exportFileName (const KitCard& kit, const SoundFile& file)
    {
        engine::KitInfo info;
        info.id = kit.id.toStdString();
        info.title = kit.title.toStdString();
        info.key = kit.key.toStdString();
        info.camelot = kit.camelot.toStdString();
        info.bpm = kit.bpm;
        info.contentVersion = kit.contentVersion;
        engine::FileEntry entry;
        entry.path = (file.label + ".wav").toStdString(); // the installed name: the stem export names start with
        return fromUtf8 (sounds::exportName (info, entry));
    }

    //==============================================================================================================
    KitChip::KitChip (const juce::String& id)
        : juce::Button (id), kitId (id)
    {
        setComponentID ("samples_kit_" + id);
    }

    void KitChip::show (bool isBrowsed, bool isPlayingKit)
    {
        if (isBrowsed == browsed && isPlayingKit == playing)
            return;
        browsed = isBrowsed;
        playing = isPlayingKit;
        repaint();
    }

    void KitChip::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = accentForKit (kitId);
        auto area = getLocalBounds().toFloat().reduced (1.5f);
        const auto corner = area.getHeight() * 0.5f;
        draw::rubber (g, area, corner, isButtonDown, isMouseOver, accent, 0.0f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);

        // Browsed: a lit rim and a tint in its kit's colour.
        if (browsed)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.32f), area.getY(), accent.withAlpha (0.10f), area.getBottom()));
            g.fillRoundedRectangle (area, corner);
            g.setColour (accent.withAlpha (0.9f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.2f);
        }

        // Its LED lights for the kit that plays.
        draw::led (g, { area.getX() + 12.0f, area.getCentreY() }, 3.4f, accent, playing ? 1.0f : 0.0f);
        g.setFont (uiFont (11.5f, true, 0.08f));
        g.setColour (browsed ? colours::text : accent.interpolatedWith (colours::textDim, 0.35f));
        g.drawText (kitId, area.withTrimmedLeft (20.0f).withTrimmedRight (5.0f), juce::Justification::centred, false);
    }

    //==============================================================================================================
    CategoryButton::CategoryButton (SoundCategory c)
        : juce::Button (sounds::categoryName (c)), which (c)
    {
        setComponentID (juce::String ("samples_category_") + categoryId (c));
        setTooltip (categoryTip (c));
        setTitle (getButtonText());
        setEnabled (false); // until show () gives it sounds
    }

    void CategoryButton::show (int numSounds, bool isSelected)
    {
        if (numSounds == sounds && isSelected == selected)
            return;
        sounds = numSounds;
        selected = isSelected;
        setEnabled (sounds > 0);
        setTitle (getButtonText() + ", " + juce::String (sounds) + (sounds == 1 ? " sound" : " sounds"));
        repaint();
    }

    void CategoryButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = accentOf (*this);
        const auto enabled = isEnabled();
        const auto area = getLocalBounds().toFloat().reduced (1.0f);

        if (selected)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.24f), area.getY(), accent.withAlpha (0.10f), area.getBottom()));
            g.fillRoundedRectangle (area, 6.0f);
            g.setColour (accent.withAlpha (0.6f));
            g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (area.withWidth (3.0f).reduced (0.0f, 8.0f).translated (6.0f, 0.0f), 1.5f);
        }
        else if (isMouseOver && enabled)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (area, 6.0f);
        }

        auto text = area.withTrimmedLeft (17.0f).withTrimmedRight (11.0f).translated (0.0f, isButtonDown ? 0.6f : 0.0f);
        g.setFont (uiFont (13.0f, true, 0.02f));
        g.setColour (! enabled ? colours::textFaint.withAlpha (0.6f) : (selected ? colours::text : colours::textDim));
        g.drawText (getButtonText(), text, juce::Justification::centredLeft, true);
        g.setFont (uiFont (11.5f, true));
        g.setColour (! enabled ? colours::textFaint.withAlpha (0.5f) : (selected ? accent.brighter (0.3f) : colours::textFaint));
        g.drawText (juce::String (sounds), text, juce::Justification::centredRight, false);
    }

    //==============================================================================================================
    SoundRow::SoundRow (SamplesPanel& o, int index)
        : owner (o), row (index)
    {
        setComponentID ("sound_row_" + juce::String (index));
        setTooltip ("Click to hear it, drag it into your DAW");
    }

    void SoundRow::show (const Look& look)
    {
        if (look == shown)
            return;
        shown = look;
        setTitle (look.spoken);
        repaint();
    }

    juce::Rectangle<int> SoundRow::markArea() const
    {
        return getLocalBounds().removeFromLeft (32);
    }

    void SoundRow::paint (juce::Graphics& g)
    {
        const auto accent = accentOf (*this);
        const auto& l = shown;
        const auto alpha = l.dimmed ? 0.4f : 1.0f;
        const auto area = getLocalBounds().toFloat().reduced (2.0f, 1.0f);

        // The playing row lit in the kit's colour, the selected one raised, the one under the mouse a touch lighter.
        if (l.playing && ! l.dimmed)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.30f), area.getY(), accent.withAlpha (0.14f), area.getBottom()));
            g.fillRoundedRectangle (area, 5.0f);
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
        }
        else if (l.selected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.075f));
            g.fillRoundedRectangle (area, 5.0f);
            g.setColour ((l.failed ? colours::warning : accent).withAlpha (0.4f * alpha));
            g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
        }
        else if (hover && ! l.dimmed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (area, 5.0f);
        }

        // The mark: play, stop while it plays, three dots while it waits; the warning colour when it couldn't play.
        if (! l.dimmed)
        {
            const auto box = markArea().toFloat().withSizeKeepingCentre (9.0f, 9.0f).translated (2.0f, 0.0f);
            if (l.waiting)
            {
                g.setColour (colours::busy);
                for (int dot = 0; dot < 3; ++dot)
                    g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ box.getX() + 1.0f + (float) dot * 3.6f, box.getCentreY() }));
            }
            else
            {
                g.setColour (l.failed ? colours::warning : (l.playing ? accent.brighter (0.25f) : (l.selected || hover ? colours::text : colours::textDim)));
                g.fillPath (l.playing ? draw::stopGlyph (box) : draw::playGlyph (box));
            }
        }

        // From the right: the grip, how many files, END, the length; the name takes the rest.
        auto line = getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedRight (10.0f);
        const auto gripArea = line.removeFromRight (12.0f);
        if (! l.dimmed)
            draw::grip (g, gripArea.withSizeKeepingCentre (7.0f, 11.0f), colours::textFaint.withAlpha (hover || l.selected ? 1.0f : 0.55f));
        line.removeFromRight (12.0f);

        const auto filesArea = line.removeFromRight (52.0f);
        if (l.files > 1)
        {
            g.setFont (uiFont (11.0f));
            g.setColour (colours::textFaint.brighter (0.15f).withAlpha (alpha));
            g.drawText (juce::String (l.files) + " files", filesArea, juce::Justification::centredRight, false);
        }
        line.removeFromRight (10.0f);

        const auto endArea = line.removeFromRight (36.0f);
        if (l.endAligned)
        {
            const auto badge = endArea.withSizeKeepingCentre (34.0f, 15.0f);
            g.setColour (colours::busy.withAlpha (0.13f * alpha));
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (colours::busy.withAlpha (0.6f * alpha));
            g.drawRoundedRectangle (badge.reduced (0.5f), 3.0f, 0.8f);
            g.setFont (uiFont (9.0f, true, 0.12f));
            g.setColour (colours::busy.withAlpha (alpha));
            g.drawText ("END", badge, juce::Justification::centred, false);
        }
        line.removeFromRight (10.0f);

        const auto lengthArea = line.removeFromRight (62.0f);
        g.setFont (uiFont (12.0f));
        g.setColour ((l.playing ? colours::text : colours::textDim).withAlpha (alpha));
        g.drawText (l.length, lengthArea, juce::Justification::centredRight, false);
        line.removeFromRight (12.0f);

        g.setFont (uiFont (13.5f, true));
        g.setColour ((l.failed ? colours::warning : (l.playing || l.selected ? colours::text : colours::text.withAlpha (0.85f))).withAlpha (alpha));
        g.drawText (l.name, line, juce::Justification::centredLeft, true);
    }

    void SoundRow::mouseEnter (const juce::MouseEvent&)
    {
        hover = true;
        repaint();
    }

    void SoundRow::mouseExit (const juce::MouseEvent&)
    {
        hover = false;
        repaint();
    }

    void SoundRow::mouseDown (const juce::MouseEvent& e)
    {
        pressed = ! e.mods.isPopupMenu();
        dragging = false;
        if (pressed)
            owner.rowPressed (row, markArea().contains (e.getPosition()));
    }

    void SoundRow::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressed || dragging || e.getDistanceFromDragStart() < 4)
            return;
        dragging = true;
        owner.rowDragged (row, *this); // the OS runs the drag from here (on macOS, inside this mouse event)
    }

    void SoundRow::mouseUp (const juce::MouseEvent&)
    {
        pressed = dragging = false;
    }

    std::unique_ptr<juce::AccessibilityHandler> SoundRow::createAccessibilityHandler()
    {
        return std::make_unique<RowAccessibility> (*this,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { owner.rowPressed (row, false); }));
    }

    //==============================================================================================================
    SoundList::SoundList (SamplesPanel& o)
        : owner (o)
    {
        setComponentID ("sound_list");
        setTitle ("Sounds");
        setWantsKeyboardFocus (true);
    }

    bool SoundList::keyPressed (const juce::KeyPress& key)
    {
        return owner.listKey (key);
    }

    void SoundList::focusGained (FocusChangeType)
    {
        owner.repaint();
    }

    void SoundList::focusLost (FocusChangeType)
    {
        owner.repaint();
    }

    std::unique_ptr<juce::AccessibilityHandler> SoundList::createAccessibilityHandler()
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::list);
    }

    //==============================================================================================================
    SoundDragTile::SoundDragTile (SamplesPanel& o)
        : owner (o)
    {
        setComponentID ("sound_drag");
        setTooltip ("Drag this WAV into Live, Logic or Finder. Click to show it.");
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void SoundDragTile::show (const juce::String& fileName, bool isFailed)
    {
        if (fileName == name && isFailed == failed)
            return;
        name = fileName;
        failed = isFailed;
        setTitle (name.isNotEmpty() ? "Drag " + name + " into your DAW" : juce::String ("No sound to drag"));
        repaint();
    }

    void SoundDragTile::paint (juce::Graphics& g)
    {
        const auto accent = accentOf (*this);
        const auto enabled = isEnabled();
        const auto active = pressed || dragging;
        const auto area = getLocalBounds().toFloat().reduced (1.5f);
        constexpr auto corner = 9.0f;

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner);
        g.setGradientFill (juce::ColourGradient::vertical (active ? accent.withAlpha (0.30f).overlaidWith (juce::Colour (0xff1d2025))
                                                                  : juce::Colour (hover && enabled ? 0xff262a31 : 0xff1d2025),
            area.getY(), juce::Colour (0xff131518), area.getBottom()));
        g.fillRoundedRectangle (area, corner);

        // A dashed border says "drag me".
        const auto border = failed ? colours::warning : (enabled ? accent.withAlpha (hover || active ? 1.0f : 0.7f) : juce::Colour (0xff3a3f47));
        juce::Path outline;
        outline.addRoundedRectangle (area.reduced (0.5f), corner);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 3.5f };
        juce::PathStrokeType (1.3f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (border);
        g.fillPath (dashed);

        auto content = area.reduced (16.0f, 13.0f);
        const auto lit = failed ? colours::warning : (enabled ? accent : colours::textFaint);
        draw::grip (g, content.removeFromLeft (20.0f).withSizeKeepingCentre (10.0f, 17.0f), lit);
        content.removeFromLeft (14.0f);
        draw::engraved (g, failed ? "Couldn't save it" : "Drag into your DAW", content.removeFromTop (13.0f), juce::Justification::centredLeft, 9.5f,
            failed ? colours::warning : (enabled ? lit.interpolatedWith (colours::text, 0.25f) : colours::textFaint));
        content.removeFromTop (6.0f);
        g.setFont (uiFont (13.5f, true));
        g.setColour (enabled ? colours::text : colours::textFaint);
        g.drawFittedText (name, content.toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
    }

    void SoundDragTile::mouseEnter (const juce::MouseEvent&)
    {
        hover = true;
        repaint();
    }

    void SoundDragTile::mouseExit (const juce::MouseEvent&)
    {
        hover = false;
        repaint();
    }

    void SoundDragTile::mouseDown (const juce::MouseEvent& e)
    {
        pressed = isEnabled() && ! e.mods.isPopupMenu();
        dragging = false;
        repaint();
    }

    void SoundDragTile::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressed || dragging || e.getDistanceFromDragStart() < 4)
            return;
        dragging = true;
        repaint();
        owner.dragOut (*this); // the OS runs the drag from here (on macOS, inside this mouse event)
    }

    void SoundDragTile::mouseUp (const juce::MouseEvent& e)
    {
        const auto click = pressed && ! dragging && e.mouseWasClicked();
        pressed = dragging = false;
        repaint();
        if (click)
            owner.reveal();
    }

    bool SoundDragTile::keyPressed (const juce::KeyPress& key)
    {
        if (key != juce::KeyPress::returnKey)
            return false;
        if (isEnabled())
            owner.reveal();
        return true;
    }

    std::unique_ptr<juce::AccessibilityHandler> SoundDragTile::createAccessibilityHandler()
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this]
                {
                    if (isEnabled())
                        owner.reveal();
                }));
    }

    //==============================================================================================================
    SamplesPanel::SamplesPanel (Controller& c)
        : platform (buildPlatform()), controller (c)
    {
        setComponentID ("samples_drawer");
        setTitle ("Samples");
        setLookAndFeel (&look);

        startDrag = [] (const juce::File& file, juce::Component& source)
        {
            return juce::DragAndDropContainer::performExternalDragDropOfFiles ({ dragout::externalDragPath (file) }, false, &source);
        };
        revealFile = [] (const juce::File& file) { dragout::revealToUser (file); };
        chooseFolder = [this] (std::function<void (const juce::File&)> done)
        {
            const auto start = samplesFolder.isDirectory() ? samplesFolder : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
            chooser = std::make_unique<juce::FileChooser> ("Choose where Wubansia saves the sounds you drag out", start);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [done] (const juce::FileChooser& fc)
                {
                    const auto folder = fc.getResult();
                    if (folder.isDirectory())
                        done (folder);
                });
        };

        // The header's Stop and Close.
        stopButton.setComponentID ("sound_stop");
        stopButton.setTooltip ("Stop the sound (Esc)");
        stopButton.onClick = [this] { stop(); };
        closeButton.setComponentID ("samples_close");
        closeButton.setTooltip ("Close the drawer; the Samples button opens it again");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };
        stopButton.setExplicitFocusOrder (listOrder + 5);
        closeButton.setExplicitFocusOrder (listOrder + 6);
        addAndMakeVisible (stopButton);
        addAndMakeVisible (closeButton);

        // The rail.
        for (std::size_t i = 0; i < categories.size(); ++i)
        {
            const auto category = static_cast<SoundCategory> (i);
            categories[i] = std::make_unique<CategoryButton> (category);
            categories[i]->onClick = [this, category] { pickCategory (category); };
            categories[i]->setExplicitFocusOrder (categoryOrder + static_cast<int> (i));
            addAndMakeVisible (*categories[i]);
        }

        // The list.
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.setWantsKeyboardFocus (false);
        viewport.setExplicitFocusOrder (listOrder);
        addAndMakeVisible (viewport);

        // The detail pane.
        fileBox.setComponentID ("sound_file");
        fileBox.setTitle ("File");
        fileBox.setTooltip ("v1 is the softest layer, rr a round robin. Notes count middle C as C4; Live and Logic call it C3.");
        fileBox.onChange = [this] { fileChosen(); };
        fileBox.setExplicitFocusOrder (listOrder + 1);
        tile.setExplicitFocusOrder (listOrder + 2);
        playButton.setButtonText ("Play");
        playButton.setComponentID ("sound_play");
        playButton.getProperties().set ("icon", "play");
        playButton.setTooltip ("Hear the file from the top (Return in the list)");
        playButton.onClick = [this] { play(); };
        playButton.setExplicitFocusOrder (listOrder + 3);
        revealButton.setButtonText (platform == ContentPanel::Platform::macOS     ? "Show in Finder"
                                    : platform == ContentPanel::Platform::windowsOS ? "Show in Explorer"
                                                                                    : "Show in folder");
        revealButton.setComponentID ("sound_reveal");
        revealButton.getProperties().set ("quiet", true);
        revealButton.getProperties().set ("icon", "folder");
        revealButton.setTooltip ("Save the file into your samples folder and show it there");
        revealButton.onClick = [this] { reveal(); };
        revealButton.setExplicitFocusOrder (listOrder + 4);
        for (auto* component : std::initializer_list<juce::Component*> { &fileBox, &tile, &playButton, &revealButton })
            addAndMakeVisible (component);

        // The footer, and the banner's button.
        folderButton.setButtonText (utf8 ("Change\xe2\x80\xa6"));
        folderButton.setComponentID ("samples_folder");
        folderButton.getProperties().set ("quiet", true);
        folderButton.onClick = [this] { changeFolder(); };
        folderButton.setExplicitFocusOrder (listOrder + 7);
        contentButton.setButtonText (utf8 ("Content\xe2\x80\xa6"));
        contentButton.setComponentID ("samples_content");
        contentButton.getProperties().set ("quiet", true);
        contentButton.setTooltip ("Where the kits' samples are installed, and how to install them");
        contentButton.onClick = [this]
        {
            if (onShowContent != nullptr)
                onShowContent();
        };
        contentButton.setExplicitFocusOrder (listOrder + 8);
        contentFolderButton.setButtonText (utf8 ("Choose content folder\xe2\x80\xa6"));
        contentFolderButton.setComponentID ("samples_content_folder");
        contentFolderButton.setTooltip ("Point Wubansia at the folder that holds the kits' sample folders (fut01, fut02...). "
                                        "It's remembered for you, not saved in projects.");
        contentFolderButton.onClick = [this]
        {
            if (chooseContentFolder != nullptr)
                chooseContentFolder();
        };
        contentFolderButton.setExplicitFocusOrder (listOrder + 9);
        addAndMakeVisible (folderButton);
        addAndMakeVisible (contentButton);
        addChildComponent (contentFolderButton);
    }

    SamplesPanel::~SamplesPanel()
    {
        setLookAndFeel (nullptr);
    }

    //==============================================================================================================
    void SamplesPanel::open (const EditorModel& model)
    {
        kits = model.kits;
        playingKit = model.hasKit() ? model.kit.id : juce::String();
        playingContent = model.content;
        audition = model.audition;
        standalone = model.standalone;
        samplesFolder = controller.samplesFolder();
        chipPicked = false;
        message = {};
        refusal.clear();
        auditionDismissed = true; // closing stopped what played; its words go too
        exported = juce::File();
        refreshChips();
        layOutFooter();

        // Afresh, on the playing kit at the category last picked.
        browsed.clear();
        browse (playingKit.isNotEmpty() ? playingKit : (kits.empty() ? juce::String() : kits.front().id));
        refresh();
    }

    void SamplesPanel::update (const EditorModel& model)
    {
        if (! isVisible())
            return;

        kits = model.kits;
        playingKit = model.hasKit() ? model.kit.id : juce::String();
        playingContent = model.content;
        audition = model.audition;
        standalone = model.standalone;
        refreshChips();

        // It follows the kit that plays until a chip picks another; never in the middle of an OS drag (Windows runs
        // one in a modal loop, timer and all), which would delete the row the drag started from.
        if (! chipPicked && ! handingOver && playingKit.isNotEmpty() && playingKit != browsed)
            browse (playingKit);

        if (message.until != 0 && static_cast<int> (juce::Time::getMillisecondCounter() - message.until) >= 0)
            message = {};
        refresh();
    }

    //==============================================================================================================
    const std::vector<SoundInfo>& SamplesPanel::soundsOf (const juce::String& kit)
    {
        auto it = catalog.find (kit);
        if (it == catalog.end())
            it = catalog.emplace (kit, controller.sounds (kit)).first;
        return it->second;
    }

    const std::vector<SoundInfo>* SamplesPanel::listOf (const juce::String& kit) const
    {
        const auto it = catalog.find (kit);
        return it != catalog.end() ? &it->second : nullptr;
    }

    const KitCard* SamplesPanel::cardOf (const juce::String& kit) const
    {
        for (const auto& card : kits)
            if (card.id == kit)
                return &card;
        return nullptr;
    }

    ContentStatus SamplesPanel::contentOf (const juce::String& kit) const
    {
        if (kit == playingKit)
            return playingContent; // the current kit's load, not the survey's
        if (const auto* card = cardOf (kit))
            return card->content;
        return {};
    }

    bool SamplesPanel::kitReady() const
    {
        return contentOf (browsed).state == ContentState::ready;
    }

    const SoundInfo* SamplesPanel::selectedSound() const
    {
        const auto* sounds = listOf (browsed);
        if (sounds == nullptr || selected < 0 || static_cast<std::size_t> (selected) >= shownSounds.size())
            return nullptr;
        return &(*sounds)[static_cast<std::size_t> (shownSounds[static_cast<std::size_t> (selected)])];
    }

    int SamplesPanel::chosenFile (const SoundInfo& sound) const
    {
        const auto count = static_cast<int> (sound.files.size());
        if (const auto it = chosenFiles.find (sound.id); it != chosenFiles.end() && it->second >= 0 && it->second < count)
            return it->second;
        return juce::jlimit (0, juce::jmax (0, count - 1), sound.defaultFile);
    }

    SoundRef SamplesPanel::refOf (const SoundInfo& sound) const
    {
        if (sound.files.empty())
            return { browsed, -1 };
        return { browsed, sound.files[static_cast<std::size_t> (chosenFile (sound))].file };
    }

    juce::String SamplesPanel::tileName() const
    {
        const auto* sound = selectedSound();
        const auto* card = cardOf (browsed);
        if (sound == nullptr || card == nullptr || sound->files.empty())
            return {};
        if (exported != juce::File() && exportedRef == refOf (*sound))
            return exported.getFileName(); // the copy as written, " (2)" and all
        return exportFileName (*card, sound->files[static_cast<std::size_t> (chosenFile (*sound))]);
    }

    //==============================================================================================================
    void SamplesPanel::browse (const juce::String& kit)
    {
        if (kit == browsed)
            return;
        browsed = kit;
        look.setAccent (accentForKit (kit));
        message = {};
        auditionDismissed = true;
        tileFailed = false;
        showCategory (wantedCategory);
        repaint(); // the whole drawer takes the kit's colour
    }

    void SamplesPanel::pickKit (const juce::String& kit)
    {
        chipPicked = kit != playingKit; // the playing kit's own chip follows it again
        browse (kit);
        focusList();
    }

    void SamplesPanel::pickCategory (SoundCategory category)
    {
        wantedCategory = category;
        if (category != shownCategory)
        {
            if (message.tone == colours::warning)
                message = {};
            showCategory (category);
        }
        focusList();
    }

    void SamplesPanel::showCategory (SoundCategory wanted)
    {
        const auto& sounds = soundsOf (browsed);
        const auto has = [&sounds] (SoundCategory c)
        { return std::any_of (sounds.begin(), sounds.end(), [c] (const SoundInfo& s) { return s.category == c; }); };

        // The category picked, or the first this kit has.
        auto shown = wanted;
        if (! has (shown))
            for (int i = 0; i < kSoundCategories; ++i)
                if (has (static_cast<SoundCategory> (i)))
                {
                    shown = static_cast<SoundCategory> (i);
                    break;
                }
        shownCategory = shown;

        shownSounds.clear();
        for (std::size_t i = 0; i < sounds.size(); ++i)
            if (sounds[i].category == shown)
                shownSounds.push_back (static_cast<int> (i));

        rows.clear();
        for (std::size_t i = 0; i < shownSounds.size(); ++i)
            list.addAndMakeVisible (rows.add (new SoundRow (*this, static_cast<int> (i))));
        selected = shownSounds.empty() ? -1 : 0;
        tileFailed = false;
        viewport.setViewPosition (0, 0);
        layOutList();
        refresh();
    }

    void SamplesPanel::select (int row)
    {
        if (row < 0 || row >= rows.size() || row == selected)
            return;
        selected = row;
        tileFailed = false;

        // Keep it in view.
        const auto* shown = rows[row];
        const auto top = viewport.getViewPositionY();
        if (shown->getY() < top)
            viewport.setViewPosition (0, shown->getY());
        else if (shown->getBottom() > top + viewport.getViewHeight())
            viewport.setViewPosition (0, shown->getBottom() - viewport.getViewHeight());
        refresh();
    }

    void SamplesPanel::rowPressed (int row, bool onMark)
    {
        const auto wasPlaying = row >= 0 && row < rows.size() && rows[row]->look().playing;
        select (row);
        if (onMark && wasPlaying)
            stop();
        else
            play();
    }

    bool SamplesPanel::rowDragged (int row, juce::Component& source)
    {
        select (row);
        return dragOut (source);
    }

    bool SamplesPanel::listKey (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            if (rows.isEmpty())
                return false;
            const auto next = selected + (key == juce::KeyPress::upKey ? -1 : 1);
            if (next >= 0 && next < rows.size())
            {
                select (next);
                play();
            }
            return true; // the end of the list: nothing to do, but the key was the list's
        }
        if (key == juce::KeyPress::returnKey && selectedSound() != nullptr)
        {
            play();
            return true;
        }
        return false; // Esc goes on to the drawer; Space, shortcuts and the rest to the host
    }

    bool SamplesPanel::keyPressed (const juce::KeyPress& key)
    {
        if (key != juce::KeyPress::escapeKey)
            return false;
        if (! auditioning() && auditionMessage().isEmpty() && message.text.isEmpty())
            return false; // nothing to stop or put away: the host may want it
        stop();
        return true;
    }

    void SamplesPanel::play()
    {
        const auto* sound = selectedSound();
        if (sound == nullptr || sound->files.empty() || ! kitReady())
            return;

        message = {};
        refusal.clear();
        auditionDismissed = false;
        auditionKit = browsed;
        auditionSound = sound->id;
        auditionLabel = sound->files[static_cast<std::size_t> (chosenFile (*sound))].label;
        ticket = controller.audition (refOf (*sound));
        audition = controller.auditionState();
        if (ticket == 0)
            refusal = audition.problem.isNotEmpty() ? audition.problem : juce::String ("it can't play now");
        refresh();
    }

    void SamplesPanel::stop()
    {
        controller.stopAudition();
        audition = controller.auditionState();
        auditionDismissed = true;
        message = {};
        refresh();
    }

    void SamplesPanel::fileChosen()
    {
        const auto* sound = selectedSound();
        const auto index = fileBox.getSelectedItemIndex();
        if (sound == nullptr || index < 0 || index >= static_cast<int> (sound->files.size()))
            return;
        chosenFiles[sound->id] = index;
        tileFailed = false;
        play();
        refresh();
    }

    bool SamplesPanel::dragOut (juce::Component& source)
    {
        if (! kitReady() || selectedSound() == nullptr)
            return false;
        const auto file = exportSelected();
        if (file == juce::File() || startDrag == nullptr)
            return false;
        const juce::ScopedValueSetter<bool> handing (handingOver, true);
        return startDrag (file, source);
    }

    void SamplesPanel::reveal()
    {
        if (! kitReady() || selectedSound() == nullptr)
            return;
        const auto file = exportSelected();
        if (file != juce::File() && revealFile != nullptr)
            revealFile (file);
    }

    juce::File SamplesPanel::exportSelected()
    {
        const auto* sound = selectedSound();
        if (sound == nullptr || sound->files.empty())
            return {};
        const auto ref = refOf (*sound);
        const auto label = sound->files[static_cast<std::size_t> (chosenFile (*sound))].label;
        const auto copy = controller.exportSound (ref);
        auditionDismissed = true; // what the status line says now is about the file

        if (copy.file == juce::File())
        {
            // What helps: another samples folder when the copy couldn't be written; when the kit's own file is the
            // problem, the kit's samples (unless the problem already says to reinstall).
            const auto problem = withoutFullStop (copy.problem);
            const auto advice = ! copy.sourceProblem                    ? utf8 (" Choose another folder with Change\xe2\x80\xa6")
                                : problem.containsIgnoreCase ("install") ? juce::String()
                                                                         : " Check that " + ref.kit + "'s samples are still installed.";
            tileFailed = true;
            setMessage ("Couldn't save " + label + ": " + problem + "." + advice, colours::warning);
            refresh();
            return {};
        }
        tileFailed = false;
        exportedRef = ref;
        exported = copy.file;
        setMessage ("Saved " + copy.file.getFileName(), colours::textDim, 3000);
        refresh();
        return copy.file;
    }

    void SamplesPanel::changeFolder()
    {
        if (chooseFolder == nullptr)
            return;
        juce::Component::SafePointer<SamplesPanel> safe (this);
        chooseFolder ([safe] (const juce::File& folder)
            {
                if (safe != nullptr)
                    safe->folderChosen (folder);
            });
    }

    void SamplesPanel::folderChosen (const juce::File& folder)
    {
        if (folder == juce::File())
            return;
        if (controller.setSamplesFolder (folder))
        {
            samplesFolder = controller.samplesFolder();
            message = {};
            tileFailed = false;
            exported = juce::File(); // the next copy goes to the new folder
        }
        else
        {
            setMessage ("Couldn't save drags to " + tidy (folder) + ": it's inside a content folder or one of Wubansia's own folders, "
                        "or it can't be made. Choose another folder.",
                colours::warning);
        }
        layOutFooter();
        refresh();
        repaint();
    }

    void SamplesPanel::focusList()
    {
        if (list.isShowing() && ! rows.isEmpty())
            list.grabKeyboardFocus();
    }

    //==============================================================================================================
    bool SamplesPanel::auditioning() const
    {
        return ticket != 0 && (audition.sounding == ticket || (audition.waiting && audition.latest == ticket));
    }

    juce::String SamplesPanel::auditionMessage() const
    {
        if (auditionDismissed)
            return {};
        if (refusal.isNotEmpty())
            return "Couldn't play " + auditionLabel + ": " + refusal;
        if (ticket == 0 || audition.latest != ticket)
            return {};
        if (audition.hostSilent && standalone) // JUCE's standalone window: its Options button and menu item
            return "No sound: the app's audio output isn't running. Choose one in Options > Audio/MIDI Settings...";
        if (audition.hostSilent)
            return "No sound from your DAW: Logic only runs a stopped instrument track while it's selected or armed. Select this track, or press Play.";
        if (audition.problem.isNotEmpty() && ! audition.waiting && audition.sounding != ticket)
            return "Couldn't play " + auditionLabel + ": " + audition.problem;
        return {};
    }

    juce::String SamplesPanel::kitMessage() const
    {
        const auto content = contentOf (browsed);
        if (content.state != ContentState::loading || browsed.isEmpty())
            return {};
        if (content.folder == juce::File())
            return "Looking for " + browsed + utf8 ("'s samples\xe2\x80\xa6");
        return "Loading " + browsed + utf8 ("'s samples\xe2\x80\xa6 ") + juce::String (juce::jlimit (0, 100, content.percent)) + " %";
    }

    juce::String SamplesPanel::statusText() const
    {
        // While the kit's samples load, that comes first: nothing plays or drags until they're in.
        if (const auto loading = kitMessage(); loading.isNotEmpty())
            return loading;
        if (message.text.isNotEmpty())
            return message.text;
        return auditionMessage();
    }

    juce::Colour SamplesPanel::statusTone() const
    {
        if (kitMessage().isNotEmpty())
            return colours::busy;
        if (message.text.isNotEmpty())
            return message.tone;
        return audition.hostSilent && refusal.isEmpty() ? colours::busy : colours::warning;
    }

    void SamplesPanel::setMessage (const juce::String& text, juce::Colour tone, int forMs)
    {
        message.text = text;
        message.tone = tone;
        message.until = forMs > 0 ? juce::jmax (static_cast<juce::uint32> (1), juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (forMs)) : 0;
    }

    juce::String SamplesPanel::bannerText() const
    {
        const auto content = contentOf (browsed);
        if (browsed.isEmpty() || content.state == ContentState::ready || content.state == ContentState::loading)
            return {};

        const auto kit = browsed;
        const auto folder = "'" + kit.toLowerCase() + "'";
        const auto mac = platform == ContentPanel::Platform::macOS;
        const auto* card = cardOf (kit);
        const auto version = juce::String (juce::jmax (1, card != nullptr ? card->contentVersion : 1));
        const auto installed = mac ? juce::String ("Install it with install-macos.sh")
                                   : "Unzip " + kit + "'s content zip into "
                                         + (platform == ContentPanel::Platform::linuxOS ? juce::String ("~/.local/share/Wubansia Audio/Wubansia/Content")
                                                                                        : utf8 ("the Content folder (Content\xe2\x80\xa6 shows it)"));
        const auto reinstalled = mac ? juce::String ("Reinstall it with install-macos.sh") : juce::String ("Unzip its content zip again");

        juce::String headline, advice;
        switch (content.state)
        {
            case ContentState::missing:
                headline = kit + "'s samples aren't installed";
                advice = installed + ", or choose the folder that holds " + folder + ".";
                break;
            case ContentState::lfsPointers:
                headline = kit + "'s samples are Git LFS pointers, not audio";
                advice = "In your clone, run git lfs pull, or choose a folder that holds the real samples.";
                break;
            case ContentState::mismatch:
                headline = kit + "'s samples don't match this version";
                advice = reinstalled + ", or choose the folder that holds " + kit + "'s content v" + version + ".";
                break;
            case ContentState::failed:
                headline = kit + "'s samples couldn't be loaded";
                advice = reinstalled + ", or choose another folder.";
                break;
            case ContentState::ready:
            case ContentState::loading:
                return {};
        }
        // The check's problem, in words, before the drawer's advice; but not where it only says what the headline
        // and the advice do: no folder holds the kit at all ("install its content, or choose the folder..."), or the
        // files are LFS pointers ("git lfs pull or install the content").
        const auto saysItAll = (content.state == ContentState::missing && content.folder == juce::File()) || content.state == ContentState::lfsPointers;
        const auto problem = saysItAll ? juce::String() : sentence (withoutCode (content.problem));
        return headline + "\n" + (problem.isNotEmpty() ? problem + " " : juce::String()) + advice;
    }

    juce::String SamplesPanel::folderText() const
    {
        return "Drags save to " + tidy (samplesFolder);
    }

    juce::String SamplesPanel::detailText() const
    {
        const auto* sound = selectedSound();
        const auto* card = cardOf (browsed);
        if (sound == nullptr || card == nullptr || sound->files.empty())
            return {};
        const auto& file = sound->files[static_cast<std::size_t> (chosenFile (*sound))];
        return sound->name + "\n" + sounds::categoryName (sound->category) + middleDot() + card->id + middleDot() + shortKey (card->key) + middleDot()
               + card->camelot + "\n" + secondsText (file.seconds) + " s" + middleDot() + (file.channels == 1 ? "mono" : "stereo") + middleDot()
               + "48 kHz 24-bit";
    }

    juce::String SamplesPanel::hintText() const
    {
        const auto* sound = selectedSound();
        const auto* card = cardOf (browsed);
        return sound != nullptr && card != nullptr ? hintFor (*sound, *card) : juce::String();
    }

    //==============================================================================================================
    void SamplesPanel::refresh()
    {
        const auto ready = kitReady();
        const auto* sound = selectedSound();
        const auto canUse = ready && sound != nullptr && ! sound->files.empty();

        refreshRows();
        refreshDetail();

        tile.setEnabled (canUse);
        tile.show (tileName(), tileFailed && canUse);
        playButton.setEnabled (canUse);
        revealButton.setEnabled (canUse);
        stopButton.setToggleState (auditioning(), juce::dontSendNotification);
        folderButton.setTooltip ("Choose where drags save their WAV files, now " + tidy (samplesFolder) + ". It's kept for you, not in projects.");

        // The rail: each category's count in the browsed kit.
        std::array<int, kSoundCategories> counts {};
        if (const auto* sounds = listOf (browsed))
            for (const auto& s : *sounds)
                ++counts[static_cast<std::size_t> (s.category)];
        for (std::size_t i = 0; i < categories.size(); ++i)
            categories[i]->show (counts[i], static_cast<SoundCategory> (i) == shownCategory && ! rows.isEmpty());

        // The banner over the list, when the kit can't play.
        const auto banner = bannerText();
        if (banner.isNotEmpty() != contentFolderButton.isVisible())
        {
            contentFolderButton.setVisible (banner.isNotEmpty());
            layOutList();
        }

        // What paint () draws of the text; repainted only when it changes.
        const auto shown = browsed + "|" + juce::String (kits.size()) + "|" + detailText() + "|" + hintText() + "|" + statusText() + "|" + banner + "|"
                           + folderText() + "|" + juce::String (rows.size()) + "|" + juce::String (list.hasKeyboardFocus (true) ? 1 : 0)
                           + juce::String (statusTone().getARGB());
        if (shown != painted)
        {
            painted = shown;
            repaint();
        }
    }

    void SamplesPanel::refreshChips()
    {
        juce::StringArray ids;
        for (const auto& card : kits)
            ids.add (card.id);

        if (ids != chipKits)
        {
            chipKits = ids;
            chips.clear();
            for (int i = 0; i < ids.size(); ++i)
            {
                auto* chip = chips.add (new KitChip (ids[i]));
                const auto& card = kits[static_cast<std::size_t> (i)];
                chip->setTooltip ("Browse " + card.id + " " + card.title + "'s sounds. The kit that plays doesn't change.");
                chip->setTitle (card.id + " " + card.title);
                chip->setExplicitFocusOrder (chipOrder + i);
                chip->onClick = [this, kit = ids[i]] { pickKit (kit); };
                addAndMakeVisible (chip);
            }
            resized();
        }

        for (auto* chip : chips)
            chip->show (chip->kit() == browsed, chip->kit() == playingKit);
    }

    void SamplesPanel::refreshRows()
    {
        const auto* sounds = listOf (browsed);
        const auto ready = kitReady();
        for (int i = 0; i < rows.size(); ++i)
        {
            if (sounds == nullptr || static_cast<std::size_t> (i) >= shownSounds.size())
                break;
            const auto& sound = (*sounds)[static_cast<std::size_t> (shownSounds[static_cast<std::size_t> (i)])];
            if (sound.files.empty())
                continue;
            const auto file = chosenFile (sound);
            const auto wholeBars = sound.bars > 0 && file == sound.defaultFile;
            const auto seconds = secondsText (sound.files[static_cast<std::size_t> (file)].seconds);

            SoundRow::Look rowLook;
            rowLook.name = sound.name;
            rowLook.length = wholeBars ? barsText (sound.bars) : seconds + " s";
            rowLook.spoken = sound.name + ", " + (wholeBars ? barsText (sound.bars) : seconds + " seconds") + (sound.endAligned ? ", ends on the beat" : "");
            rowLook.endAligned = sound.endAligned;
            rowLook.files = static_cast<int> (sound.files.size());
            rowLook.selected = i == selected;
            const auto isLast = ticket != 0 && sound.id == auditionSound && browsed == auditionKit;
            rowLook.playing = isLast && audition.sounding == ticket;
            rowLook.waiting = isLast && ! rowLook.playing && audition.waiting && audition.latest == ticket;
            rowLook.failed = sound.id == auditionSound && browsed == auditionKit && ! auditionDismissed
                          && (refusal.isNotEmpty() || (isLast && audition.latest == ticket && audition.problem.isNotEmpty() && ! audition.waiting && ! rowLook.playing));
            rowLook.dimmed = ! ready;
            rows[i]->show (rowLook);
        }
    }

    void SamplesPanel::refreshDetail()
    {
        // The File box lists the selected sound's files by their installed names.
        const auto* sound = selectedSound();
        const juce::StringArray which { browsed, sound != nullptr ? sound->id : juce::String() };
        if (which != fileBoxSound)
        {
            fileBoxSound = which;
            fileBox.clear (juce::dontSendNotification);
            if (sound != nullptr)
                for (std::size_t i = 0; i < sound->files.size(); ++i)
                    fileBox.addItem (sound->files[i].label, static_cast<int> (i) + 1);
        }
        const auto index = sound != nullptr && ! sound->files.empty() ? chosenFile (*sound) : -1;
        if (fileBox.getSelectedItemIndex() != index)
            fileBox.setSelectedItemIndex (index, juce::dontSendNotification);
        fileBox.setEnabled (sound != nullptr && sound->files.size() > 1);
    }

    //==============================================================================================================
    void SamplesPanel::resized()
    {
        auto area = getLocalBounds();
        headerArea = area.removeFromTop (headerHeight);
        footerArea = area.removeFromBottom (footerHeight);
        railArea = area.removeFromLeft (railWidth);
        listArea = area.removeFromLeft (listWidth);
        detailArea = area;

        // The header: SAMPLES, the chips, the browsed kit, then Stop and Close.
        auto header = headerArea.reduced (14, 0);
        closeButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 28));
        header.removeFromRight (4);
        stopButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 28));
        header.removeFromRight (12);
        header.removeFromLeft (86); // the engraved SAMPLES
        for (auto* chip : chips)
        {
            chip->setBounds (header.removeFromLeft (68).withSizeKeepingCentre (68, 24));
            header.removeFromLeft (5);
        }
        header.removeFromLeft (12);
        titleArea = header;

        // The rail.
        auto rail = railArea.reduced (10, 10);
        for (auto& category : categories)
        {
            category->setBounds (rail.removeFromTop (34));
            rail.removeFromTop (3);
        }

        // The list's well; the banner sits at its top while the kit can't play.
        wellArea = listArea.reduced (8, 8);
        layOutList();

        // The detail pane.
        auto detail = detailArea.reduced (16, 12);
        nameArea = detail.removeFromTop (26);
        subArea = detail.removeFromTop (18);
        detail.removeFromTop (10);
        auto fileRow = detail.removeFromTop (28);
        fileLabelArea = fileRow.removeFromLeft (40);
        fileBox.setBounds (fileRow);
        detail.removeFromTop (8);
        formatArea = detail.removeFromTop (16);
        detail.removeFromTop (4);
        hintArea = detail.removeFromTop (32);
        detail.removeFromTop (8);
        tile.setBounds (detail.removeFromTop (88));
        detail.removeFromTop (12);
        auto actions = detail.removeFromTop (30);
        playButton.setBounds (actions.removeFromLeft (112));
        actions.removeFromLeft (10);
        revealButton.setBounds (actions.removeFromLeft (172));

        layOutFooter();
    }

    void SamplesPanel::layOutList()
    {
        auto well = wellArea.reduced (4);
        bannerArea = {};
        if (contentFolderButton.isVisible())
        {
            bannerArea = well.removeFromTop (bannerHeight);
            contentFolderButton.setBounds (bannerArea.reduced (16, 14).removeFromBottom (30).removeFromLeft (236));
            well.removeFromTop (6);
        }
        viewport.setBounds (well);

        const auto height = rows.size() * SoundRow::height;
        const auto width = well.getWidth() - (height > well.getHeight() ? viewport.getScrollBarThickness() + 2 : 0);
        list.setSize (juce::jmax (0, width), height);
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, i * SoundRow::height, list.getWidth(), SoundRow::height);
    }

    void SamplesPanel::layOutFooter()
    {
        auto footer = footerArea.reduced (14, 0);
        contentButton.setBounds (footer.removeFromRight (100).withSizeKeepingCentre (100, 22));
        footer.removeFromRight (16);
        const auto textWidth = juce::GlyphArrangement::getStringWidth (uiFont (11.5f), folderText());
        folderArea = footer.removeFromLeft (juce::jmin (330, static_cast<int> (std::ceil (textWidth)) + 2));
        footer.removeFromLeft (8);
        folderButton.setBounds (footer.removeFromLeft (80).withSizeKeepingCentre (80, 22));
        footer.removeFromLeft (16);
        statusArea = footer;
    }

    void SamplesPanel::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto accent = look.accent();

        // A frosted panel over the lanes, as the install panel is, rimmed in the browsed kit's colour.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xfb21242a), bounds.getY(), juce::Colour (0xfd16181c), bounds.getBottom()));
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

        // Hairlines between the header, rail, list, detail and footer.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) headerArea.getBottom(), bounds.getWidth() - 24.0f, 1.0f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) footerArea.getY() - 1.0f, bounds.getWidth() - 24.0f, 1.0f));
        for (const auto x : { railArea.getRight(), listArea.getRight() })
            g.fillRect (juce::Rectangle<float> ((float) x, (float) railArea.getY() + 10.0f, 1.0f, (float) railArea.getHeight() - 20.0f));

        // The header: SAMPLES, and the browsed kit.
        draw::engraved (g, "Samples", headerArea.withTrimmedLeft (16).withWidth (84).toFloat(), juce::Justification::centredLeft, 11.0f, colours::textDim);
        if (const auto* card = cardOf (browsed))
        {
            juce::AttributedString title;
            title.setWordWrap (juce::AttributedString::none);
            title.setJustification (juce::Justification::centredLeft);
            title.append (card->id + " ", uiFont (12.5f, true, 0.08f), accent);
            title.append (card->title, uiFont (14.0f, true), colours::text);
            title.append (middleDot() + juce::String (card->bpm) + " BPM" + middleDot() + shortKey (card->key) + middleDot() + card->camelot, uiFont (12.5f),
                colours::textDim);
            title.draw (g, titleArea.toFloat());
        }

        // The rail's foot: how much the kit holds.
        if (const auto* sounds = listOf (browsed); sounds != nullptr && ! sounds->empty())
        {
            std::set<int> files;
            for (const auto& s : *sounds)
                for (const auto& f : s.files)
                    files.insert (f.file);
            draw::engraved (g, juce::String (sounds->size()) + " sounds" + middleDot() + juce::String ((int) files.size()) + " files",
                railArea.reduced (16, 12).removeFromBottom (14).toFloat(), juce::Justification::centredLeft, 8.5f, colours::textFaint);
        }

        // The list's well, its rim lit while it has the keyboard.
        draw::recess (g, wellArea.toFloat(), 8.0f);
        if (list.hasKeyboardFocus (true))
        {
            g.setColour (accent.withAlpha (0.5f));
            g.drawRoundedRectangle (wellArea.toFloat().reduced (0.5f), 8.0f, 1.0f);
        }
        if (rows.isEmpty())
        {
            g.setFont (uiFont (13.0f));
            g.setColour (colours::textFaint);
            g.drawText (browsed + " has no sounds in this build", viewport.getBounds().toFloat(), juce::Justification::centred, true);
        }

        // The banner: why the kit can't play, and what to do.
        if (! bannerArea.isEmpty())
        {
            const auto box = bannerArea.toFloat();
            g.setColour (colours::warning.withAlpha (0.08f));
            g.fillRoundedRectangle (box, 7.0f);
            g.setColour (colours::warning.withAlpha (0.55f));
            g.drawRoundedRectangle (box.reduced (0.5f), 7.0f, 1.0f);
            auto text = box.reduced (16.0f, 12.0f);
            const auto lines = juce::StringArray::fromLines (bannerText());
            auto top = text.removeFromTop (20.0f);
            draw::led (g, { top.getX() + 5.0f, top.getCentreY() }, 4.0f, colours::warning, 1.0f);
            g.setFont (uiFont (15.0f, true));
            g.setColour (colours::text);
            g.drawText (lines[0], top.withTrimmedLeft (17.0f), juce::Justification::centredLeft, true);
            text.removeFromTop (5.0f);
            juce::AttributedString body;
            body.append (lines.size() > 1 ? lines[1] : juce::String(), uiFont (13.0f), colours::textDim.brighter (0.2f));
            body.setLineSpacing (1.5f);
            juce::TextLayout layout;
            layout.createLayout (body, text.getWidth());
            layout.draw (g, text.withTrimmedBottom (36.0f));
        }

        // The detail pane: the name, where it comes from, File, the length and format, and the hint.
        if (const auto detail = juce::StringArray::fromLines (detailText()); detail.size() >= 3)
        {
            g.setFont (uiFont (19.0f, true));
            g.setColour (colours::text);
            g.drawText (detail[0], nameArea.toFloat(), juce::Justification::centredLeft, true);
            g.setFont (uiFont (12.0f));
            g.setColour (colours::textDim);
            g.drawText (detail[1], subArea.toFloat(), juce::Justification::centredLeft, true);
            draw::engraved (g, "File", fileLabelArea.toFloat(), juce::Justification::centredLeft, 9.5f);
            g.setFont (uiFont (12.0f));
            g.setColour (colours::textDim);
            g.drawText (detail[2], formatArea.toFloat(), juce::Justification::centredLeft, true);

            if (const auto hint = hintText(); hint.isNotEmpty())
            {
                auto area = hintArea.toFloat();
                draw::led (g, { area.getX() + 4.0f, area.getY() + 8.0f }, 2.6f, accent, 0.8f);
                g.setFont (uiFont (12.5f));
                g.setColour (colours::text.withAlpha (0.9f));
                g.drawFittedText (hint, area.withTrimmedLeft (14.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
            }
        }
        else
        {
            g.setFont (uiFont (13.0f));
            g.setColour (colours::textFaint);
            g.drawText ("Pick a sound in the list", nameArea.toFloat(), juce::Justification::centredLeft, true);
        }

        // The footer: where drags go, and the status line.
        g.setFont (uiFont (11.5f));
        g.setColour (colours::textDim);
        g.drawText (folderText(), folderArea.toFloat(), juce::Justification::centredLeft, true);
        if (const auto status = statusText(); status.isNotEmpty())
        {
            g.setFont (uiFont (11.5f, true));
            g.setColour (statusTone());
            g.drawFittedText (status, statusArea, juce::Justification::centredLeft, 2, 0.9f);
        }
    }
}
