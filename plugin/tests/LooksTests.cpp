// The four looks in the editor (ADR 0032): each kit wears its own look, switching with the kit on the message thread;
// the same controls sit in the same places in every look, every label reads in full in its embedded face, the keys and
// screen-reader names work the same, and every light shows the kit's own notes, levels or output.
//
// The kits are the four real ones, their cards, Song sections and pattern views from the embedded catalog, behind a
// FakeController.

#include "content/EmbeddedContent.h"
#include "editor/DrawnText.h"
#include "editor/RepaintLedger.h"
#include "helpers/editor_test_helpers.h"
#include "services/KitCatalog.h"
#include "services/PatternView.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
#include <set>

using namespace wubansia::test::ui;
using wubansia::test::FakeController;
using wubansia::test::TestProcessor;
namespace editor = wubansia::editor;
namespace engine = wubansia::engine;
using editor::SkinId;
using editor::StepCell;

namespace
{
    const wubansia::KitCatalog& catalog()
    {
        static const wubansia::KitCatalog c (wubansia::EmbeddedContent::sources());
        return c;
    }

    const juce::StringArray kKits { "FUT01", "FUT02", "FUT03", "FUT04" };
    const std::map<juce::String, SkinId> kLookOf { { "FUT01", SkinId::nightBus }, { "FUT02", SkinId::missionHud },
        { "FUT03", SkinId::deepGlass }, { "FUT04", SkinId::machineRoom } };

    // A FakeController showing the four real kits, ready, with their Song sections and their own notes.
    struct LooksRig
    {
        LooksRig()
        {
            auto& c = controller;
            c.kitCards.clear();
            for (const auto& e : catalog().entries())
            {
                auto card = e.card;
                card.content.state = wubansia::ContentState::ready;
                c.kitCards.push_back (card);
            }
            c.patternFor = [this] (engine::PlayMode mode, engine::Variant variant) -> std::shared_ptr<const wubansia::PatternView>
            {
                const auto* e = catalog().find (controller.currentKit().toStdString());
                return e != nullptr ? wubansia::patternOf (*e, mode, variant) : nullptr;
            };
            select ("FUT01");
        }

        void select (const juce::String& kit)
        {
            controller.selectKit (kit);
            controller.contentStatus.state = wubansia::ContentState::ready;
            const auto* e = catalog().find (kit.toStdString());
            controller.sectionList = e != nullptr ? e->sections[0] : std::vector<wubansia::SectionInfo> {};
            for (const auto& card : controller.kitCards)
                if (card.id == kit)
                    controller.transportState.bpm = card.bpm;
        }

        const wubansia::KitCard& card (const juce::String& kit) const
        {
            for (const auto& c : controller.kitCards)
                if (c.id == kit)
                    return c;
            FAIL ("no kit " << kit);
            return controller.kitCards.front();
        }

        FakeController controller;
        TestProcessor processor { [this] (TestProcessor& p) { return new PluginEditor (p, controller); } };
    };

    // The editor's face (the 1120 x 700 component the zoom scales).
    juce::Component& face (PluginEditor& editor) { return *editor.getChildComponent (0); }

    juce::Rectangle<int> faceBounds (PluginEditor& editor, juce::Component& c)
    {
        return face (editor).getLocalArea (&c, c.getLocalBounds());
    }

    // Shows kit's look: the kit chosen as the pill does it, then one tick.
    void showKit (LooksRig& rig, PluginEditor& editor, const juce::String& kit)
    {
        rig.select (kit);
        editor.tick();
    }

    // Plays: a snapshot at song tick `ticks` with the given hits, then a tick.
    void playAt (LooksRig& rig, PluginEditor& editor, double ticks, std::uint32_t hits = 0, float peak = 0.5f)
    {
        auto& s = rig.controller.nextSnapshot;
        s.playing = true;
        s.bpm = editor.model().kit.bpm;
        s.songTicks = ticks;
        s.laneHits = hits;
        for (auto& p : s.lanePeak)
            p = peak;
        editor.tick();
    }

    void stop (LooksRig& rig, PluginEditor& editor)
    {
        rig.controller.nextSnapshot = {};
        editor.tick();
    }

    void collectSources (juce::Component& root, std::vector<juce::Component*>& out)
    {
        for (auto* child : root.getChildren())
        {
            if (! child->isVisible())
                continue;
            if (dynamic_cast<editor::DrawnTextSource*> (child) != nullptr)
                out.push_back (child);
            collectSources (*child, out);
        }
    }

    std::vector<editor::DrawnText> allText (juce::Component& root)
    {
        std::vector<juce::Component*> sources;
        collectSources (root, sources);
        std::vector<editor::DrawnText> all;
        for (auto* s : sources)
            for (auto& t : dynamic_cast<editor::DrawnTextSource*> (s)->drawnText())
                all.push_back (t);
        return all;
    }

    bool anyTextContains (juce::Component& c, const juce::String& needle)
    {
        for (const auto& t : dynamic_cast<editor::DrawnTextSource&> (c).drawnText())
            if (t.text.contains (needle))
                return true;
        return false;
    }

    juce::String joinedText (juce::Component& c)
    {
        juce::StringArray texts;
        for (const auto& t : dynamic_cast<editor::DrawnTextSource&> (c).drawnText())
            texts.add (t.text);
        return texts.joinIntoString (" | ");
    }

    // Every text a component draws reads in full in its font, and every character has a glyph in its face unless the
    // style lets the platform fill in.
    void checkFits (juce::Component& c)
    {
        for (const auto& t : dynamic_cast<editor::DrawnTextSource&> (c).drawnText())
        {
            INFO (c.getComponentID() << " / " << c.getName() << ": \"" << t.text << "\" in " << t.area.toString() << " at " << t.font.getHeightInPoints());
            CHECK_FALSE (t.cut);
            CHECK (juce::GlyphArrangement::getStringWidth (t.font, t.text) <= t.area.getWidth() + 0.5f);
            CHECK (t.font.getHeightInPoints() >= t.minPx - 0.01f);
            if (! t.font.getFallbackEnabled())
                if (auto typeface = t.font.getTypefacePtr())
                    for (int i = 0; i < t.text.length(); ++i)
                    {
                        INFO ("U+" << juce::String::toHexString ((int) t.text[i]));
                        CHECK (typeface->getNominalGlyphForCodepoint (t.text[i]).has_value());
                    }
        }
    }

