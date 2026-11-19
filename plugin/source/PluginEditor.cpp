#include "PluginEditor.h"
#include "ParameterIds.h"
#include "editor/ChordName.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"
#include "services/PatternView.h"

#include <atomic>
#include <map>

namespace wubansia
{
    namespace
    {
        // The zoom the last editor used, so a new window opens the way the person left the last one.
        std::atomic<float> lastZoom { 1.0f };

        bool isProblem (ContentState state)
        {
            return state == ContentState::missing || state == ContentState::lfsPointers || state == ContentState::mismatch
                   || state == ContentState::failed;
        }
    }

    PluginEditor::PluginEditor (juce::AudioProcessor& audioProcessor, Controller& c)
        : AudioProcessorEditor (&audioProcessor),
          controller (c),
          standalone (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
          logicHost (! standalone && juce::PluginHostType().isLogic()),
          kitPanel (controller),
          transport (controller, standalone),
          modePanel (controller),
          abPanel(),
          dragStrip (controller),
          contentPanel (controller),
          samplesPanel (controller),
          phrasesPanel (controller)
    {
        setLookAndFeel (&lookAndFeel);
        setOpaque (true);
        setTitle ("Wubansia");

        backdrop.setBufferedToImage (true);
        backdrop.setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (backdrop);

        // The Samples button opens the Samples drawer (ADR 0024); the install panel is behind its Content... button.
        samplesButton.setComponentID ("samples");
        samplesButton.setTooltip ("Browse every kit's sounds, hear them, and drag them into your DAW as WAV files");
        samplesButton.setClickingTogglesState (false);
        samplesButton.onClick = [this] { showSamplesPanel (! samplesPanel.isVisible()); };

        // The Phrases button opens the Phrases drawer (ADR 0031) where the Samples drawer sits; the two never show together.
        phrasesButton.setComponentID ("phrases");
        phrasesButton.setTooltip ("Browse 8-bar phrases written over this kit's chords, hear them with the groove, and drag them into your DAW as MIDI");
        phrasesButton.setClickingTogglesState (false);
        phrasesButton.onClick = [this] { showPhrasesPanel (! phrasesPanel.isVisible()); };
        phrasesButton.setRole (editor::LedButton::Role::headerButton);
        samplesButton.setRole (editor::LedButton::Role::headerButton);

        zoomBox.setComponentID ("zoom");
        zoomBox.setTooltip ("Window size");
        for (const auto percent : zoomPercents)
            zoomBox.addItem (juce::String (percent) + " %", percent);
        zoomBox.onChange = [this] { setZoom ((float) zoomBox.getSelectedId() / 100.0f); };

        for (int i = 0; i < engine::kLanes; ++i)
            lanes[(std::size_t) i] = std::make_unique<editor::LaneStrip> (static_cast<engine::Lane> (i), controller);

        contentPanel.onClose = [this] { showContentPanel (false); };
        samplesPanel.onClose = [this] { showSamplesPanel (false); };
        samplesPanel.onShowContent = [this] { showContentPanel (true); };
        samplesPanel.chooseContentFolder = [this] { contentPanel.chooseContentFolder(); };
        phrasesPanel.onClose = [this] { showPhrasesPanel (false); };

        for (auto* component : std::initializer_list<juce::Component*> { &kitPanel, &phrasesButton, &samplesButton, &zoomBox, &lcd, &transport, &modePanel, &sectionStrip })
            content.addAndMakeVisible (component);
        for (auto& lane : lanes)
            content.addAndMakeVisible (*lane);
        for (auto* component : std::initializer_list<juce::Component*> { &abPanel, &fxPanel, &dragStrip })
            content.addAndMakeVisible (component);
        content.addChildComponent (contentPanel);
        content.addChildComponent (samplesPanel);
        content.addChildComponent (phrasesPanel);
        content.addAndMakeVisible (focusRing);

        // Tab's order, the same in every look (ADR 0032): the pills, Phrases, Samples, the zoom, the transport, the mode
        // panel, each lane's fader and mute, the A/B switches, the FX, then the drag strip; the drawers after.
        int order = 1;
        for (auto* component : std::initializer_list<juce::Component*> { &kitPanel, &phrasesButton, &samplesButton, &zoomBox, &transport, &modePanel })
            component->setExplicitFocusOrder (order++);
        for (auto& lane : lanes)
            lane->setExplicitFocusOrder (order++);
        for (auto* component : std::initializer_list<juce::Component*> { &abPanel, &fxPanel, &dragStrip, &samplesPanel, &phrasesPanel, &contentPanel })
            component->setExplicitFocusOrder (order++);
        addAndMakeVisible (content);

        bindParameters (audioProcessor);
        layOut();

        zoom = juce::jlimit (1.0f, 2.0f, lastZoom.load());
        zoomBox.setSelectedId (juce::roundToInt (zoom * 100.0f), juce::dontSendNotification);
        setSize (juce::roundToInt ((float) baseWidth * zoom), juce::roundToInt ((float) baseHeight * zoom));

        tick();
        startTimerHz (timerHz);
    }

    PluginEditor::~PluginEditor()
    {
        stopTimer();
        controller.setScopeOn (false); // nobody reads the scope once the window is gone
        if (samplesPanel.isVisible())
            controller.stopAudition(); // the drawer's sound ends with the window it plays from
        if (phrasesPanel.isVisible())
            controller.stopPhraseAudition(); // and the Phrases drawer's, with the Preview it started
        attachments.clear(); // before the controls they hold callbacks on
        setLookAndFeel (nullptr);
    }

    void PluginEditor::bindParameters (juce::AudioProcessor& audioProcessor)
    {
        std::map<juce::String, juce::RangedAudioParameter*> byId;
        for (auto* parameter : audioProcessor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                byId[ranged->getParameterID()] = ranged;

        const auto find = [&byId] (const char* id) -> juce::RangedAudioParameter*
        {
            const auto it = byId.find (id);
            return it != byId.end() ? it->second : nullptr;
        };

        // Every control gets its parameter's ID as its component ID; one the processor lacks stays disabled.
        const auto bind = [this, &find] (auto& control, const char* id)
        {
            control.setComponentID (id);
            if (auto* parameter = find (id))
            {
                attachments.push_back (std::make_unique<editor::PollingAttachment> (control, *parameter));
                control.onContextMenu = [this, parameter, &control] { showParameterMenu (*parameter, control); };
            }
            else
                control.setEnabled (false);
        };

        bind (fxPanel.output, ParameterIds::masterGain);
        bind (modePanel.playMode, ParameterIds::playMode);
        bind (modePanel.vox, ParameterIds::voxSynth);
        for (int i = 0; i < engine::kAbSwitches; ++i)
            bind (abPanel.abSwitch (i), ParameterIds::ab[(std::size_t) i]);
        for (std::size_t lane = 0; lane < lanes.size(); ++lane)
        {
            bind (lanes[lane]->level, ParameterIds::laneLevel[lane]);
            bind (lanes[lane]->mute, ParameterIds::laneMute[lane]);
            muteParameters[lane] = find (ParameterIds::laneMute[lane]);
        }
        bind (fxPanel.space, ParameterIds::space);
        bind (fxPanel.echo, ParameterIds::echo);
        bind (fxPanel.pump, ParameterIds::pump);
        bind (fxPanel.club, ParameterIds::clubMaster);

        playModeParameter = find (ParameterIds::playMode);
        for (std::size_t i = 0; i < abParameters.size(); ++i)
            abParameters[i] = find (ParameterIds::ab[i]);
        voxParameter = find (ParameterIds::voxSynth);
    }

    void PluginEditor::showParameterMenu (juce::RangedAudioParameter& parameter, juce::Component& control)
    {
        // The host's own menu for the parameter (automation, MIDI learn...) where it has one.
        if (auto* host = getHostContext())
            if (auto menu = host->getContextMenuForParameter (&parameter))
            {
                menu->showNativeMenu (getMouseXYRelative());
                return;
            }

        juce::PopupMenu menu;
        menu.addSectionHeader (parameter.getName (64));
        menu.addItem ("Reset to default", [&parameter]
            {
                parameter.beginChangeGesture();
                parameter.setValueNotifyingHost (parameter.getDefaultValue());
                parameter.endChangeGesture();
            });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&control).withParentComponent (this));
    }

