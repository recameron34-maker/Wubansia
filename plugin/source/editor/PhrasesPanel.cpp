#include "PhrasesPanel.h"

#include "ExternalDrag.h"
#include "editor/skin/Paint.h"
#include "SamplesPanel.h" // announceToScreenReader, noSoundLine

#include "gen/phrase.h"

#include <array>
#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        // The drawer's rows inside its 1080 x 396, and the grid's margins: four 256 px tiles 8 apart are 1048 px, with
        // 16 px on the left and room for the scroll bar on the right.
        constexpr int headerHeight = 44, filterHeight = 32, gridHeight = 280, footerHeight = 40, margin = 16, filterGap = 4;

        // Focus order (Tab): the filters, the grid (and in it the selected tile's star), More, Stop, then Close.
        constexpr int filterOrder = 1, gridOrder = 20, moreOrder = 21, stopOrder = 22, closeOrder = 23;

        // The filter row: its key, its words, and the plural the More line uses.
        struct FilterWords
        {
            const char* key;
            const char* name;
            const char* plural;
        };
        constexpr std::array<FilterWords, 11> kFilters { { { "all", "All", "phrases" }, { "hook", "Hooks", "hooks" },
            { "answer", "Call & answer", "call-and-answer phrases" }, { "period", "Periods", "periods" }, { "sparse", "Sparse", "sparse phrases" },
            { "arp", "Arps", "arps" }, { "stabs", "Stabs", "stab phrases" }, { "counter", "Counter-lines", "counter-lines" },
            { "bounce", "Bounce", "bounce phrases" }, { "kit", "From the kit", "" }, { "kept", "Kept", "" } } };

        bool isKind (const juce::String& key) { return gen::phraseKindFromId (key.toStdString()).has_value(); }

        juce::String pluralOf (const juce::String& key)
        {
            for (const auto& f : kFilters)
                if (key == f.key && juce::String (f.plural).isNotEmpty())
                    return f.plural;
            return "phrases";
        }

        // "Hook", "Call & answer"...: the kind's word, or "" for the kit's own.
        juce::String kindWord (const PhraseInfo& p)
        {
            const auto kind = gen::phraseKindFromId (p.kind.toStdString());
            return kind ? juce::String (std::string (gen::phraseKindWord (*kind))) : juce::String();
        }

        bool inFilter (const PhraseInfo& p, const juce::String& key)
        {
            if (key == "all")
                return true;
            if (key == "kit")
                return p.origin == PhraseOrigin::kit;
            if (key == "kept")
                return p.kept;
            return p.kind == key;
        }

        juce::String barsText (int bars) { return juce::String (bars) + (bars == 1 ? " bar" : " bars"); }
        juce::Font filterFont (const Skin& skin) { return bodyFont (skin, 12.5f, true, 0.02f); }
        juce::Font countFont (const Skin& skin) { return bodyFont (skin, 11.5f, true); }

        const juce::String idleLine ("Click a phrase to hear it with the groove; drag it into your DAW as MIDI.");
        const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
        const juce::String enDash (juce::CharPointer_UTF8 ("\xe2\x80\x93"));
    }

    //==============================================================================================================
    PhraseFilter::PhraseFilter (const juce::String& name)
        : juce::Button (name)
    {
        setClickingTogglesState (false);
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

    void PhraseFilter::setPadding (int pixels)
    {
        padding = pixels;
        repaint();
    }

    int PhraseFilter::fitWidth() const
    {
        // As paintButton () lays it out: 1 px of rim and the padding each side, the name, 7 px, the count.
        const auto name = juce::GlyphArrangement::getStringWidth (filterFont (skinOf (*this)), getButtonText());
        const auto count = juce::GlyphArrangement::getStringWidth (countFont (skinOf (*this)), juce::String (phrases));
        return (int) std::ceil (1.0f + (float) padding + name + 7.0f + count + (float) padding + 1.0f) + 1;
    }

    void PhraseFilter::updateTitle()
    {
        setTitle (getButtonText() + ", " + juce::String (phrases) + (phrases == 1 ? " phrase" : " phrases") + (shown ? ", shown" : ""));
    }

    void PhraseFilter::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = skinOf (*this).colours.accent;
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto corner = area.getHeight() * 0.5f;
        if (shown)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.26f), area.getY(), accent.withAlpha (0.10f), area.getBottom()));
            g.fillRoundedRectangle (area, corner);
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        }
        else if (isMouseOver || hasKeyboardFocus (false))
        {
            g.setColour (juce::Colours::white.withAlpha (hasKeyboardFocus (false) ? 0.09f : 0.05f));
            g.fillRoundedRectangle (area, corner);
        }

        auto line = area.withTrimmedLeft ((float) padding).withTrimmedRight ((float) padding).translated (0.0f, isButtonDown ? 0.6f : 0.0f);
        g.setFont (filterFont (skinOf (*this)));
        g.setColour (shown ? skinOf (*this).colours.text : skinOf (*this).colours.dim);
        const auto name = getButtonText();
        g.drawText (name, line, juce::Justification::centredLeft, false);
        line.removeFromLeft (juce::GlyphArrangement::getStringWidth (filterFont (skinOf (*this)), name) + 7.0f);
        g.setFont (countFont (skinOf (*this)));
        g.setColour (shown ? accent.brighter (0.3f) : skinOf (*this).colours.faint);
        g.drawText (juce::String (phrases), line, juce::Justification::centredLeft, false);
    }

    //==============================================================================================================
    PhraseStar::PhraseStar()
        : juce::Button ("Keep")
    {
        setClickingTogglesState (false); // the Controller keeps it; the next list lights it
        setTooltip ("Keep this phrase: it stays under Kept for you, in every project");
        setWantsKeyboardFocus (false);
    }

    void PhraseStar::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = skinOf (*this).colours.accent;
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        juce::Path star;
        star.addStar (area.getCentre(), 5, area.getWidth() * 0.22f, area.getWidth() * 0.5f, 0.0f);
        if (getToggleState())
        {
            g.setColour (accent.brighter (isMouseOver ? 0.4f : 0.15f));
            g.fillPath (star);
        }
        g.setColour (getToggleState() ? accent : skinOf (*this).colours.faint.brighter (isMouseOver || hasKeyboardFocus (false) ? 0.6f : 0.0f));
        g.strokePath (star, juce::PathStrokeType (isButtonDown ? 1.6f : 1.1f));
    }

    //==============================================================================================================
    PhraseTile::PhraseTile (PhrasesPanel& o, int index)
        : owner (o), tile (index)
    {
        setComponentID ("phrase_tile_" + juce::String (index));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        star.setComponentID ("phrase_star_" + juce::String (index));
        star.onClick = [this] { owner.keep (tile); };
        addAndMakeVisible (star);
    }

    void PhraseTile::show (const PhraseInfo& info, bool isSelected, bool isFailed, bool isPlaying, bool isWaiting, int startsAtBar)
    {
        if (info == shownPhrase && isSelected == selected && isFailed == failed && isPlaying == playing && isWaiting == waiting && startsAtBar == startBar)
            return;
        if (! (info == shownPhrase))
        {
            shownPhrase = info;
            setTooltip (info.origin == PhraseOrigin::kit
                            ? "Click to hear it with the groove; drag it into your DAW. The kit's own " + info.id + "."
                            : "Click to hear it with the groove; drag it into your DAW. Seed " + juce::String (info.seed) + ".");
            star.setTitle ("Keep " + info.name);
            star.setToggleState (info.kept, juce::dontSendNotification);
        }
        selected = isSelected;
        failed = isFailed;
        playing = isPlaying;
        waiting = isWaiting;
        startBar = startsAtBar;
        star.setWantsKeyboardFocus (selected); // Tab reaches the selected tile's star only
        updateTitle();
        repaint();
    }

    void PhraseTile::updateTitle()
    {
        auto title = shownPhrase.name;
        if (shownPhrase.origin == PhraseOrigin::kit)
            title << ", from the kit, " << barsText (shownPhrase.bars);
        else
        {
            title << ", " << kindWord (shownPhrase).toLowerCase() << ", " << barsText (shownPhrase.bars);
            for (const auto& tag : shownPhrase.tags)
                title << ", " << tag;
        }
        if (playing)
            title << ", playing";
        else if (waiting && startBar > 0)
            title << ", starts at bar " << startBar;
        if (shownPhrase.kept)
            title << ", kept";
        setTitle (title);
    }

    juce::String PhraseTile::detailText() const
    {
        if (shownPhrase.origin == PhraseOrigin::kit)
            return "From the kit" + middleDot() + barsText (shownPhrase.bars);
        auto text = kindWord (shownPhrase) + middleDot() + barsText (shownPhrase.bars);
        if (! shownPhrase.tags.isEmpty())
            text << middleDot() << shownPhrase.tags.joinIntoString (", ");
        return text;
    }

    juce::String PhraseTile::badgeText() const
    {
        return shownPhrase.fresh ? "New" : "";
    }

    void PhraseTile::resized()
    {
        star.setBounds (getWidth() - 30, 8, 20, 20);
    }

    void PhraseTile::paint (juce::Graphics& g)
    {
        const auto accent = skinOf (*this).colours.accent;
        const auto active = pressed || dragging || playing;
        const auto focused = selected && getParentComponent() != nullptr && getParentComponent()->hasKeyboardFocus (false);
        const auto area = getLocalBounds().toFloat().reduced (1.5f);
        constexpr auto corner = 8.0f;

        // A dark tile, lit in the kit's colour while pressed or playing, raised while selected.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner);
        g.setGradientFill (juce::ColourGradient::vertical (active ? accent.withAlpha (playing ? 0.24f : 0.30f).overlaidWith (juce::Colour (0xff1d2025))
                                                                  : juce::Colour (hover || selected ? 0xff262a31 : 0xff1d2025),
            area.getY(), juce::Colour (0xff131518), area.getBottom()));
        g.fillRoundedRectangle (area, corner);
        const auto rim = failed ? skinOf (*this).colours.warning : accent.withAlpha (selected || hover || active || waiting ? 0.85f : 0.3f);
        g.setColour (rim);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, selected || failed || playing ? 1.4f : 1.0f);
        if (focused)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (area.expanded (0.5f), corner + 1.0f, 2.0f);
        }

        // The MIDI glyph, the name (and New), what it is, and its roll.
        auto content = area.reduced (12.0f, 8.0f);
        content.removeFromRight (22.0f); // the star
        auto top = content.removeFromTop (19.0f);
        paint::midiGlyph (g, top.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 12.0f), failed ? skinOf (*this).colours.warning : accent);
        top.removeFromLeft (8.0f);
        const auto badge = badgeText();
        if (badge.isNotEmpty())
        {
            const auto pill = top.removeFromRight (32.0f).withSizeKeepingCentre (32.0f, 15.0f);
            g.setColour (accent.withAlpha (0.85f));
            g.fillRoundedRectangle (pill, 7.5f);
            g.setColour (juce::Colour (0xff101215));
            g.setFont (bodyFont (skinOf (*this), 10.0f, true, 0.04f));
            g.drawText (badge, pill, juce::Justification::centred, false);
            top.removeFromRight (4.0f);
        }
        g.setFont (bodyFont (skinOf (*this), 14.5f, true));
        g.setColour (failed ? skinOf (*this).colours.warning : skinOf (*this).colours.text);
        g.drawFittedText (shownPhrase.name, top.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
        g.setFont (bodyFont (skinOf (*this), 11.0f));
        g.setColour (skinOf (*this).colours.dim);
        g.drawText (detailText(), content.removeFromTop (15.0f), juce::Justification::centredLeft, true);

        // The roll: each note a bar in the kit's colour, its 16ths across, its keys from low to high.
        const auto roll = content.withTrimmedTop (3.0f).withTrimmedRight (-22.0f);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (roll, 3.0f);
        if (! shownPhrase.roll.empty())
        {
            int low = 127, high = 0, last = 1;
            for (const auto& n : shownPhrase.roll)
            {
                low = juce::jmin (low, (int) n.key);
                high = juce::jmax (high, (int) n.key);
                last = juce::jmax (last, n.step + n.len16);
            }
            const auto steps = (float) juce::jmax (shownPhrase.bars * 16, last);
            const auto span = (float) juce::jmax (1, high - low);
            const auto inner = roll.reduced (3.0f, 2.0f);
            g.setColour (accent.withAlpha (playing || selected || hover ? 0.95f : 0.7f));
            for (const auto& n : shownPhrase.roll)
            {
                const auto x = inner.getX() + inner.getWidth() * (float) n.step / steps;
                const auto w = juce::jmax (1.5f, inner.getWidth() * (float) n.len16 / steps - 0.5f);
                const auto y = inner.getBottom() - 2.0f - (inner.getHeight() - 2.0f) * (float) (n.key - low) / span;
                g.fillRect (juce::Rectangle<float> (x, y, w, 2.0f));
            }
        }
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
            owner.play (tile);
    }

    std::unique_ptr<juce::AccessibilityHandler> PhraseTile::createAccessibilityHandler()
    {
        // A screen reader can't drag: its press plays or stops the phrase, as a click does.
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { owner.play (tile); }));
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
        announce = announceToScreenReader;

        for (int i = 0; i < (int) kFilters.size(); ++i)
        {
            auto* f = filters.add (new PhraseFilter (kFilters[(std::size_t) i].name));
            const juce::String key (kFilters[(std::size_t) i].key);
            f->setComponentID ("phrases_filter_" + key);
            f->setExplicitFocusOrder (filterOrder + i);
            f->onClick = [this, key] { showFilter (key); };
            addAndMakeVisible (f);
        }
        filters[9]->setTooltip ("The kit's own written phrases and its lead line, which were the chips on the drag strip");

        moreButton.setComponentID ("phrases_more");
        moreButton.setTooltip ("8 new phrases of the kind shown (one of each kind when the filter isn't a kind), written over this kit's chords. "
                               "They stay until you close the plugin; star one to keep it.");
        moreButton.onClick = [this] { more(); };
        moreButton.setExplicitFocusOrder (moreOrder);
        addAndMakeVisible (moreButton);

        stopButton.setComponentID ("phrases_stop");
        stopButton.setTooltip ("Stop the phrase playing");
        stopButton.onClick = [this] {
            controller.stopPhraseAudition();
            audition = controller.phraseAuditionState();
            refresh();
        };
        stopButton.setExplicitFocusOrder (stopOrder);
        addAndMakeVisible (stopButton);

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
        moreWaiting = false;
        moreLine.clear();
        painted.clear();
        filter = "all";
        listed = false; // the list again, from All
        take (model);
        select (tiles.isEmpty() ? -1 : 0);
        announced = statusText();
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
        audition = model.phraseAudition;
        mutedLanes = model.mutedLanes;
        content = model.content.state;
        standalone = model.standalone;
        logicHost = model.logicHost;

        // The list again only when it may have changed, and never while a drag is handed over from one of the tiles it
        // would delete: the next tick takes it.
        if ((! listed || model.phrasesGeneration != listedGeneration) && ! handingOver)
        {
            listed = true;
            listedGeneration = model.phrasesGeneration;
            list = controller.phrases();
            if (kit.id != listedKit)
            {
                // Another kit: All, and nothing of the last kit's says anything.
                listedKit = kit.id;
                filter = "all";
                moreWaiting = false;
                moreLine.clear();
                failure.clear();
                failedTile = -1;
            }
            if (moreWaiting)
            {
                int fresh = 0;
                for (const auto& p : list)
                    if (p.origin == PhraseOrigin::more && moreBefore.count (p.id) == 0)
                        ++fresh;
                if (fresh > 0)
                {
                    moreWaiting = false;
                    const auto kinds = pluralOf (moreKind);
                    moreLine = fresh >= 8 ? juce::String (fresh) + " new " + kinds + " at the end of the list."
                                          : "Only " + juce::String (fresh) + " new " + kinds + " this time.";
                }
            }
            const auto keep = selected;
            rebuildTiles();
            selected = tiles.isEmpty() ? -1 : juce::jlimit (0, tiles.size() - 1, juce::jmax (0, keep));
        }
        refresh();
    }

    void PhrasesPanel::rebuildTiles()
    {
        shownRows.clear();
        for (int i = 0; i < (int) list.size(); ++i)
            if (inFilter (list[(std::size_t) i], filter))
                shownRows.push_back (i);
        failedTile = -1;
        tiles.clear();
        for (int i = 0; i < (int) shownRows.size(); ++i)
            grid.addAndMakeVisible (tiles.add (new PhraseTile (*this, i)));
        layOutGrid();
    }

    void PhrasesPanel::focusGrid()
    {
        if (grid.isShowing() && ! tiles.isEmpty())
            grid.grabKeyboardFocus();
    }

    const PhraseInfo* PhrasesPanel::phraseAt (int tile) const
    {
        return tile >= 0 && tile < (int) shownRows.size() ? &list[(std::size_t) shownRows[(std::size_t) tile]] : nullptr;
    }

    const PhraseInfo* PhrasesPanel::auditioned() const
    {
        if (audition.latest == 0 || audition.id.isEmpty() || ! (audition.waiting || (audition.sounding != 0 && audition.sounding == audition.latest)))
            return nullptr;
        for (const auto& p : list)
            if (p.id == audition.id)
                return &p;
        return nullptr;
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
            play (selected);
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

    void PhrasesPanel::play (int tile)
    {
        const auto* p = phraseAt (tile);
        if (p == nullptr)
            return;
        select (tile);
        moreLine.clear();
        const auto* now = auditioned();
        if (now != nullptr && now->id == p->id)
            controller.stopPhraseAudition(); // a click on the playing (or waiting) tile stops it
        else
            controller.auditionPhrase (p->id);
        audition = controller.phraseAuditionState();
        refresh();
    }

    void PhrasesPanel::keep (int tile)
    {
        if (const auto* p = phraseAt (tile))
            controller.setPhraseKept (p->id, ! p->kept);
    }

    void PhrasesPanel::showFilter (const juce::String& key)
    {
        if (key == filter)
            return;
        filter = key;
        rebuildTiles();
        select (tiles.isEmpty() ? -1 : 0);
        viewport.setViewPosition (0, 0);
    }

    void PhrasesPanel::more()
    {
        // A kind's filter asks for that kind; All, From the kit and Kept ask for one of each and show All.
        const auto kind = isKind (filter) ? filter : juce::String();
        if (! controller.morePhrases (kind))
            return;
        moreWaiting = true;
        moreKind = kind;
        moreBefore.clear();
        for (const auto& p : list)
            moreBefore.insert (p.id);
        moreLine.clear();
        if (kind.isEmpty())
            showFilter ("all");
        refresh();
    }

    bool PhrasesPanel::dragOut (int tile, juce::Component& source)
    {
        const auto file = write (tile);
        if (file == juce::File() || startDrag == nullptr)
            return false;
        const juce::ScopedValueSetter<bool> handing (handingOver, true);
        return startDrag (file, source);
    }

    juce::File PhrasesPanel::write (int tile)
    {
        const auto* phrase = phraseAt (tile);
        if (phrase == nullptr)
            return {};
        DragRequest request;
        request.what = phrase->origin == PhraseOrigin::kit ? DragWhat::phrase : DragWhat::generatedPhrase;
        request.phraseFile = phrase->id;
        const auto file = controller.writeDragFile (request);
        if (file == juce::File())
        {
            // writeDragFile () gives no reason: it writes the phrase's file into the drag folder, or nothing.
            failedTile = tile;
            failure = "Couldn't write " + phrase->name + ": its MIDI file couldn't be saved in Wubansia's drag folder.";
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
        auto line = kit.id + " " + kit.title;
        if (! kit.phraseChords.isEmpty())
            line << middleDot() << "over " << kit.phraseChords.joinIntoString (" " + enDash + " ");
        return line + middleDot() + juce::String (kit.bpm) + " BPM" + middleDot() + shortKey (kit.key) + " " + kit.camelot;
    }

    juce::String PhrasesPanel::statusText() const
    {
        if (failure.isNotEmpty())
            return failure;
        if (audition.hostSilent && audition.latest != 0)
            return noSoundLine (standalone, logicHost);
        const auto* now = auditioned();
        if (now != nullptr && (now->lanes & mutedLanes) != 0)
        {
            // A clip's lanes play through their mix channels: a muted one is silent (ADR 0031).
            for (int lane = 0; lane < engine::kLanes; ++lane)
                if ((now->lanes & mutedLanes & engine::laneBit (static_cast<engine::Lane> (lane))) != 0)
                    return kit.laneLabels[(std::size_t) lane] + " is muted: unmute it to hear " + now->name + ".";
        }
        if (content != ContentState::ready && content != ContentState::loading)
            return "The kit's samples aren't installed: the phrases drag out but play silent.";
        if (moreWaiting)
            return "Making 8 more" + ellipsis;
        if (moreLine.isNotEmpty())
            return moreLine;
        if (now != nullptr && audition.waiting)
            return audition.startsAtBar > 0 ? now->name + " starts at bar " + juce::String (audition.startsAtBar) + ellipsis
                                            : now->name + " starts at the next chord" + ellipsis;
        if (now != nullptr)
        {
            juce::StringArray lanes;
            for (int lane = 0; lane < engine::kLanes; ++lane)
                if ((now->lanes & engine::laneBit (static_cast<engine::Lane> (lane))) != 0 && kit.laneLabels[(std::size_t) lane].isNotEmpty())
                    lanes.add (kit.laneLabels[(std::size_t) lane]);
            auto line = "Playing " + now->name + " on " + now->sound;
            if (! lanes.isEmpty())
                line << ", in place of the kit's " << lanes.joinIntoString (" and ");
            line << ".";
            if (audition.startedTransport)
                line << " Wubansia started playing for it; Stop ends both.";
            return line;
        }
        return idleLine;
    }

    juce::String PhrasesPanel::synthsText() const
    {
        return juce::String ("For my synths: ") + (forSynths ? "on" : "off");
    }

    void PhrasesPanel::refresh()
    {
        const auto* now = auditioned();
        for (int i = 0; i < tiles.size(); ++i)
        {
            const auto& p = list[(std::size_t) shownRows[(std::size_t) i]];
            const bool mine = now != nullptr && now->id == p.id;
            tiles[i]->show (p, i == selected, i == failedTile, mine && ! audition.waiting, mine && audition.waiting, audition.startsAtBar);
        }
        for (int f = 0; f < filters.size(); ++f)
        {
            const juce::String key (kFilters[(std::size_t) f].key);
            int n = 0;
            for (const auto& p : list)
                n += inFilter (p, key) ? 1 : 0;
            filters[f]->show (n, key == filter);
        }
        layOutFilters();

        // Changes are said to a screen reader, as the Samples drawer says them; the idle line never is.
        const auto status = statusText();
        if (status != announced)
        {
            announced = status;
            if (status != idleLine && isVisible() && announce != nullptr)
                announce (status);
        }

        // What paint () draws of the text; repainted only when it changes.
        const auto shown = kitLine() + "|" + status + "|" + synthsText() + "|" + juce::String (tiles.size()) + "|" + juce::String (skinOf (*this).colours.accent.getARGB());
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

        // The header: PHRASES, the kit line, More, Stop, Close.
        auto header = headerArea.reduced (margin - 2, 0);
        closeButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 28));
        header.removeFromRight (6);
        stopButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 28));
        header.removeFromRight (6);
        moreButton.setBounds (header.removeFromRight (64).withSizeKeepingCentre (64, 26));
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

    void PhrasesPanel::layOutFilters()
    {
        // Left to right, each with its words in full: the padding narrows (14 px down to 4) until the row fits.
        const auto room = filterArea.getWidth() - 2 * margin;
        for (int padding = 14; padding >= 4; --padding)
        {
            int total = 0;
            for (auto* f : filters)
            {
                f->setPadding (padding);
                total += f->fitWidth() + filterGap;
            }
            if (total - filterGap <= room)
                break;
        }
        auto x = filterArea.getX() + margin;
        for (auto* f : filters)
        {
            const auto w = f->fitWidth();
            f->setBounds (juce::Rectangle<int> (x, filterArea.getY(), w, filterArea.getHeight()).withSizeKeepingCentre (w, 24));
            x += w + filterGap;
        }
    }

    void PhrasesPanel::layOutGrid()
    {
        const auto rows = (tiles.size() + columns - 1) / columns;
        grid.setSize (columns * PhraseTile::width + (columns - 1) * gap, juce::jmax (0, rows * PhraseTile::height + (rows - 1) * gap));
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds ((i % columns) * (PhraseTile::width + gap), (i / columns) * (PhraseTile::height + gap), PhraseTile::width, PhraseTile::height);
    }

    std::vector<DrawnText> PhrasesPanel::drawnText() const
    {
        // The status line (run-time words: the body face, fallback on) and the For my synths note, as paint () draws them.
        std::vector<DrawnText> out { fitText (skinOf (*this).type.rowLabel, getTitle(), headerArea.withTrimmedLeft (margin).withWidth (84).toFloat()) };
        const auto font = bodyFont (skinOf (*this), 11.5f, true);
        for (const auto& [text, area] : { std::pair { statusText(), statusArea.toFloat() }, std::pair { synthsText(), synthsArea.toFloat() } })
        {
            if (text.isEmpty())
                continue;
            DrawnText t;
            t.text = text;
            t.font = font;
            t.area = area;
            t.styledPx = t.minPx = font.getHeightInPoints() * 0.9f; // the status line may squeeze to 0.9 on its two lines
            if (juce::GlyphArrangement::getStringWidth (font, text) > area.getWidth())
                t.area = area.withWidth (area.getWidth() * 2.0f); // its two lines
            out.push_back (t);
        }
        return out;
    }

    void PhrasesPanel::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto accent = skinOf (*this).colours.accent;

        // The drawer's frame in the look (ADR 0032).
        paint::drawer (g, skinOf (*this), bounds);

        // Hairlines under the header and over the footer.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) headerArea.getBottom(), bounds.getWidth() - 24.0f, 1.0f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 12.0f, (float) footerArea.getY() - 1.0f, bounds.getWidth() - 24.0f, 1.0f));

        // The header: PHRASES, then the kit line.
        paint::label (g, skinOf (*this), getTitle(), headerArea.withTrimmedLeft (margin).withWidth (84).toFloat(), juce::Justification::centredLeft, 11.0f, skinOf (*this).colours.dim);
        if (kit.id.isNotEmpty())
        {
            juce::AttributedString title;
            title.setWordWrap (juce::AttributedString::none);
            title.setJustification (juce::Justification::centredLeft);
            title.append (kit.id + " ", bodyFont (skinOf (*this), 12.5f, true, 0.08f), accent);
            title.append (kit.title, bodyFont (skinOf (*this), 14.0f, true), skinOf (*this).colours.text);
            auto rest = juce::String();
            if (! kit.phraseChords.isEmpty())
                rest << middleDot() << "over " << kit.phraseChords.joinIntoString (" " + enDash + " ");
            rest << middleDot() << juce::String (kit.bpm) << " BPM" << middleDot() << shortKey (kit.key) << " " << kit.camelot;
            title.append (rest, bodyFont (skinOf (*this), 12.5f), skinOf (*this).colours.dim);
            title.draw (g, titleArea.toFloat());
        }

        if (tiles.isEmpty())
        {
            g.setFont (bodyFont (skinOf (*this), 13.0f));
            g.setColour (skinOf (*this).colours.faint);
            const auto empty = filter == "kept" ? juce::String ("Star a phrase to keep it here, in every project")
                               : list.empty()   ? kit.id + " has no phrases in this build"
                                                : juce::String ("None of this kind here yet: More writes 8");
            g.drawText (empty, viewport.getBounds().toFloat(), juce::Justification::centred, true);
        }

        // The footer: the status line, a failure in the warning colour, and For my synths.
        g.setFont (bodyFont (skinOf (*this), 11.5f, true));
        g.setColour (failure.isNotEmpty() ? skinOf (*this).colours.warning : skinOf (*this).colours.dim);
        g.drawFittedText (statusText(), statusArea, juce::Justification::centredLeft, 2, 0.9f);
        g.setFont (bodyFont (skinOf (*this), 11.5f, true));
        g.setColour (forSynths ? skinOf (*this).colours.text.withAlpha (0.85f) : skinOf (*this).colours.dim);
        g.drawText (synthsText(), synthsArea.toFloat(), juce::Justification::centredRight, false);
    }
}