    // The control IDs every look shows (the plan's section 5.7), with the region each lives in.
    struct Region
    {
        juce::String id;
        juce::Rectangle<int> bounds;
    };

    juce::Rectangle<int> laneBounds (int i) { return { 20 + 108 * i, 246, i == 9 ? 108 : 100, 276 }; }

    juce::Rectangle<int> regionOf (const juce::String& id)
    {
        const juce::Rectangle<int> header { 20, 12, 1080, 58 }, transport { 620, 80, 128, 120 }, mode { 758, 80, 342, 120 },
            ab { 20, 530, 340, 112 }, fx { 370, 530, 730, 112 }, strip { 20, 650, 1080, 38 };
        if (id.startsWith ("kit_") || id == "kits" || id == "phrases" || id == "samples" || id == "zoom")
            return header;
        if (id == "preview" || id == "transport_play" || id.startsWith ("tempo_"))
            return transport;
        if (id == "play_mode" || id == "vox_synth" || id.startsWith ("midi_in") || id == "phrase_mode")
            return mode;
        if (id.endsWith ("_ab"))
            return ab;
        if (id == "space" || id == "echo" || id == "pump" || id == "club_master" || id == "master_gain")
            return fx;
        for (int i = 0; i < engine::kLanes; ++i)
        {
            const auto lane = juce::String (std::string (engine::laneId (static_cast<engine::Lane> (i))));
            if (id == lane + "_level" || id == lane + "_mute" || id == "drag_" + lane)
                return laneBounds (i);
        }
        if (id.startsWith ("drag_"))
            return strip;
        return {};
    }

    juce::StringArray controlIds()
    {
        juce::StringArray list { "kit_FUT01", "kit_FUT02", "kit_FUT03", "kit_FUT04", "phrases", "samples", "zoom", "preview",
            "play_mode", "vox_synth", "midi_in_mode", "phrase_mode" };
        for (int i = 0; i < engine::kLanes; ++i)
        {
            const auto lane = juce::String (std::string (engine::laneId (static_cast<engine::Lane> (i))));
            list.addArray (juce::StringArray { lane + "_level", lane + "_mute", "drag_" + lane });
        }
        list.addArray (juce::StringArray { "kick_ab", "bass_ab", "stab_ab", "space", "echo", "pump", "club_master", "master_gain",
            "drag_loop", "drag_song", "drag_part", "drag_drumkit", "drag_drums", "drag_synths" });
        return list;
    }

    bool shownInEditor (juce::Component* c, juce::Component& root)
    {
        for (; c != nullptr && c != &root; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return c == &root;
    }

    // The order Tab visits the face in, in every look (the plan's 3.11), before the ones the editor shows are kept.
    juce::StringArray tabOrder()
    {
        juce::StringArray list { "kit_FUT01", "kit_FUT02", "kit_FUT03", "kit_FUT04", "phrases", "samples", "zoom", "preview",
            "play_mode", "vox_synth", "midi_in_mode", "midi_in_lane", "phrase_mode" };
        for (int i = 0; i < engine::kLanes; ++i)
        {
            const auto lane = juce::String (std::string (engine::laneId (static_cast<engine::Lane> (i))));
            list.addArray (juce::StringArray { lane + "_level", lane + "_mute" });
        }
        list.addArray (juce::StringArray { "kick_ab", "bass_ab", "stab_ab", "space", "echo", "pump", "club_master", "master_gain",
            "drag_part", "drag_drumkit", "drag_drums", "drag_synths" });
        return list;
    }

    // A 41.2 Hz saw at 0.35 plus an 82.4 Hz sine at 0.15 (peak about 0.5), from sample `from`, at 48 kHz.
    std::vector<float> scopeSignal (int from, int count)
    {
        std::vector<float> s ((std::size_t) count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = (double) (from + i) / 48000.0;
            const auto saw = 2.0 * (t * 41.2 - std::floor (t * 41.2 + 0.5));
            s[(std::size_t) i] = (float) (0.35 * saw + 0.15 * std::sin (2.0 * juce::MathConstants<double>::pi * 82.4 * t));
        }
        return s;
    }

    std::map<juce::String, juce::Rectangle<int>> fixedRegions()
    {
        std::map<juce::String, juce::Rectangle<int>> r { { "lcd", { 20, 80, 590, 120 } }, { "transport", { 620, 80, 128, 120 } },
            { "mode", { 758, 80, 342, 120 } }, { "sections", { 20, 208, 1080, 30 } }, { "ab", { 20, 530, 340, 112 } },
            { "fx", { 370, 530, 730, 112 } }, { "drag", { 20, 650, 1080, 38 } } };
        for (int i = 0; i < engine::kLanes; ++i)
            r["lane_" + juce::String (std::string (engine::laneId (static_cast<engine::Lane> (i))))] = laneBounds (i);
        return r;
    }

    wubansia::editor::LaneStrip& lane (juce::Component& root, engine::Lane l)
    {
        return get<wubansia::editor::LaneStrip> (root, "lane_" + juce::String (std::string (engine::laneId (l))));
    }

    juce::Image paintFace (PluginEditor& editor)
    {
        juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
        juce::Graphics g (image);
        editor.paintEntireComponent (g, false);
        return image;
    }
}

TEST_CASE ("the look follows the kit on the next tick, and changes nothing else", "[editor][looks]")
{
    LooksRig rig;
    GestureLog log;
    rig.processor.addListener (&log);
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            CHECK (editor.skin().id == SkinId::nightBus);
            for (const auto& kit : { "FUT02", "FUT03", "FUT04", "FUT01" })
            {
                INFO (kit);
                click (get<juce::Component> (editor, "kit_" + juce::String (kit)));
                CHECK (rig.controller.selectedKits[rig.controller.selectedKits.size() - 1] == kit);
                CHECK (editor.skin().id != kLookOf.at (kit)); // not before the tick
                rig.select (kit);
                editor.tick();
                CHECK (editor.skin().id == kLookOf.at (kit));
                const auto& c = editor.skin().colours;
                CHECK (editor.getLookAndFeel().findColour (juce::PopupMenu::backgroundColourId) == editor::over (c.panel, c.face));
            }
            CHECK (rig.controller.scopeRequests == std::vector<bool> { true, false }); // on into machine room, off out of it
        });
    rig.processor.removeListener (&log);
    CHECK (rig.controller.scopeRequests == std::vector<bool> { true, false, false }); // closing the editor turns it off
    CHECK (rig.controller.transportRequests.empty());
    CHECK (rig.controller.previewRequests.empty());
    for (const auto& e : log.events)
        CHECK (e.type != 'v'); // no parameter moved
}