    void PluginEditor::layOut()
    {
        content.setBounds (0, 0, baseWidth, baseHeight);
        backdrop.setBounds (content.getLocalBounds());
        focusRing.setBounds (content.getLocalBounds());
        backdrop.panels.clear();
        backdrop.lanes.clear();

        auto area = content.getLocalBounds().reduced (20, 0);

        // The header: the logo, the kit cards, the Phrases and Samples buttons and the zoom. "PHRASES" needs 89.8 px of its
        // button and "SAMPLES" 90.1 in DejaVu Sans Bold, the widest default font: both buttons are 91 px, 7 apart, in a
        // 278 px block, and the cards come 2 px from the logo and the buttons, 146 px each with a 114 px direction line
        // (FUT03's needs 111.42 at 8 px in DejaVu Sans).
        // The header (ADR 0032): the logo block (icon, wordmark, the kit's direction), the kit pills from x 250, and at the
        // right Phrases, Samples and the zoom, 8 apart.
        auto header = area.removeFromTop (76).withTrimmedTop (12).withTrimmedBottom (6);
        backdrop.logo = header.removeFromLeft (214).toFloat();
        auto right = header;
        zoomBox.setBounds (right.removeFromRight (72).withSizeKeepingCentre (72, 30));
        right.removeFromRight (8);
        samplesButton.setBounds (right.removeFromRight (samplesButton.fitWidth (44)).withSizeKeepingCentre (samplesButton.fitWidth (44), 44));
        right.removeFromRight (8);
        phrasesButton.setBounds (right.removeFromRight (phrasesButton.fitWidth (44)).withSizeKeepingCentre (phrasesButton.fitWidth (44), 44));
        right.removeFromRight (8);
        kitPanel.setBounds (juce::Rectangle<int> (250, 19, juce::jmax (0, right.getRight() - 250), 44));

        // The display, the transport and the mode switches.
        area.removeFromTop (4);
        auto row = area.removeFromTop (120);
        lcd.setBounds (row.removeFromLeft (590));
        row.removeFromLeft (10);
        transport.setBounds (row.removeFromLeft (128));
        backdrop.panels.push_back (transport.getBounds().toFloat());
        row.removeFromLeft (10);
        modePanel.setBounds (row);
        backdrop.panels.push_back (modePanel.getBounds().toFloat());
        backdrop.well = lcd.getBounds().toFloat();

        // Where playback is.
        area.removeFromTop (8);
        sectionStrip.setBounds (area.removeFromTop (30));

        // The lanes.
        area.removeFromTop (8);
        auto laneArea = area.removeFromTop (276);
        constexpr auto gap = 8;
        const auto laneWidth = (laneArea.getWidth() - gap * (engine::kLanes - 1)) / engine::kLanes;
        auto lanesLeft = laneArea;
        for (std::size_t i = 0; i < lanes.size(); ++i)
        {
            const auto width = i + 1 == lanes.size() ? lanesLeft.getWidth() : laneWidth;
            lanes[i]->setBounds (lanesLeft.removeFromLeft (width));
            lanesLeft.removeFromLeft (gap);
        }
        contentPanel.setBounds (laneArea.withTrimmedBottom (60)); // the lanes' mute pads and drag chips stay in reach

        // The A/B choices and the FX and master.
        area.removeFromTop (8);
        auto bottom = area.removeFromTop (112);

        // The Samples and Phrases drawers cover the lanes, A/B and FX: (20, 246), 1080 x 396.
        samplesPanel.setBounds (laneArea.withBottom (bottom.getBottom()));
        phrasesPanel.setBounds (samplesPanel.getBounds());
        abPanel.setBounds (bottom.removeFromLeft (340));
        bottom.removeFromLeft (10);
        fxPanel.setBounds (bottom);

        // MIDI drag-out.
        area.removeFromTop (8);
        dragStrip.setBounds (area.removeFromTop (38));
        backdrop.panels.push_back (dragStrip.getBounds().toFloat());
        backdrop.panels.push_back (abPanel.getBounds().toFloat());
        backdrop.panels.push_back (fxPanel.getBounds().toFloat());
        for (auto& lane : lanes)
            backdrop.lanes.push_back (lane->getBounds().toFloat());

        backdrop.repaint();
    }

