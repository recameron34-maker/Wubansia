#include "PhrasesPanel.h"
#include "ExternalDrag.h"
#include "SamplesPanel.h" // announceToScreenReader

#include "dragout/DragFiles.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        // The drawer's rows inside its 1080 x 396, and the grid's margins: four 256 px tiles 8 apart are 1048 px, with
        // 16 px on the left and room for the scroll bar on the right.
        constexpr int headerHeight = 44, filterHeight = 32, gridHeight = 280, footerHeight = 40, margin = 16;

        // Focus order (Tab): the filter, the grid, then Close.
        constexpr int filterOrder = 1, gridOrder = 2, closeOrder = 3;

        juce::String barsText (int bars) { return juce::String (bars) + (bars == 1 ? " bar" : " bars"); }

        juce::Font filterFont() { return uiFont (12.5f, true, 0.02f); }
        juce::Font countFont() { return uiFont (11.5f, true); }
    }

    //==============================================================================================================
    PhraseFilter::PhraseFilter (const juce::String& name)
        : juce::Button (name)
    {
        setClickingTogglesState (false); // the only filter: always shown
        updateTitle();
    }

    void PhraseFilter::show (int numPhrases, bool isShown)
    {
        if (numPhrases == phrases && isShown == shown)
            return;
        phrases = numPhrases;
        shown = isShown;
        updateTitle();
        repaint();
    }

    juce::String PhraseFilter::label() const
    {
        return getButtonText() + " " + juce::String (phrases);
    }

    int PhraseFilter::fitWidth() const
    {
        // As paintButton () lays it out: 1 px of rim and 14 of padding each side, the name, 7 px, the count.
        const auto name = juce::GlyphArrangement::getStringWidth (filterFont(), getButtonText());
        const auto count = juce::GlyphArrangement::getStringWidth (countFont(), juce::String (phrases));
        return (int) std::ceil (1.0f + 14.0f + name + 7.0f + count + 14.0f + 1.0f) + 1;
    }

    void PhraseFilter::updateTitle()
    {
        setTitle (getButtonText() + ", " + juce::String (phrases) + (phrases == 1 ? " phrase" : " phrases") + (shown ? ", shown" : ""));
    }

    void PhraseFilter::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = accentOf (*this);
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto corner = area.getHeight() * 0.5f;
        if (shown)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.26f), area.getY(), accent.withAlpha (0.10f), area.getBottom()));
            g.fillRoundedRectangle (area, corner);
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        }
        else if (isMouseOver)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (area, corner);
        }

        auto line = area.withTrimmedLeft (14.0f).withTrimmedRight (14.0f).translated (0.0f, isButtonDown ? 0.6f : 0.0f);
        g.setFont (filterFont());
        g.setColour (shown ? colours::text : colours::textDim);
        const auto name = getButtonText();
        g.drawText (name, line, juce::Justification::centredLeft, false);
        line.removeFromLeft (juce::GlyphArrangement::getStringWidth (filterFont(), name) + 7.0f);
        g.setFont (countFont());
        g.setColour (shown ? accent.brighter (0.3f) : colours::textFaint);
        g.drawText (juce::String (phrases), line, juce::Justification::centredLeft, false);
    }

    //==============================================================================================================
    PhraseTile::PhraseTile (PhrasesPanel& o, int index)
        : owner (o), tile (index)
    {
        setComponentID ("phrase_tile_" + juce::String (index));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void PhraseTile::show (const PhraseInfo& info, bool isSelected, bool isFailed)
    {
        if (info == shownPhrase && isSelected == selected && isFailed == failed)
            return;
        if (! (info == shownPhrase))
        {
            shownPhrase = info;
            setTitle (info.name + ", from the kit, " + barsText (info.bars));
            setTooltip ("Drag it into your DAW as MIDI; click to show the file. The kit's own " + info.id + ".");
        }
        selected = isSelected;
        failed = isFailed;
        repaint();
    }

    juce::String PhraseTile::detailText() const
    {
        return "From the kit" + middleDot() + barsText (shownPhrase.bars);
    }

    void PhraseTile::paint (juce::Graphics& g)
    {
        const auto accent = accentOf (*this);
        const auto active = pressed || dragging;
        const auto focused = selected && getParentComponent() != nullptr && getParentComponent()->hasKeyboardFocus (false);
        const auto area = getLocalBounds().toFloat().reduced (1.5f);
        constexpr auto corner = 8.0f;

        // A dark tile, lit in the kit's colour while pressed, raised while selected.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner);
        g.setGradientFill (juce::ColourGradient::vertical (active ? accent.withAlpha (0.30f).overlaidWith (juce::Colour (0xff1d2025))
                                                                  : juce::Colour (hover || selected ? 0xff262a31 : 0xff1d2025),
            area.getY(), juce::Colour (0xff131518), area.getBottom()));
        g.fillRoundedRectangle (area, corner);
        const auto rim = failed ? colours::warning : accent.withAlpha (selected || hover || active ? 0.85f : 0.3f);
        g.setColour (rim);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, selected || failed ? 1.4f : 1.0f);
        if (focused)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (area.expanded (0.5f), corner + 1.0f, 2.0f);
        }

        // The MIDI glyph, the name and where it comes from, and a grip that says "drag me".
        auto content = area.reduced (14.0f, 12.0f);
        draw::midiGlyph (g, content.removeFromLeft (16.0f).withSizeKeepingCentre (13.0f, 13.0f), failed ? colours::warning : accent);
        content.removeFromLeft (12.0f);
        draw::grip (g, content.removeFromRight (12.0f).withSizeKeepingCentre (7.0f, 13.0f), colours::textFaint.withAlpha (hover || selected ? 1.0f : 0.55f));
        content.removeFromRight (8.0f);
        g.setFont (uiFont (14.5f, true));
        g.setColour (failed ? colours::warning : colours::text);
        g.drawFittedText (shownPhrase.name, content.removeFromTop (content.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.85f);
        content.removeFromTop (3.0f);
        g.setFont (uiFont (11.5f));
        g.setColour (colours::textDim);
        g.drawText (detailText(), content, juce::Justification::topLeft, true);
    }

    void PhraseTile::mouseEnter (const juce::MouseEvent&)
    {
        hover = true;
        repaint();
    }

    void PhraseTile::mouseExit (const juce::MouseEvent&)
    {
        hover = false;
        repaint();
    }

    void PhraseTile::mouseDown (const juce::MouseEvent& e)
    {
        pressed = isEnabled() && ! e.mods.isPopupMenu();
        dragging = false;
        if (pressed)
            owner.select (tile);
        repaint();
    }

    void PhraseTile::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressed || dragging || e.getDistanceFromDragStart() < 4)
            return;
        dragging = true;
        repaint();
        owner.dragOut (tile, *this); // the OS runs the drag from here (on macOS, inside this mouse event)
    }

    void PhraseTile::mouseUp (const juce::MouseEvent& e)
    {
        const auto click = pressed && ! dragging && e.mouseWasClicked();
        pressed = dragging = false;
        repaint();
        if (click)
            owner.reveal (tile);
    }

    std::unique_ptr<juce::AccessibilityHandler> PhraseTile::createAccessibilityHandler()
    {
        // A screen reader can't drag: its press shows the file, as a click does.
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { owner.reveal (tile); }));
    }

    //==============================================================================================================
    PhraseGrid::PhraseGrid (PhrasesPanel& o)
        : owner (o)
    {
        setComponentID ("phrase_grid");
        setTitle ("Phrases");
        setWantsKeyboardFocus (true);
    }

    bool PhraseGrid::keyPressed (const juce::KeyPress& key)
    {
        return owner.gridKey (key);
    }

    void PhraseGrid::focusGained (FocusChangeType)
    {
        repaint(); // the selected tile's focus ring
    }

    void PhraseGrid::focusLost (FocusChangeType)
    {
        repaint();
    }

    std::unique_ptr<juce::AccessibilityHandler> PhraseGrid::createAccessibilityHandler()
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
    }

    //==============================================================================================================
    PhrasesPanel::PhrasesPanel (Controller& c)
        : controller (c)
    {
        setComponentID ("phrases_drawer");
        setTitle ("Phrases");

        // The functions themselves, not lambdas around them: a test checks each default is the one (DragOutTests).
        startDrag = startExternalDrag;
        revealFile = dragout::revealToUser;
        announce = announceToScreenReader;

        fromKit.setComponentID ("phrases_filter_kit");
        fromKit.setTooltip ("The kit's own written phrases and its lead line, which were the chips on the drag strip");
        fromKit.setExplicitFocusOrder (filterOrder);
        addAndMakeVisible (fromKit);

        closeButton.setComponentID ("phrases_close");
        closeButton.setTooltip ("Close the drawer; the Phrases button opens it again");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };
        closeButton.setExplicitFocusOrder (closeOrder);
        addAndMakeVisible (closeButton);

        viewport.setViewedComponent (&grid, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.setWantsKeyboardFocus (false);
        viewport.setExplicitFocusOrder (gridOrder); // the grid's place, on the drawer's own child: JUCE orders siblings only
        addAndMakeVisible (viewport);
    }

    //==============================================================================================================
    void PhrasesPanel::open (const EditorModel& model)
    {
        failure.clear();
        failedTile = -1;
        painted.clear();
        take (model);
        select (tiles.isEmpty() ? -1 : 0);
        if (announce != nullptr)
            announce ("Phrases");
    }

    void PhrasesPanel::update (const EditorModel& model)
    {
        if (isVisible())
            take (model);
    }

    void PhrasesPanel::take (const EditorModel& model)
    {
        kit = model.kit;
        forSynths = model.dragForSynths;

        // The list again only when it may have changed (a kit change), and never while a drag is handed over from one of
        // the tiles it would delete: the next tick takes it.
        if ((! listed || model.phrasesGeneration != listedGeneration) && ! handingOver)
        {
            listed = true;
            listedGeneration = model.phrasesGeneration;
            list = controller.phrases();
            failure.clear();
            failedTile = -1;
            rebuildTiles();
            selected = tiles.isEmpty() ? -1 : 0;
        }
        refresh();
    }

    void PhrasesPanel::rebuildTiles()
    {
        tiles.clear();
        for (int i = 0; i < (int) list.size(); ++i)
            grid.addAndMakeVisible (tiles.add (new PhraseTile (*this, i)));
        layOutGrid();
    }

    void PhrasesPanel::focusGrid()
    {
        if (grid.isShowing() && ! tiles.isEmpty())
            grid.grabKeyboardFocus();
    }

    //==============================================================================================================
    void PhrasesPanel::select (int tile)
    {
        selected = tile >= 0 && tile < tiles.size() ? tile : -1;
        if (selected >= 0)
        {
            // In view: the viewport scrolls the selected row in.
            const auto* shown = tiles[selected];
            const auto top = viewport.getViewPositionY();
            if (shown->getY() < top)
                viewport.setViewPosition (0, shown->getY());
            else if (shown->getBottom() > top + viewport.getViewHeight())
                viewport.setViewPosition (0, shown->getBottom() - viewport.getViewHeight());
        }
        refresh();
    }

    bool PhrasesPanel::gridKey (const juce::KeyPress& key)
    {
        const auto count = tiles.size();
        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey || key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            if (count == 0)
                return false;
            auto next = juce::jmax (0, selected);
            if (selected < 0)
                next = 0; // the first arrow lands on the first tile
            else if (key == juce::KeyPress::leftKey)
                next = juce::jmax (0, selected - 1);
            else if (key == juce::KeyPress::rightKey)
                next = juce::jmin (count - 1, selected + 1);
            else if (key == juce::KeyPress::upKey)
                next = selected >= columns ? selected - columns : selected;
            else if (selected / columns < (count - 1) / columns)
                next = juce::jmin (count - 1, selected + columns); // the next row's tile below, or its last
            select (next);
            if (announce != nullptr)
                announce (tiles[selected]->getTitle()); // the grid keeps the focus, so nothing else names the tile
            return true; // at an edge: nothing to do, but the key was the grid's
        }
        if (key == juce::KeyPress::returnKey && selected >= 0 && selected < count)
        {
            reveal (selected);
            return true;
        }
        return false; // Escape goes on to the drawer; Space, letters, shortcuts and the rest to the host
    }

    bool PhrasesPanel::keyPressed (const juce::KeyPress& key)
    {
        if (key != juce::KeyPress::escapeKey)
            return false;
        if (onClose != nullptr)
            onClose();
        return true;
    }

    bool PhrasesPanel::dragOut (int tile, juce::Component& source)
    {
        const auto file = write (tile);
        if (file == juce::File() || startDrag == nullptr)
            return false;
        const juce::ScopedValueSetter<bool> handing (handingOver, true);
        return startDrag (file, source);
    }

    void PhrasesPanel::reveal (int tile)
    {
        select (tile);
        const auto file = write (tile);
        if (file != juce::File() && revealFile != nullptr)
            revealFile (file);
    }

    juce::File PhrasesPanel::write (int tile)
    {
        if (tile < 0 || tile >= (int) list.size())
            return {};
        const auto& phrase = list[(std::size_t) tile];
        DragRequest request;
        request.what = DragWhat::phrase;
        request.phraseFile = phrase.id;
        const auto file = controller.writeDragFile (request);
        if (file == juce::File())
        {
            // writeDragFile () gives no reason: it writes the kit's own file into the drag folder, or nothing.
            failedTile = tile;
            failure = "Couldn't write " + phrase.name + ": its MIDI file couldn't be saved in Wubansia's drag folder.";
            if (announce != nullptr)
                announce (failure);
        }
        else
        {
            failedTile = -1;
            failure.clear();
        }
        refresh();
        return file;
    }

    //==============================================================================================================
    juce::String PhrasesPanel::kitLine() const
    {
        if (kit.id.isEmpty())
            return {};
        return kit.id + " " + kit.title + middleDot() + juce::String (kit.bpm) + " BPM" + middleDot() + shortKey (kit.key) + " " + kit.camelot;
    }

    juce::String PhrasesPanel::statusText() const
    {
        return failure.isNotEmpty() ? failure : juce::String ("Drag a phrase into your DAW as MIDI, or click it to show the file.");
    }

    juce::String PhrasesPanel::synthsText() const
    {
        return juce::String ("For my synths: ") + (forSynths ? "on" : "off");
    }

    void PhrasesPanel::refresh()
    {
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->show (list[(std::size_t) i], i == selected, i == failedTile);
        fromKit.show (tiles.size(), true);
        fromKit.setBounds (filterArea.withTrimmedLeft (margin).withWidth (fromKit.fitWidth()).withSizeKeepingCentre (fromKit.fitWidth(), 24));

        // What paint () draws of the text; repainted only when it changes.
        const auto shown = kitLine() + "|" + statusText() + "|" + synthsText() + "|" + juce::String (tiles.size()) + "|"
                           + juce::String (accentOf (*this).getARGB());
        if (shown != painted)
        {
            painted = shown;
            repaint();
        }
    }

    //==============================================================================================================
    void PhrasesPanel::resized()
    {
        auto area = getLocalBounds();
        headerArea = area.removeFromTop (headerHeight);
        filterArea = area.removeFromTop (filterHeight);
        gridArea = area.removeFromTop (gridHeight);
        footerArea = area.removeFromTop (footerHeight);

        // The header: PHRASES, the kit line, Close.
        auto header = headerArea.reduced (margin - 2, 0);
        closeButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 28));
        header.removeFromRight (12);
        header.removeFromLeft (86); // the engraved PHRASES
        titleArea = header;

        // The grid: its tiles from the left margin, the scroll bar right of them.
        const auto tilesWidth = columns * PhraseTile::width + (columns - 1) * gap;
        viewport.setBounds (gridArea.getX() + margin, gridArea.getY(), tilesWidth + viewport.getScrollBarThickness() + 2, gridArea.getHeight() - gap);
        layOutGrid();

        // The footer: the status line, and For my synths on the right.
        auto footer = footerArea.reduced (margin, 0);
        synthsArea = footer.removeFromRight (150);
        footer.removeFromRight (16);
        statusArea = footer;
        refresh();
    }

    void PhrasesPanel::layOutGrid()
    {
        const auto rows = (tiles.size() + columns - 1) / columns;
        grid.setSize (columns * PhraseTile::width + (columns - 1) * gap, juce::jmax (0, rows * PhraseTile::height + (rows - 1) * gap));
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds ((i % columns) * (PhraseTile::width + gap), (i / columns) * (PhraseTile::height + gap), PhraseTile::width, PhraseTile::height);
    }

    void PhrasesPanel::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto accent = accentOf (*this);

        // A frosted panel over the lanes, as the Samples drawer is, rimmed in the kit's colour.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xfb21242a), bounds.getY(), juce::Colour (0xfd16181c), bounds.getBottom()));
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

        // Hairlines under the header and over the footer.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) headerArea.getBottom(), bounds.getWidth() - 24.0f, 1.0f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) footerArea.getY() - 1.0f, bounds.getWidth() - 24.0f, 1.0f));

        // The header: PHRASES, then the kit line.
        draw::engraved (g, getTitle(), headerArea.withTrimmedLeft (margin).withWidth (84).toFloat(), juce::Justification::centredLeft, 11.0f, colours::textDim);
        if (kit.id.isNotEmpty())
        {
            juce::AttributedString title;
            title.setWordWrap (juce::AttributedString::none);
            title.setJustification (juce::Justification::centredLeft);
            title.append (kit.id + " ", uiFont (12.5f, true, 0.08f), accent);
            title.append (kit.title, uiFont (14.0f, true), colours::text);
            title.append (middleDot() + juce::String (kit.bpm) + " BPM" + middleDot() + shortKey (kit.key) + " " + kit.camelot, uiFont (12.5f), colours::textDim);
            title.draw (g, titleArea.toFloat());
        }

        if (tiles.isEmpty())
        {
            g.setFont (uiFont (13.0f));
            g.setColour (colours::textFaint);
            g.drawText (kit.id + " has no phrases in this build", viewport.getBounds().toFloat(), juce::Justification::centred, true);
        }

        // The footer: the status line, the failure in the warning colour, and For my synths.
        g.setFont (uiFont (11.5f, true));
        g.setColour (failure.isNotEmpty() ? colours::warning : colours::textDim);
        g.drawFittedText (statusText(), statusArea, juce::Justification::centredLeft, 2, 0.9f);
        g.setFont (uiFont (11.5f, true));
        g.setColour (forSynths ? colours::text.withAlpha (0.85f) : colours::textDim);
        g.drawText (synthsText(), synthsArea.toFloat(), juce::Justification::centredRight, false);
    }

    // Stubs (step `phrases`, part B, commit 1).
    juce::String PhraseTile::badgeText() const { return {}; }
    juce::String PhrasesPanel::shownFilter() const { return "kit"; }
}