TEST_CASE ("chord and note names keep their case in every look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            auto& lcd = get<juce::Component> (editor, "lcd");
            showKit (rig, editor, "FUT04");
            playAt (rig, editor, 0.0);
            INFO (joinedText (lcd));
            CHECK (anyTextContains (lcd, "Em(add9)"));
            CHECK_FALSE (anyTextContains (lcd, "EM(ADD9)"));

            showKit (rig, editor, "FUT02");
            playAt (rig, editor, 0.0);
            INFO (joinedText (lcd));
            CHECK (anyTextContains (lcd, "Dm9"));
            CHECK (anyTextContains (lcd, "LOW A4"));
            CHECK_FALSE (anyTextContains (lcd, "DM9"));
        });
}

TEST_CASE ("the regions are where they always were, in every look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                showKit (rig, editor, kit);
                INFO (kit);
                for (const auto& [id, bounds] : fixedRegions())
                {
                    INFO (id);
                    CHECK (faceBounds (editor, get<juce::Component> (editor, id)) == bounds);
                }
                CHECK (faceBounds (editor, get<juce::Component> (editor, "samples_drawer")) == juce::Rectangle<int> (20, 246, 1080, 396));
                CHECK (faceBounds (editor, get<juce::Component> (editor, "phrases_drawer")) == juce::Rectangle<int> (20, 246, 1080, 396));
            }
        });
}

TEST_CASE ("every control is there, visible and enabled as its kit says, inside its region, and a click at its centre reaches it, at every zoom",
    "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
                for (const auto percent : PluginEditor::zoomPercents)
                {
                    showKit (rig, editor, kit);
                    editor.setZoom ((float) percent / 100.0f);
                    editor.tick();
                    INFO (kit << " at " << percent << " %");
                    const auto& card = rig.card (kit);
                    for (const auto& id : controlIds())
                    {
                        INFO (id);
                        const auto found = findAll (editor, id);
                        REQUIRE (found.size() == 1);
                        auto& c = *found.front();
                        const auto abIndex = id == "kick_ab" ? 0 : id == "bass_ab" ? 1 : id == "stab_ab" ? 2 : -1;
                        if (abIndex >= 0 && (card.abA[(std::size_t) abIndex].isEmpty() || card.abB[(std::size_t) abIndex].isEmpty()))
                        {
                            CHECK_FALSE (c.isVisible()); // a switch the kit lacks is out of sight, and of Tab's reach
                            CHECK_FALSE (c.isEnabled());
                            continue;
                        }
                        REQUIRE (shownInEditor (&c, editor));
                        if (id != "drag_drumkit") // dimmed while the samples aren't there (ADR 0030)
                            CHECK (c.isEnabled());
                        CHECK (regionOf (id).contains (faceBounds (editor, c)));

                        // The editor's own coordinates: the zoom scales the face.
                        const auto centre = editor.getLocalPoint (&c, c.getLocalBounds().getCentre());
                        auto* hit = editor.getComponentAt (centre);
                        CHECK ((hit == &c || c.isParentOf (hit)));
                    }
                    for (const auto& hidden : { "midi_in_lane", "transport_play", "tempo_down", "tempo_kit", "tempo_up" })
                        for (auto* c : findAll (editor, hidden))
                            CHECK_FALSE (shownInEditor (c, editor));

                    // The display, the sections and the lane displays are pictures: clicks go through them to nothing.
                    for (const auto& id : { "lcd", "sections" })
                    {
                        bool self = true, children = true;
                        get<juce::Component> (editor, id).getInterceptsMouseClicks (self, children);
                        CHECK_FALSE (self);
                    }
                }
            editor.setZoom (1.0f);
        });
}

TEST_CASE ("no two controls overlap in any look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                showKit (rig, editor, kit);
                INFO (kit);
                std::vector<std::pair<juce::String, juce::Rectangle<int>>> shown;
                for (const auto& id : controlIds())
                    for (auto* c : findAll (editor, id))
                        if (shownInEditor (c, editor))
                            shown.emplace_back (id, faceBounds (editor, *c));
                REQUIRE (shown.size() > 50);
                for (std::size_t i = 0; i < shown.size(); ++i)
                    for (std::size_t j = i + 1; j < shown.size(); ++j)
                    {
                        INFO (shown[i].first << " " << shown[i].second.toString() << " and " << shown[j].first << " " << shown[j].second.toString());
                        CHECK_FALSE (shown[i].second.intersects (shown[j].second));
                    }
            }
        });
}

TEST_CASE ("every label reads in full in every look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                for (const auto extreme : { 0, 1 })
                {
                    rig.processor.set (ids::space, extreme == 0 ? 0.0f : 200.0f);
                    rig.processor.set (ids::echo, extreme == 0 ? 0.0f : 200.0f);
                    rig.processor.set (ids::masterGain, extreme == 0 ? -60.0f : 6.0f);
                    for (const auto* level : ids::laneLevel)
                        rig.processor.set (level, extreme == 0 ? -60.0f : 6.0f);
                    rig.processor.set (ids::playMode, extreme == 0 ? 1.0f : 2.0f);
                    auto& s = rig.controller.nextSnapshot;
                    s = {};
                    s.playing = true;
                    s.bpm = extreme == 0 ? 200.0 : 128.5;
                    s.mode = extreme == 0 ? engine::PlayMode::loop : engine::PlayMode::song;
                    s.songTicks = extreme == 0 ? 3840.0 + 480.0 : 3840.0 * 223.0 + 2880.0; // Song bar 224
                    s.section = extreme == 0 ? -1 : (int) rig.controller.sectionList.size() - 1;
                    s.gainReductionDb = 8.0f;
                    s.laneHits = 0x3FFu;
                    for (auto& p : s.lanePeak)
                        p = 1.0f;
                    editor.tick();

                    const auto texts = allText (editor);
                    REQUIRE (texts.size() > 60);
                    std::vector<juce::Component*> sources;
                    collectSources (editor, sources);
                    REQUIRE (sources.size() > 30);
                    for (auto* source : sources)
                        checkFits (*source);
                }
            }
            rig.processor.set (ids::playMode, 1.0f);
        });
}