    void PluginEditor::paint (juce::Graphics& g)
    {
        g.fillAll (lookAndFeel.skin().colours.face);
    }

    void PluginEditor::resized()
    {
        content.setTransform (juce::AffineTransform::scale (zoom));
    }

    void PluginEditor::setZoom (float newZoom)
    {
        newZoom = juce::jlimit (1.0f, 2.0f, newZoom);
        lastZoom = newZoom;
        zoomBox.setSelectedId (juce::roundToInt (newZoom * 100.0f), juce::dontSendNotification);
        if (juce::exactlyEqual (newZoom, zoom))
            return;
        zoom = newZoom;
        setSize (juce::roundToInt ((float) baseWidth * zoom), juce::roundToInt ((float) baseHeight * zoom));
        resized();
    }

    void PluginEditor::refreshModel()
    {
        auto& m = state;
        m.kits = controller.kits();
        const auto current = controller.currentKit();
        m.currentIndex = -1;
        for (int i = 0; i < (int) m.kits.size(); ++i)
            if (m.kits[(std::size_t) i].id == current)
                m.currentIndex = i;
        m.kit = m.currentIndex >= 0 ? m.kits[(std::size_t) m.currentIndex] : KitCard();

        m.content = controller.status();
        m.sections = controller.sections();
        m.snapshot = controller.snapshot();
        m.transport = controller.transport();
        m.preview = controller.preview();
        m.standalone = standalone;
        m.logicHost = logicHost;
        m.midiInMode = controller.midiInMode();
        m.midiInLane = controller.midiInLane();
        m.phraseMode = controller.phraseMode();
        m.dragDrumsOnOneTrack = controller.dragDrumsOnOneTrack();
        m.dragForSynths = controller.dragForSynths();
        m.dragPart = controller.dragPart();
        m.mutedLanes = 0;
        for (std::size_t lane = 0; lane < muteParameters.size(); ++lane)
            if (muteParameters[lane] != nullptr && muteParameters[lane]->getValue() >= 0.5f)
                m.mutedLanes |= 1u << lane;
        m.audition = controller.auditionState();
        m.drumKit = controller.drumKitStatus();
        m.phrasesGeneration = controller.phrasesGeneration();
        m.phraseAudition = controller.phraseAuditionState();

        if (playModeParameter != nullptr)
            m.playMode = static_cast<engine::PlayMode> (juce::jlimit (0, 2, juce::roundToInt (playModeParameter->convertFrom0to1 (playModeParameter->getValue()))));
        else
            m.playMode = m.snapshot.mode;
        m.vox = voxParameter == nullptr || voxParameter->getValue() >= 0.5f;
        for (std::size_t i = 0; i < abParameters.size(); ++i)
            m.abB[i] = abParameters[i] != nullptr && abParameters[i]->getValue() >= 0.5f;

        // The looks' notes (ADR 0032): the play mode's file for the vox switch, the 16th playing in it and its chord.
        m.pattern = controller.pattern (m.playMode, m.vox ? engine::Variant::vox : engine::Variant::novox);
        m.patternStep = -1;
        if (m.pattern != nullptr && m.pattern->steps > 0 && m.snapshot.playing && m.snapshot.songTicks >= 0.0)
        {
            const auto step = (int) std::floor (m.snapshot.songTicks / 240.0);
            m.patternStep = m.pattern->mode == engine::PlayMode::song ? juce::jlimit (0, m.pattern->steps - 1, step) : step % m.pattern->steps;
        }
        m.chord = m.pattern != nullptr && m.patternStep >= 0 ? m.pattern->chordAt (m.patternStep) : juce::String();
    }