TEST_CASE ("the drawers and the content panel keep run-time text on fallback, and their fixed words in the look's face", "[editor][looks]")
{
    LooksRig rig;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const auto folder = home.getChildFile (juce::String (juce::CharPointer_UTF8 ("\xd0\x9c\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0/\xc5\x81\xc3\xb3\xd0\xb4\xc5\xba/\xe3\x82\xb5\xe3\x83\xb3\xe3\x83\x97\xe3\x83\xab")));
    rig.controller.samplesFolderChosen = folder;
    rig.controller.contentStatus.folder = folder;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                rig.controller.contentStatus.folder = folder;
                rig.controller.contentStatus.state = wubansia::ContentState::missing;
                rig.controller.contentStatus.searched = juce::StringArray { folder.getFullPathName() };
                editor.tick();
                auto& samples = get<juce::Component> (editor, "samples");
                click (samples);
                editor.tick();
                REQUIRE (editor.isSamplesPanelShown());
                int found = 0;
                for (const auto& t : allText (editor))
                    if (t.text.containsChar (0x30b5) || t.text.containsChar (0x0141) || t.text.containsChar (0x041c))
                    {
                        INFO (t.text);
                        CHECK (t.font.getFallbackEnabled());
                        ++found;
                    }
                CHECK (found > 0);
                click (samples);
                editor.tick();
                rig.controller.contentStatus.state = wubansia::ContentState::ready;
            }
        });
}

TEST_CASE ("Tab visits every control once, in the same order in every look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                juce::StringArray got;
                for (auto* c : juce::KeyboardFocusTraverser().getAllComponents (&editor))
                    got.add (c->getComponentID());

                juce::StringArray expected;
                for (const auto& id : tabOrder())
                    for (auto* c : findAll (editor, id))
                        if (shownInEditor (c, editor) && c->isEnabled())
                            expected.add (id);
                CHECK (got.joinIntoString (" ") == expected.joinIntoString (" "));
                CHECK (got.contains ("kick_level"));
                CHECK (got.contains ("space"));
            }
        });
}

TEST_CASE ("screen readers hear the names they heard in v0.2, in every look", "[editor][looks]")
{
    // Recorded from the editor at the v0.3 head before the looks (6ec8b9a): every component with an ID and a title,
    // its title and its role (juce::AccessibilityRole). kick_ab and stab_ab drop out for FUT02-FUT04 on purpose (ADR
    // 0032): the switches those kits lack are hidden, and JUCE keeps hidden components out of the accessibility tree.
    struct Entry
    {
        const char* id;
        const char* title;
        int role;
    };
    static const Entry common[] = { { "kit_FUT01", "FUT01 Alloy Signal", 0 }, { "kit_FUT02", "FUT02 Orbit Relay", 0 },
        { "kit_FUT03", "FUT03 Glass Current", 0 }, { "kit_FUT04", "FUT04 Furnace Code", 0 }, { "phrases", "Phrases", 0 },
        { "samples", "Samples", 0 }, { "preview", "Preview", 0 }, { "play_mode", "Play Mode", 3 }, { "vox_synth", "Vox Synth", 0 },
        { "kick_level", "Kick Level", 5 }, { "kick_mute", "Kick Mute", 0 }, { "snare_level", "Snare Level", 5 }, { "snare_mute", "Snare Mute", 0 },
        { "hats_level", "Hats Level", 5 }, { "hats_mute", "Hats Mute", 0 }, { "perc_level", "Perc Level", 5 }, { "perc_mute", "Perc Mute", 0 },
        { "bass_level", "Bass Level", 5 }, { "bass_mute", "Bass Mute", 0 }, { "lead_level", "Lead Level", 5 }, { "lead_mute", "Lead Mute", 0 },
        { "chords_level", "Chords Level", 5 }, { "chords_mute", "Chords Mute", 0 }, { "vox_level", "Vox Level", 5 }, { "vox_mute", "Vox Mute", 0 },
        { "accents_level", "Accents Level", 5 }, { "accents_mute", "Accents Mute", 0 }, { "fx_level", "FX Level", 5 }, { "fx_mute", "FX Mute", 0 },
        { "kick_ab", "Kick A/B", 0 }, { "bass_ab", "Bass A/B", 0 }, { "stab_ab", "Stab A/B", 0 }, { "space", "Space", 5 }, { "echo", "Echo", 5 },
        { "pump", "Pump", 5 }, { "club_master", "Club Master", 0 }, { "master_gain", "Gain", 5 }, { "drag_synths", "For my synths", 0 },
        { "drag_loop", "Drag MIDI: Loop", 30 }, { "drag_song", "Drag MIDI: Song", 30 }, { "drag_part", "Part", 3 },
        { "drag_drums", "1 drum track", 0 }, { "drag_drumkit", "Drum kit", 0 } };
    static const std::map<juce::String, juce::StringArray> laneChips {
        { "FUT01", { "Kick", "Clap", "Hats", "Perc", "Warp Bass", "Chrome Pluck", "Organ Stab", "Vox Synth", "Accents", "FX" } },
        { "FUT02", { "Kick", "Clap", "Hats", "Zap", "Roll Bass", "Chrome Arp", "Starfield Pad", "Vox Synth", "Accents", "FX" } },
        { "FUT03", { "Kick", "Clap", "Hats", "Perc", "Rubber Bass", "Keys EP", "Dub Stab", "Vox Synth", "Accents", "FX" } },
        { "FUT04", { "Kick", "Snare", "Hats", "Anvil", "Acid", "Mod Blip", "Drone", "Vox Synth", "Accents", "FX" } } };

    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                std::vector<Entry> expected (std::begin (common), std::end (common));
                if (kit != "FUT01")
                    std::erase_if (expected, [] (const Entry& e) { return juce::String (e.id) == "kick_ab" || juce::String (e.id) == "stab_ab"; });
                for (const auto& e : expected)
                {
                    INFO (e.id);
                    const auto found = findAll (editor, e.id);
                    REQUIRE (found.size() == 1);
                    CHECK (shownInEditor (found.front(), editor));
                    const auto handler = found.front()->createAccessibilityHandler();
                    REQUIRE (handler != nullptr);
                    CHECK (handler->getTitle() == juce::String (e.title));
                    CHECK ((int) handler->getRole() == e.role);
                }
                for (int i = 0; i < engine::kLanes; ++i)
                {
                    auto& chip = get<juce::Component> (editor, "drag_" + juce::String (std::string (engine::laneId (static_cast<engine::Lane> (i)))));
                    CHECK (chip.createAccessibilityHandler()->getTitle() == "Drag MIDI: " + laneChips.at (kit)[i]);
                }
                if (kit != "FUT01")
                    for (const auto* id : { "kick_ab", "stab_ab" })
                        CHECK_FALSE (shownInEditor (&get<juce::Component> (editor, id), editor));
            }
        });
}

TEST_CASE ("keys move what the mouse moves, as gestures", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                const auto machineRoom = kit == "FUT04";

                // A fader: 1 dB, Shift 0.1 dB, Home its default.
                rig.processor.set (ids::laneLevel[2], -10.0f);
                editor.tick();
                auto& fader = get<juce::Slider> (editor, ids::laneLevel[2]);
                CHECK (fader.getWantsKeyboardFocus());
                CHECK_FALSE (fader.getMouseClickGrabsKeyboardFocus());
                const auto index = rig.processor.parameter (ids::laneLevel[2]).getParameterIndex();
                GestureLog log;
                rig.processor.addListener (&log);
                CHECK (fader.keyPressed (juce::KeyPress (machineRoom ? juce::KeyPress::rightKey : juce::KeyPress::upKey)));
                CHECK (rig.processor.value (ids::laneLevel[2]) == Catch::Approx (-9.0f).margin (0.01));
                CHECK (fader.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0)));
                CHECK (rig.processor.value (ids::laneLevel[2]) == Catch::Approx (-8.9f).margin (0.01));
                CHECK (fader.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
                CHECK (rig.processor.value (ids::laneLevel[2]) == Catch::Approx (-9.9f).margin (0.01));
                CHECK (fader.keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
                CHECK (rig.processor.value (ids::laneLevel[2]) == Catch::Approx (0.0f).margin (0.01));
                CHECK (log.changedOnlyInsideGestures (index));
                rig.processor.removeListener (&log);

                // A macro knob (or deep glass's bar): 5 %, Shift 1 %.
                rig.processor.set (ids::space, 100.0f);
                editor.tick();
                auto& space = get<juce::Slider> (editor, ids::space);
                CHECK (space.getWantsKeyboardFocus());
                CHECK_FALSE (space.getMouseClickGrabsKeyboardFocus());
                GestureLog knobLog;
                rig.processor.addListener (&knobLog);
                CHECK (space.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
                CHECK (rig.processor.value (ids::space) == Catch::Approx (105.0f).margin (0.01));
                CHECK (space.keyPressed (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0)));
                CHECK (rig.processor.value (ids::space) == Catch::Approx (104.0f).margin (0.01));
                CHECK (knobLog.changedOnlyInsideGestures (rig.processor.parameter (ids::space).getParameterIndex()));
                rig.processor.removeListener (&knobLog);

                // The A/B switch: Return flips, Left picks A, Right picks B; Space is the host's.
                auto& ab = get<juce::Component> (editor, ids::bassAb); // keyPressed is Component's public face
                CHECK_FALSE (ab.getMouseClickGrabsKeyboardFocus());
                rig.processor.set (ids::bassAb, 1.0f);
                editor.tick();
                GestureLog abLog;
                rig.processor.addListener (&abLog);
                CHECK_FALSE (ab.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
                CHECK (rig.processor.value (ids::bassAb) == Catch::Approx (1.0f));
                CHECK (ab.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
                CHECK (rig.processor.value (ids::bassAb) == Catch::Approx (0.0f));
                CHECK (ab.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
                CHECK (rig.processor.value (ids::bassAb) == Catch::Approx (1.0f));
                CHECK (ab.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
                CHECK (rig.processor.value (ids::bassAb) == Catch::Approx (0.0f));
                CHECK (abLog.changedOnlyInsideGestures (rig.processor.parameter (ids::bassAb).getParameterIndex()));
                rig.processor.removeListener (&abLog);
            }
        });
}

TEST_CASE ("each look rings the focused control", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                for (const auto* id : { "kick_level", "space", "kit_FUT03", "bass_ab", "play_mode" })
                {
                    INFO (id);
                    auto& control = get<juce::Component> (editor, id);
                    editor.setFocusOverrideForTesting (nullptr);
                    const auto plain = paintFace (editor);
                    editor.setFocusOverrideForTesting (&control);
                    const auto ringed = paintFace (editor);
                    editor.setFocusOverrideForTesting (nullptr);

                    const auto box = faceBounds (editor, control);
                    const auto outer = box.expanded (6);
                    const auto focus = editor.skin().colours.focus;
                    int ring = 0, elsewhere = 0;
                    for (int y = 0; y < plain.getHeight(); ++y)
                        for (int x = 0; x < plain.getWidth(); ++x)
                            if (plain.getPixelAt (x, y) != ringed.getPixelAt (x, y))
                            {
                                if (outer.contains (x, y) && ! box.reduced (3).contains (x, y))
                                {
                                    const auto p = ringed.getPixelAt (x, y);
                                    if (std::abs (p.getRed() - focus.getRed()) < 40 && std::abs (p.getGreen() - focus.getGreen()) < 40
                                        && std::abs (p.getBlue() - focus.getBlue()) < 40)
                                        ++ring;
                                }
                                else
                                    ++elsewhere;
                            }
                    CHECK (ring > box.getWidth()); // a 2 px ring around the box
                    CHECK (elsewhere == 0);
                }
            }
        });
}