    void PluginEditor::showContentPanel (bool show)
    {
        const auto key = state.kit.id + "/" + juce::String ((int) state.content.state);
        if (show)
        {
            showSamplesPanel (false);
            showPhrasesPanel (false);
            contentOpenedByHand = true;
            contentDismissedFor.clear();
        }
        else
        {
            contentOpenedByHand = false;
            contentDismissedFor = key;
        }
        contentPanel.setVisible (show);
        if (show)
            contentPanel.toFront (false);
    }

    void PluginEditor::coverFace (bool covered)
    {
        // What a drawer covers goes out of sight altogether while it shows, so Tab and screen readers never reach a control
        // the person can't see (a mute pad, an A/B switch) and change the project there. Their ticks go on: none of them
        // depends on being visible.
        for (auto& lane : lanes)
            lane->setVisible (! covered);
        abPanel.setVisible (! covered);
        fxPanel.setVisible (! covered);
    }

    void PluginEditor::showSamplesPanel (bool show)
    {
        if (show != samplesPanel.isVisible())
        {
            if (show)
            {
                showPhrasesPanel (false); // the two drawers never show together
                coverFace (true);

                // The install panel goes; it comes back by itself once the drawer closes if the kit still can't play
                // and the person hasn't closed it.
                contentOpenedByHand = false;
                contentPanel.setVisible (false);
                samplesPanel.open (state);
                samplesPanel.setVisible (true);
                samplesPanel.toFront (false);
                samplesPanel.focusList(); // the keys go to the list, not to the Samples button just clicked
            }
            else
            {
                samplesPanel.setVisible (false);
                coverFace (false);
                controller.stopAudition(); // closing the drawer stops what it plays
            }
        }
        samplesButton.setToggleState (show, juce::dontSendNotification);
    }