TEST_CASE ("the A/B panel shows the kit's switches", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            auto& panel = get<editor::AbPanel> (editor, "ab");
            showKit (rig, editor, "FUT01");
            CHECK (panel.display (0).title == "KICK");
            CHECK (panel.display (0).a == "A" + editor::middleDot() + "Kick Split");
            CHECK (panel.display (0).b == "B" + editor::middleDot() + "Kick Garage");
            CHECK (panel.display (1).a == "A" + editor::middleDot() + "Warp Clean");
            CHECK (panel.display (2).title == "STAB");
            CHECK (panel.display (2).b == "B" + editor::middleDot() + "E Piano Stab");
            for (const auto* id : { "kick_ab", "bass_ab", "stab_ab" })
                CHECK (get<juce::Component> (editor, id).isVisible());

            showKit (rig, editor, "FUT02");
            for (const auto* id : { "kick_ab", "stab_ab" })
            {
                CHECK_FALSE (get<juce::Component> (editor, id).isVisible());
                CHECK_FALSE (get<juce::Component> (editor, id).isEnabled());
            }
            CHECK (panel.display (1).title == "ROLL BASS" + editor::middleDot() + "A/B");
            CHECK (panel.display (1).a == "A" + editor::middleDot() + "CLEAN");
            CHECK (panel.display (1).b == "B" + editor::middleDot() + "WARM");

            showKit (rig, editor, "FUT03");
            CHECK (panel.display (1).title == "Rubber Bass" + editor::middleDot() + "A/B");
            CHECK (panel.display (1).a == "A" + editor::middleDot() + "Clean");
            CHECK (panel.display (1).b == "B" + editor::middleDot() + "Warm");

            showKit (rig, editor, "FUT04");
            CHECK (panel.display (1).title == "ACID" + editor::middleDot() + "A/B");
            CHECK (panel.display (1).a == "A" + editor::middleDot() + "ACID CLEAN");
            CHECK (panel.display (1).b == "B" + editor::middleDot() + "ACID WARM");
            checkFits (panel);
        });
}

TEST_CASE ("each lane's second line says what the lane is doing", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            const auto line = [&] (engine::Lane l) { return lane (editor, l).secondLine(); };
            const auto dot = editor::middleDot();
            showKit (rig, editor, "FUT01");
            const auto& fut01 = rig.card ("FUT01");
            const auto rec = [&] (engine::Lane l, const juce::String& word) { return (fut01.phraseLanes & engine::laneBit (l)) != 0 ? word : juce::String(); };
            const auto join = [&] (const juce::String& a, const juce::String& b) { return a.isEmpty() ? b : b.isEmpty() ? a : a + dot + b; };
            CHECK (line (engine::Lane::kick) == join ("B", rec (engine::Lane::kick, "REC")));
            CHECK (line (engine::Lane::bass) == join ("B", rec (engine::Lane::bass, "REC")));
            CHECK (line (engine::Lane::chords) == join ("A", rec (engine::Lane::chords, "REC")));
            CHECK (line (engine::Lane::lead) == rec (engine::Lane::lead, "REC"));
            CHECK (line (engine::Lane::vox) == rec (engine::Lane::vox, "REC"));
            rig.controller.transportState.bpm = 120.0; // far from the kit's 134: the multisamples play
            editor.tick();
            CHECK (line (engine::Lane::vox) == rec (engine::Lane::vox, "MULTI"));

            showKit (rig, editor, "FUT02");
            const auto& fut02 = rig.card ("FUT02");
            const auto ch = [&] (engine::Lane l, int n) { return "CH " + juce::String (n) + ((fut02.phraseLanes & engine::laneBit (l)) != 0 ? dot + "REC" : juce::String()); };
            for (const auto l : { engine::Lane::kick, engine::Lane::snare, engine::Lane::hats, engine::Lane::perc })
                CHECK (line (l) == ch (l, 10));
            CHECK (line (engine::Lane::bass) == ch (engine::Lane::bass, 1));
            CHECK (line (engine::Lane::lead) == ch (engine::Lane::lead, 2));
            CHECK (line (engine::Lane::chords) == ch (engine::Lane::chords, 4));
            CHECK (line (engine::Lane::vox) == ch (engine::Lane::vox, 5));
            CHECK (line (engine::Lane::accents) == ch (engine::Lane::accents, 6));
            CHECK (line (engine::Lane::fx) == ch (engine::Lane::fx, 7));

            // The MIDI-in target in Lane mode.
            rig.controller.midiMode = engine::MidiInMode::lane;
            rig.controller.midiLane = engine::Lane::perc;
            editor.tick();
            CHECK (line (engine::Lane::perc).contains ("MIDI IN"));
            CHECK_FALSE (line (engine::Lane::hats).contains ("MIDI IN"));
        });
}

TEST_CASE ("night bus's grid shows the loop's kick, clap and bass and the 16th playing", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            showKit (rig, editor, "FUT01");
            auto& lcd = get<editor::Lcd> (editor, "lcd");
            playAt (rig, editor, 18.0 * 240.0);
            auto grid = lcd.grid();
            CHECK (grid.page == 0);
            CHECK (grid.playing == 18);
            for (int i = 0; i < 32; ++i)
            {
                INFO ("step " << i);
                const auto kick = grid.rows[0][(std::size_t) i];
                if (i == 0 || i == 10 || i == 16 || i == 26)
                    CHECK (kick == StepCell::accent);
                else if (i == 23)
                    CHECK (kick == StepCell::hit);
                else
                    CHECK ((kick == StepCell::off || kick == StepCell::held));
                const auto clap = grid.rows[1][(std::size_t) i];
                const auto onset = i == 4 || i == 12 || i == 20 || i == 28 || i == 31;
                CHECK ((clap == StepCell::hit || clap == StepCell::accent) == onset);
            }
            playAt (rig, editor, 40.0 * 240.0);
            CHECK (lcd.grid().page == 32);
            CHECK (lcd.grid().playing == 40);
            stop (rig, editor);
            CHECK (lcd.grid().page == 0);
            CHECK (lcd.grid().playing == -1);
            CHECK (lcd.grid().rows[0][0] == StepCell::accent); // stopped, the file's first page
        });
}

TEST_CASE ("mission HUD's orbit puts the bar's arp notes on their octave's ring", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            showKit (rig, editor, "FUT02");
            auto& lcd = get<editor::Lcd> (editor, "lcd");
            playAt (rig, editor, 3.0 * 240.0);
            const auto sats = lcd.satellites();
            REQUIRE (sats.size() == 15);
            std::set<int> rings;
            for (const auto& s : sats)
            {
                rings.insert (s.ring);
                CHECK (s.ring == (s.key >= 72 ? 1 : 0)); // A4 on the inner ring, octave 5 on the next
                if (s.step == 3)
                {
                    CHECK (s.key == 81); // A5
                    CHECK (s.state == editor::Lcd::Satellite::State::sounding);
                }
                else if (s.step < 3)
                    CHECK (s.state == editor::Lcd::Satellite::State::played);
                else
                    CHECK (s.state == editor::Lcd::Satellite::State::upcoming);
            }
            CHECK (rings == std::set<int> { 0, 1 });
            const auto lines = lcd.orbitLines();
            const auto dot = editor::middleDot();
            REQUIRE (lines.size() == 4);
            CHECK (lines[0] == "CHROME ARP" + dot + "LOW A4");
            CHECK (lines[1] == "TOP E5" + dot + "F5" + dot + "G5" + dot + "A5");
            CHECK (lines[2] == "LOOP" + dot + "BAR 1 OF 4");
            CHECK (lines[3] == "Dm" + dot + "7A" + dot + "130 BPM");
        });
}

TEST_CASE ("deep glass's waves follow the drums and the music, with a dot at each kick", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            showKit (rig, editor, "FUT03");
            auto& lcd = get<editor::Lcd> (editor, "lcd");
            for (int i = 0; i < 30; ++i)
            {
                auto& s = rig.controller.nextSnapshot;
                s = {};
                s.playing = true;
                s.bpm = 128.0;
                s.songTicks = 120.0 * i;
                s.laneHits = i % 8 == 0 ? engine::laneBit (engine::Lane::kick) : 0u;
                s.lanePeak[(std::size_t) engine::Lane::kick] = i % 8 == 0 ? 1.0f : 0.1f;
                s.lanePeak[(std::size_t) engine::Lane::bass] = 0.5f;
                editor.tick();
            }
            const auto& h = lcd.history();
            REQUIRE (h.drums.size() >= 30);
            REQUIRE (h.music.size() == h.drums.size());
            REQUIRE (h.kicks.size() == h.drums.size());
            CHECK (h.music.back() == Catch::Approx (juce::Decibels::gainToDecibels (0.5f)).margin (0.5));
            int kicks = 0;
            for (std::size_t i = h.kicks.size() - 30; i < h.kicks.size(); ++i)
                kicks += h.kicks[i] ? 1 : 0;
            CHECK (kicks == 4);
            CHECK (h.kicks[h.kicks.size() - 30]);

            // Stopped: both fall to the floor, then the display stops asking to be painted.
            for (int i = 0; i < 120; ++i)
                stop (rig, editor);
            CHECK (lcd.history().drums.back() <= -47.9f);
            CHECK (lcd.history().music.back() <= -47.9f);
            editor::RepaintLedger::start();
            for (int i = 0; i < 10; ++i)
                stop (rig, editor);
            for (const auto& r : editor::RepaintLedger::stop())
                CHECK (r.component != &lcd);
        });
}

TEST_CASE ("machine room's scope draws the plugin's output, triggered on the low band", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            showKit (rig, editor, "FUT04");
            CHECK (rig.controller.scopeOn);
            auto& lcd = get<editor::Lcd> (editor, "lcd");

            // No samples: flat lines.
            editor.tick();
            REQUIRE (lcd.scopeTrace().out.size() == 556);
            for (const auto y : lcd.scopeTrace().out)
                CHECK (y == 0.0f);

            for (const int tickSize : { 1600, 1470 })
            {
                INFO ("ticks of " << tickSize);
                int sample = 0;
                std::vector<float> previous;
                for (int tick = 0; tick < 12; ++tick)
                {
                    rig.controller.scopeSamples = scopeSignal (sample, tickSize);
                    sample += tickSize;
                    playAt (rig, editor, 960.0 * tick);
                    const auto& trace = lcd.scopeTrace();
                    REQUIRE (trace.out.size() == 556);
                    REQUIRE (trace.sub.size() == 556);
                    if (tick < 4)
                        continue; // the history fills
                    // The low band starts at a rising zero crossing.
                    CHECK (std::abs (trace.sub.front()) < 2.0f);
                    CHECK (trace.sub[12] > trace.sub.front());
                    float peak = 0.0f;
                    for (const auto y : trace.out)
                        peak = std::max (peak, std::abs (y));
                    CHECK (peak == Catch::Approx (0.5f * 34.0f).margin (3.0f));
                    // Triggered: the picture stands still from tick to tick.
                    if (! previous.empty())
                    {
                        float diff = 0.0f;
                        for (std::size_t i = 0; i < 556; ++i)
                            diff += std::abs (trace.sub[i] - previous[i]);
                        CHECK (diff / 556.0f < 1.5f);
                    }
                    previous = trace.sub;
                }
            }
        });
}