    void PluginEditor::showPhrasesPanel (bool show)
    {
        if (show != phrasesPanel.isVisible())
        {
            if (show)
            {
                showSamplesPanel (false); // the two drawers never show together
                coverFace (true);

                // The install panel steps aside as for the Samples drawer.
                contentOpenedByHand = false;
                contentPanel.setVisible (false);
                phrasesPanel.open (state);
                phrasesPanel.setVisible (true);
                phrasesPanel.toFront (false);
                phrasesPanel.focusGrid(); // the keys go to the grid, not to the Phrases button just clicked
            }
            else
            {
                phrasesPanel.setVisible (false);
                coverFace (false);
                controller.stopPhraseAudition(); // closing the drawer stops what it plays, and the Preview it started
            }
        }
        phrasesButton.setToggleState (show, juce::dontSendNotification);
    }

    void PluginEditor::tick()
    {
        refreshModel();
        for (auto& attachment : attachments)
            attachment->poll();

        // The look follows the kit (ADR 0032), on the message thread, on the tick after the kit changed.
        if (const auto& next = editor::skinForKit (state.kit.id); next.id != lookAndFeel.skin().id || ! skinApplied)
            applySkin (next);
        backdrop.setSubtitle (state.kit.direction);

        // Machine room's scope: drained every tick while it shows.
        if (scopeOn)
            lcd.pushScope (scopeBuffer.data(), controller.readScope (scopeBuffer.data(), (int) scopeBuffer.size()));

        kitPanel.update (state);
        lcd.update (state);
        transport.update (state);
        modePanel.update (state);
        sectionStrip.update (state);
        for (std::size_t i = 0; i < lanes.size(); ++i)
            lanes[i]->update (state, muteParameters[i] != nullptr && muteParameters[i]->getValue() >= 0.5f);
        abPanel.update (state);
        fxPanel.update (state);
        dragStrip.update (state);
        contentPanel.update (state);
        samplesPanel.update (state);
        phrasesPanel.update (state);

        // The content panel opens by itself while the kit can't play, until the person closes it; never while a drawer is
        // open (the Samples drawer's banner says the same).
        const auto key = state.kit.id + "/" + juce::String ((int) state.content.state);
        if (contentDismissedFor.isNotEmpty() && contentDismissedFor != key)
            contentDismissedFor.clear();
        const auto drawerOpen = samplesPanel.isVisible() || phrasesPanel.isVisible();
        const auto show = ! drawerOpen && (contentOpenedByHand || (isProblem (state.content.state) && contentDismissedFor.isEmpty()));
        if (show != contentPanel.isVisible())
        {
            contentPanel.setVisible (show);
            if (show)
                contentPanel.toFront (false);
        }
        samplesButton.setToggleState (samplesPanel.isVisible(), juce::dontSendNotification);
        phrasesButton.setToggleState (phrasesPanel.isVisible(), juce::dontSendNotification);

        auto* focused = focusOverride != nullptr ? focusOverride.getComponent() : juce::Component::getCurrentlyFocusedComponent();
        focusRing.follow (focused != nullptr && content.isParentOf (focused) && focused->getWantsKeyboardFocus() ? focused : nullptr);
    }

    void PluginEditor::applySkin (const editor::Skin& next)
    {
        skinApplied = true;
        lookAndFeel.setSkin (next);
        const auto wantScope = next.display == editor::DisplayKind::scope;
        if (wantScope != scopeOn)
        {
            scopeOn = wantScope;
            controller.setScopeOn (wantScope);
        }
        content.sendLookAndFeelChange(); // each panel lays itself out again in the new look
        layOut(); // the header's widths depend on the look's fonts
        for (auto* panel : std::initializer_list<juce::Component*> { &kitPanel, &transport, &modePanel, &abPanel, &fxPanel, &dragStrip })
            panel->resized();
        for (auto& lane : lanes)
            lane->resized();
        content.repaint(); // a new look repaints everything once, the cached backdrop included
        backdrop.repaint();
    }

    PluginEditor::Backdrop::Backdrop()
    {
        setComponentID ("backdrop");
    }

    void PluginEditor::Backdrop::setSubtitle (const juce::String& direction)
    {
        if (direction != subtitle)
        {
            subtitle = direction;
            repaint();
        }
    }

    std::vector<editor::DrawnText> PluginEditor::Backdrop::drawnText() const
    {
        const auto& skin = editor::skinOf (*this);
        const auto text = logo.withTrimmedLeft (50.0f);
        return { editor::fitText (skin.type.wordmark, skin.words.wordmark, text.withY (logo.getY() + 8.0f).withHeight (22.0f)),
            editor::fitText (skin.type.subtitle, subtitle, text.withY (logo.getY() + 33.0f).withHeight (14.0f)) };
    }

    void PluginEditor::Backdrop::paint (juce::Graphics& g)
    {
        ++renders;
        const auto& skin = editor::skinOf (*this);
        const auto& c = skin.colours;
        namespace paint = editor::paint;
        g.fillAll (c.face);
        const auto bounds = getLocalBounds().toFloat();
        switch (skin.texture)
        {
            case editor::Texture::grid: // mission HUD: a faint plotting grid
                g.setColour (c.texture.withAlpha (0.35f));
                for (auto x = 0.0f; x < bounds.getWidth(); x += 40.0f)
                    g.fillRect (x, 0.0f, 1.0f, bounds.getHeight());
                for (auto y = 0.0f; y < bounds.getHeight(); y += 40.0f)
                    g.fillRect (0.0f, y, bounds.getWidth(), 1.0f);
                break;
            case editor::Texture::water:
            case editor::Texture::furnace:
            case editor::Texture::plain:
                break;
        }

        for (const auto& panel : panels)
            paint::box (g, panel, skin.shape.panel, c.panel, c.panelEdge);
        for (const auto& lane : lanes)
            paint::box (g, lane, skin.shape.lane, c.lane, c.laneEdge);
        paint::box (g, well, skin.shape.well, c.well, c.wellEdge);

        // The emblem: a tile with the look's mark, then the wordmark and the kit's direction.
        const auto tile = juce::Rectangle<float> (logo.getX(), logo.getY() + 10.0f, 38.0f, 38.0f);
        paint::box (g, tile, skin.shape.headerButton, c.panel, c.controlEdge);
        juce::Path mark;
        const auto m = tile.reduced (10.0f, 13.0f);
        mark.startNewSubPath (m.getX(), m.getBottom());
        mark.lineTo (m.getX() + m.getWidth() * 0.3f, m.getY());
        mark.lineTo (m.getCentreX(), m.getBottom() - m.getHeight() * 0.35f);
        mark.lineTo (m.getRight() - m.getWidth() * 0.3f, m.getY());
        mark.lineTo (m.getRight(), m.getBottom());
        g.setColour (c.accent);
        g.strokePath (mark, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto texts = drawnText();
        editor::drawText (g, texts[0], c.text);
        editor::drawText (g, texts[1], c.dim);
    }

    PluginEditor::FocusRing::FocusRing()
    {
        setInterceptsMouseClicks (false, false);
    }

    juce::Rectangle<float> PluginEditor::FocusRing::ringArea() const
    {
        if (shown == nullptr || getParentComponent() == nullptr)
            return {};
        const auto& skin = editor::skinOf (*this);
        return getParentComponent()->getLocalArea (shown, shown->getLocalBounds()).toFloat().expanded (skin.shape.focusGap + skin.shape.focusWidth + 1.0f);
    }

    void PluginEditor::FocusRing::follow (juce::Component* target)
    {
        if (target == shown.getComponent())
            return;
        if (const auto old = ringArea(); ! old.isEmpty())
            editor::repaintArea (*this, old.getSmallestIntegerContainer());
        shown = target;
        if (const auto now = ringArea(); ! now.isEmpty())
            editor::repaintArea (*this, now.getSmallestIntegerContainer());
    }

    void PluginEditor::FocusRing::paint (juce::Graphics& g)
    {
        if (shown == nullptr || ! shown->isVisible())
            return;
        const auto& skin = editor::skinOf (*this);
        const auto box = getLocalArea (shown, shown->getLocalBounds()).toFloat();
        editor::paint::focusRing (g, skin, box, std::min (skin.shape.segment.corner, box.getHeight() * 0.5f));
    }

    const editor::Skin& PluginEditor::skin() const noexcept { return lookAndFeel.skin(); }

    int PluginEditor::backdropRenders() const noexcept { return backdrop.renders; }

    void PluginEditor::setFocusOverrideForTesting (juce::Component* c)
    {
        focusOverride = c;
        focusRing.follow (c);
    }
}