TEST_CASE ("machine room's lanes light the bar's 16 steps", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            showKit (rig, editor, "FUT04");
            const auto cells = [&] (engine::Lane l)
            {
                juce::String s;
                for (const auto c : lane (editor, l).stepCells())
                    s << (c == StepCell::accent ? "X" : c == StepCell::hit ? "x" : c == StepCell::held ? "-" : ".");
                return s;
            };
            stop (rig, editor);
            CHECK (cells (engine::Lane::kick) == "X---X---X---X---");
            CHECK (cells (engine::Lane::hats) == "XxXXXxXXXxXXXxX.");
            CHECK (lane (editor, engine::Lane::kick).playingCell() == -1);
            playAt (rig, editor, 5.0 * 240.0);
            CHECK (lane (editor, engine::Lane::kick).playingCell() == 5);
            CHECK (cells (engine::Lane::kick) == "X---X---X---X---");
            playAt (rig, editor, 21.0 * 240.0); // bar 2
            CHECK (lane (editor, engine::Lane::kick).playingCell() == 5);
        });
}

TEST_CASE ("a tick repaints only what changed, and never the whole face", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                if (kit == "FUT04")
                    rig.controller.scopeSamples = scopeSignal (0, 1600);
                for (int i = 0; i < 5; ++i)
                    playAt (rig, editor, 120.0 * i, i % 2 == 0 ? 0x3FFu : 0u, 0.2f + 0.1f * (float) i);

                editor::RepaintLedger::start();
                for (int i = 5; i < 35; ++i)
                {
                    if (kit == "FUT04")
                        rig.controller.scopeSamples = scopeSignal (1600 * i, 1600);
                    playAt (rig, editor, 120.0 * i, i % 2 == 0 ? 0x3FFu : 0u, 0.2f + 0.02f * (float) (i % 10));
                }
                const auto requests = editor::RepaintLedger::stop();
                CHECK_FALSE (requests.empty());
                for (const auto& r : requests)
                {
                    INFO (r.component->getComponentID() << " " << r.area.toString());
                    CHECK (r.component != &face (editor));
                    CHECK (r.component != &editor);
                    CHECK (r.component->getLocalBounds().contains (r.area));
                    CHECK (r.area.getWidth() * r.area.getHeight() < 1120 * 700 / 4);
                }

                // Nothing changes: nothing repaints.
                rig.controller.nextSnapshot = {};
                for (int i = 0; i < 60; ++i)
                    editor.tick();
                editor::RepaintLedger::start();
                for (int i = 0; i < 30; ++i)
                    editor.tick();
                const auto idle = editor::RepaintLedger::stop();
                for (const auto& r : idle)
                    INFO (r.component->getComponentID() << " " << r.area.toString());
                CHECK (idle.empty());
            }
        });
}

TEST_CASE ("the backdrop renders once per look and zoom", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            paintFace (editor);
            const auto first = editor.backdropRenders();
            CHECK (first >= 1);
            paintFace (editor);
            playAt (rig, editor, 960.0);
            paintFace (editor);
            CHECK (editor.backdropRenders() == first);

            showKit (rig, editor, "FUT02");
            paintFace (editor);
            CHECK (editor.backdropRenders() == first + 1);
            paintFace (editor);
            CHECK (editor.backdropRenders() == first + 1);

            editor.setZoom (1.5f);
            paintFace (editor);
            CHECK (editor.backdropRenders() == first + 2);
            editor.setZoom (1.0f);
        });
}

TEST_CASE ("the drawers and the content panel wear the look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                const auto& c = editor.skin().colours;
                for (const auto* button : { "samples", "phrases" })
                {
                    INFO (button);
                    click (get<juce::Component> (editor, button));
                    editor.tick();
                    auto& drawer = get<juce::Component> (editor, juce::String (button) + "_drawer");
                    REQUIRE (drawer.isVisible());
                    CHECK (faceBounds (editor, drawer) == juce::Rectangle<int> (20, 246, 1080, 396));
                    REQUIRE (dynamic_cast<editor::DrawnTextSource*> (&drawer) != nullptr);
                    CHECK (dynamic_cast<editor::DrawnTextSource&> (drawer).drawnText().size() >= 4);
                    checkFits (drawer);
                    const auto image = paintFace (editor);
                    const auto inside = image.getPixelAt (20 + 6, 246 + 200); // the frame's fill, left of the content
                    const auto panel = editor::over (c.panel, c.face);
                    CHECK (std::abs (inside.getRed() - panel.getRed()) <= 24);
                    CHECK (std::abs (inside.getGreen() - panel.getGreen()) <= 24);
                    CHECK (std::abs (inside.getBlue() - panel.getBlue()) <= 24);
                    click (get<juce::Component> (editor, button));
                    editor.tick();
                }

                // The content panel's headline and path fit too.
                rig.controller.contentStatus.state = wubansia::ContentState::missing;
                editor.tick();
                auto& content = get<juce::Component> (editor, "content");
                REQUIRE (content.isVisible());
                REQUIRE (dynamic_cast<editor::DrawnTextSource*> (&content) != nullptr);
                checkFits (content);
                rig.controller.contentStatus.state = wubansia::ContentState::ready;
                editor.tick();
            }
        });
}

TEST_CASE ("while the kit isn't ready, the display and its pill say so in every look", "[editor][looks]")
{
    LooksRig rig;
    withEditor (rig.processor, [&] (PluginEditor& editor)
        {
            for (const auto& kit : kKits)
            {
                INFO (kit);
                showKit (rig, editor, kit);
                auto& lcd = get<juce::Component> (editor, "lcd");
                auto& pill = get<juce::Component> (editor, "kit_" + kit);
                const std::vector<std::pair<wubansia::ContentState, juce::String>> states {
                    { wubansia::ContentState::loading, "LOADING 42 %" }, { wubansia::ContentState::missing, "NO SAMPLES" },
                    { wubansia::ContentState::mismatch, "WRONG FILES" } };
                for (const auto& [state, words] : states)
                {
                    rig.controller.contentStatus.state = state;
                    rig.controller.contentStatus.percent = 42;
                    editor.tick();
                    INFO (joinedText (lcd));
                    CHECK (anyTextContains (lcd, words));
                    CHECK (dynamic_cast<editor::DrawnTextSource*> (&pill) != nullptr);
                }
                rig.controller.contentStatus.state = wubansia::ContentState::ready;
                editor.tick();
                CHECK_FALSE (anyTextContains (lcd, "NO SAMPLES"));
            }
        });
}
