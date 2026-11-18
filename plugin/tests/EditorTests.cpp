// The editor (P10), built as a host builds it but with no native window, against the FakeController
// (helpers/fake_controller.h) and the test processor holding the real parameter layout
// (helpers/test_processor.h). Painting into an image runs the drawing code; synthetic mouse events
// drive the controls the way a person does.
//
// The hidden [.screenshot] case renders the editor at 2x (and the Samples drawer at 1x too) into PNG
// files in $WUBANSIA_SCREENSHOTS.

#include "PluginEditor.h"
#include "content/EmbeddedContent.h"
#include "helpers/fake_controller.h"
#include "helpers/test_processor.h"
#include "services/KitCatalog.h"
#include "sounds/SampleExport.h"

#include "kit/file_sample_source.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <tuple>
#include <utility>

#if JUCE_LINUX || JUCE_BSD
    #include <dlfcn.h>
#endif

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::EndsWith;
using Catch::Matchers::WithinAbs;
using wubansia::PluginEditor;
using wubansia::test::FakeController;
using wubansia::test::TestProcessor;
namespace editor = wubansia::editor;
namespace engine = wubansia::engine;
namespace ids = wubansia::ParameterIds;

namespace
{
    // A processor whose editor talks to a FakeController.
    struct Rig
    {
        FakeController controller;
        TestProcessor processor { [this] (TestProcessor& p) { return new PluginEditor (p, controller); } };
    };

    // Creates the editor as a host does (no native window), runs the code, then deletes it, however the code
    // ends: a failed REQUIRE throws, and an editor left behind would go on ticking against a controller that's gone.
    void withEditor (TestProcessor& processor, const std::function<void (PluginEditor&)>& code)
    {
        auto* created = processor.createEditorAndMakeActive();
        REQUIRE (created != nullptr);
        struct Closer
        {
            TestProcessor& owner;
            juce::AudioProcessorEditor* opened;

            ~Closer()
            {
                owner.editorBeingDeleted (opened);
                delete opened;
            }
        } closer { processor, created };
        auto* editorComponent = dynamic_cast<PluginEditor*> (created);
        REQUIRE (editorComponent != nullptr);
        code (*editorComponent);
    }

    void withEditor (Rig& rig, const std::function<void (PluginEditor&)>& code) { withEditor (rig.processor, code); }

    void collectById (juce::Component& root, const juce::String& id, std::vector<juce::Component*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (child->getComponentID() == id)
                found.push_back (child);
            collectById (*child, id, found);
        }
    }

    std::vector<juce::Component*> findAll (juce::Component& root, const juce::String& id)
    {
        std::vector<juce::Component*> found;
        collectById (root, id, found);
        return found;
    }

    template <class T>
    T& get (juce::Component& root, const juce::String& id)
    {
        const auto found = findAll (root, id);
        INFO ("component " << id);
        REQUIRE (found.size() == 1);
        auto* typed = dynamic_cast<T*> (found.front());
        REQUIRE (typed != nullptr);
        return *typed;
    }

    // A mouse event at a point in the component, as the left button reports it.
    juce::MouseEvent mouseAt (juce::Component& component, juce::Point<float> position, juce::Point<float> downAt, bool dragged = false)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
            juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
            juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, &component, &component, now, downAt, now, 1, dragged };
    }

    // A click: down and up at one point.
    void click (juce::Component& component, juce::Point<float> at)
    {
        component.mouseDown (mouseAt (component, at, at));
        component.mouseUp (mouseAt (component, at, at));
    }

    void click (juce::Component& component) { click (component, component.getLocalBounds().toFloat().getCentre()); }

    // A vertical drag on a knob or fader: down at its centre, up by `pixels`, release.
    void dragUp (juce::Component& component, float pixels)
    {
        const auto start = component.getLocalBounds().toFloat().getCentre();
        component.mouseDown (mouseAt (component, start, start));
        for (int step = 1; step <= 4; ++step)
            component.mouseDrag (mouseAt (component, start.translated (0.0f, -pixels * (float) step / 4.0f), start, true));
        component.mouseUp (mouseAt (component, start.translated (0.0f, -pixels), start, true));
    }

    // What the host hears: gestures and values, in order.
    struct GestureLog : juce::AudioProcessorListener
    {
        struct Event
        {
            char type; // 'b' begin, 'v' value, 'e' end
            int index;
            float value;
        };
        std::vector<Event> events;

        void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float value) override { events.push_back ({ 'v', index, value }); }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override { events.push_back ({ 'b', index, 0.0f }); }
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override { events.push_back ({ 'e', index, 0.0f }); }

        // Every value change of the parameter came between a begin and an end, and there was one.
        bool changedOnlyInsideGestures (int index) const
        {
            auto open = false, changed = false;
            for (const auto& e : events)
            {
                if (e.index != index)
                    continue;
                if (e.type == 'b')
                    open = true;
                else if (e.type == 'e')
                    open = false;
                else if (! open)
                    return false;
                else
                    changed = true;
            }
            return changed && ! open;
        }
    };

    // The plain value a parameter shows on its control.
    float shownValue (juce::Component& control, juce::RangedAudioParameter& parameter)
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (&control))
            return (float) slider->getValue();
        if (auto* button = dynamic_cast<juce::Button*> (&control))
            return button->getToggleState() ? 1.0f : 0.0f;
        if (auto* box = dynamic_cast<juce::ComboBox*> (&control))
            return parameter.getNormalisableRange().start + (float) box->getSelectedItemIndex();
        return std::nanf ("");
    }

    // A value other than the parameter's default: for the "set from the current value" checks.
    float otherValue (juce::RangedAudioParameter& parameter)
    {
        const auto id = parameter.getParameterID();
        const auto def = parameter.convertFrom0to1 (parameter.getDefaultValue());
        if (id.endsWith ("_level") || id == ids::masterGain)
            return -12.5f;
        if (id == ids::space || id == ids::echo || id == ids::pump)
            return 150.0f;
        return std::fmod (def + 1.0f, (float) parameter.getNumSteps());
    }

    editor::LaneStrip& lane (juce::Component& root, engine::Lane l)
    {
        return get<editor::LaneStrip> (root, "lane_" + juce::String (std::string (engine::laneId (l))));
    }

    void addKit (FakeController& controller, const juce::String& id, const juce::String& title, const juce::String& direction,
        const juce::String& key, const juce::String& camelot, int bpm, const std::array<juce::String, engine::kLanes>& labels,
        std::uint32_t phraseLanes, const std::array<juce::String, 3>& abA, const std::array<juce::String, 3>& abB)
    {
        wubansia::KitCard card;
        card.id = id;
        card.title = title;
        card.direction = direction;
        card.key = key;
        card.camelot = camelot;
        card.bpm = bpm;
        card.contentVersion = 1;
        card.laneLabels = labels;
        card.phraseLanes = phraseLanes;
        card.abA = abA;
        card.abB = abB;
        card.content.state = wubansia::ContentState::ready;
        controller.kitCards.push_back (card);
    }

    // A fresh temp folder, deleted with this.
    struct TempFolder
    {
        TempFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaEditorTest", "", false)) {}
        ~TempFolder() { folder.deleteRecursively(); }
        juce::File folder;
    };

    // Opens the Samples drawer as the header's button does, and returns it.
    editor::SamplesPanel& openDrawer (PluginEditor& editor)
    {
        if (! editor.isSamplesPanelShown())
            click (get<juce::Button> (editor, "samples"));
        editor.tick();
        REQUIRE (editor.isSamplesPanelShown());
        return get<editor::SamplesPanel> (editor, "samples_drawer");
    }

    editor::SoundRow& soundRow (juce::Component& root, int index)
    {
        return get<editor::SoundRow> (root, "sound_row_" + juce::String (index));
    }

    // A press on a row, away from its play mark: it selects the row and plays it.
    void press (editor::SoundRow& row)
    {
        const auto at = juce::Point<float> ((float) row.getWidth() * 0.6f, (float) row.getHeight() * 0.5f);
        row.mouseDown (mouseAt (row, at, at));
        row.mouseUp (mouseAt (row, at, at));
    }

    // A press on a row's play or stop mark.
    void pressMark (editor::SoundRow& row)
    {
        const auto at = row.markArea().toFloat().getCentre();
        REQUIRE (row.getLocalBounds().toFloat().contains (at));
        row.mouseDown (mouseAt (row, at, at));
        row.mouseUp (mouseAt (row, at, at));
    }

    // Down in the middle, a drag `pixels` to the right, and up there.
    void drag (juce::Component& component, float pixels)
    {
        const auto start = component.getLocalBounds().toFloat().getCentre();
        component.mouseDown (mouseAt (component, start, start));
        component.mouseDrag (mouseAt (component, start.translated (pixels, 0.0f), start, true));
        component.mouseUp (mouseAt (component, start.translated (pixels, 0.0f), start, true));
    }

    // What the drawer hands the OS, recorded instead of done.
    struct Handoffs
    {
        juce::Array<juce::File> dragged, revealed;

        void hook (editor::SamplesPanel& drawer)
        {
            drawer.startDrag = [this] (const juce::File& file, juce::Component&)
            {
                dragged.add (file);
                return true;
            };
            drawer.revealFile = [this] (const juce::File& file) { revealed.add (file); };
        }
    };

    // Whether a click at the middle of a component lands in the drawer instead. It asks the zoomed face the
    // drawer sits on: the editor itself is only visible once a host puts it in a window.
    bool coveredByDrawer (PluginEditor& editor, juce::Component& component)
    {
        auto& drawer = get<editor::SamplesPanel> (editor, "samples_drawer");
        auto* face = drawer.getParentComponent();
        REQUIRE (face != nullptr);
        auto* hit = face->getComponentAt (face->getLocalPoint (&component, component.getLocalBounds().getCentre()));
        REQUIRE (hit != nullptr);
        return hit == &drawer || drawer.isParentOf (hit);
    }
}

TEST_CASE ("the editor opens and closes 20 times with no native window", "[editor]")
{
    Rig rig;
    REQUIRE (rig.processor.hasEditor());

    for (int i = 0; i < 20; ++i)
        withEditor (rig, [] (PluginEditor& editor)
            {
                CHECK (editor.getPeer() == nullptr);
                CHECK (editor.getWidth() == juce::roundToInt ((float) PluginEditor::baseWidth * editor.getZoom()));
                CHECK (editor.getHeight() == juce::roundToInt ((float) PluginEditor::baseHeight * editor.getZoom()));

                juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
                {
                    juce::Graphics g (image);
                    editor.paintEntireComponent (g, false);
                }
                CHECK (image.getPixelAt (editor.getWidth() / 2, 2).getAlpha() == 255); // the background was painted
                CHECK (image.getPixelAt (editor.getWidth() - 3, editor.getHeight() - 3).getAlpha() == 255);
            });

    CHECK (rig.processor.getActiveEditor() == nullptr);
}

TEST_CASE ("the output knob is master_gain", "[editor]")
{
    Rig rig;
    rig.processor.set (ids::masterGain, -6.0f);

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& knob = get<juce::Slider> (editor, ids::masterGain);
            CHECK_THAT (knob.getValue(), WithinAbs (-6.0, 1.0e-3)); // shows the current value

            knob.setValue (-12.0, juce::sendNotificationSync);
            CHECK_THAT (rig.processor.value (ids::masterGain), WithinAbs (-12.0, 1.0e-3));
        });
}

TEST_CASE ("every parameter has exactly one control whose component ID is its ID", "[editor]")
{
    Rig rig;

    // Move every parameter off its default first, so the controls must show the current value.
    for (auto* base : rig.processor.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (otherValue (*parameter)));
    }

    withEditor (rig, [&] (PluginEditor& editor)
        {
            REQUIRE (rig.processor.getParameters().size() == 30);
            for (auto* base : rig.processor.getParameters())
            {
                auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base);
                const auto id = parameter->getParameterID();
                INFO (id);

                const auto controls = findAll (editor, id);
                REQUIRE (controls.size() == 1);
                auto& control = *controls.front();
                CHECK (control.isEnabled());
                CHECK_THAT (shownValue (control, *parameter), WithinAbs (otherValue (*parameter), 1.0e-3));

                // Move it the way a person does, and check the host saw the change inside a gesture.
                GestureLog log;
                rig.processor.addListener (&log);
                const auto before = parameter->getValue();
                if (auto* slider = dynamic_cast<juce::Slider*> (&control))
                    dragUp (*slider, 40.0f);
                else if (auto* ab = dynamic_cast<editor::AbSwitch*> (&control))
                    click (*ab, { ab->getToggleState() ? 4.0f : (float) ab->getWidth() - 4.0f, (float) ab->getHeight() * 0.5f });
                else if (auto* box = dynamic_cast<juce::ComboBox*> (&control))
                {
                    const auto next = (box->getSelectedItemIndex() + 1) % box->getNumItems();
                    click (*box, { ((float) next + 0.5f) * (float) box->getWidth() / (float) box->getNumItems(), (float) box->getHeight() * 0.5f });
                }
                else
                    click (control);
                rig.processor.removeListener (&log);

                CHECK_FALSE (juce::exactlyEqual (parameter->getValue(), before));
                CHECK (log.changedOnlyInsideGestures (parameter->getParameterIndex()));
                CHECK_THAT (shownValue (control, *parameter), WithinAbs (parameter->convertFrom0to1 (parameter->getValue()), 1.0e-3));
            }
        });
}

TEST_CASE ("a right-click opens the parameter's menu and never moves or toggles the control", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            for (const auto* id : { ids::laneLevel[1], ids::space, ids::laneMute[1], ids::voxSynth, ids::bassAb, ids::playMode })
            {
                INFO (id);
                auto* control = findAll (editor, id).front();
                auto* hook = dynamic_cast<editor::ContextMenuHook*> (control);
                REQUIRE (hook != nullptr);
                REQUIRE (hook->onContextMenu != nullptr); // the editor installed the host menu
                auto menus = 0;
                hook->onContextMenu = [&menus] { ++menus; };

                const auto before = rig.processor.parameter (id).getValue();
                const auto at = control->getLocalBounds().toFloat().getCentre();
                const auto right = [&] (juce::Point<float> p, bool dragged)
                {
                    const auto now = juce::Time::getCurrentTime();
                    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier),
                        juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
                        juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, control, control, now, at, now, 1, dragged);
                };
                control->mouseDown (right (at, false));
                control->mouseDrag (right (at.translated (0.0f, -40.0f), true));
                control->mouseUp (right (at.translated (0.0f, -40.0f), true));

                CHECK (menus == 1);
                CHECK (juce::exactlyEqual (rig.processor.parameter (id).getValue(), before));
            }
        });
}

TEST_CASE ("double-clicking a fader returns it to its default inside a gesture", "[editor]")
{
    Rig rig;
    rig.processor.set (ids::laneLevel[4], -20.0f);

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& fader = get<juce::Slider> (editor, ids::laneLevel[4]);
            GestureLog log;
            rig.processor.addListener (&log);
            const auto centre = fader.getLocalBounds().toFloat().getCentre();
            fader.mouseDoubleClick (mouseAt (fader, centre, centre));
            rig.processor.removeListener (&log);

            CHECK_THAT (rig.processor.value (ids::laneLevel[4]), WithinAbs (0.0, 1.0e-4));
            CHECK (log.changedOnlyInsideGestures (rig.processor.parameter (ids::laneLevel[4]).getParameterIndex()));
        });
}

TEST_CASE ("the editor never uses JUCE's parameter attachments or listeners", "[editor]")
{
    const auto source = juce::File (__FILE__).getParentDirectory().getParentDirectory().getChildFile ("source");
    auto files = source.getChildFile ("editor").findChildFiles (juce::File::findFiles, false, "*.h;*.cpp");
    REQUIRE (files.size() >= 20); // the editor's sources are there, so the check isn't vacuous
    files.add (source.getChildFile ("PluginEditor.h"));
    files.add (source.getChildFile ("PluginEditor.cpp"));

    const juce::StringArray banned { "SliderAttachment", "ButtonAttachment", "ComboBoxAttachment", "ParameterAttachment",
        "AudioProcessorParameter::Listener", "addParameterListener", "addListener (this)" };
    for (const auto& file : files)
    {
        REQUIRE (file.existsAsFile());
        const auto text = file.loadFileAsString();
        for (const auto& word : banned)
        {
            INFO (file.getFileName() << " uses " << word);
            CHECK_FALSE (text.contains (word));
        }
    }
}

TEST_CASE ("a parameter changed on another thread shows within two timer ticks", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            const auto fromAnotherThread = [&] (const char* id, float plain)
            {
                auto& parameter = rig.processor.parameter (id);
                std::thread host ([&] { parameter.setValueNotifyingHost (parameter.convertTo0to1 (plain)); });
                host.join();
            };

            for (const auto& [id, plain] : { std::pair { ids::laneLevel[2], -18.0f }, std::pair { ids::laneMute[5], 1.0f },
                     std::pair { ids::playMode, 2.0f }, std::pair { ids::kickAb, 0.0f }, std::pair { ids::space, 175.0f } })
            {
                INFO (id);
                auto& control = *findAll (editor, id).front();
                auto& parameter = rig.processor.parameter (id);
                fromAnotherThread (id, plain);

                auto ticks = 0;
                while (ticks < 2 && std::abs (shownValue (control, parameter) - plain) > 1.0e-3f)
                {
                    editor.tick();
                    ++ticks;
                }
                CHECK_THAT (shownValue (control, parameter), WithinAbs (plain, 1.0e-3));
            }

            // While a person holds a fader, automation doesn't yank it; it shows once they let go.
            auto& fader = get<juce::Slider> (editor, ids::laneLevel[0]);
            fader.onDragStart();
            fromAnotherThread (ids::laneLevel[0], -30.0f);
            editor.tick();
            CHECK_THAT (fader.getValue(), WithinAbs (0.0, 1.0e-3));
            fader.onDragEnd();
            editor.tick();
            CHECK_THAT (fader.getValue(), WithinAbs (-30.0, 1.0e-3));
        });
}

TEST_CASE ("kit cards select kits, and A/B switches follow the kit", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& fut01 = get<editor::KitPanel::Card> (editor, "kit_FUT01");
            auto& fut02 = get<editor::KitPanel::Card> (editor, "kit_FUT02");
            CHECK (fut01.isSelected());
            CHECK_FALSE (fut02.isSelected());
            CHECK (fut01.statusText() == "READY");
            CHECK (fut02.statusText() == "NO SAMPLES");

            // FUT01 has all three alternatives, named its way.
            for (int i = 0; i < 3; ++i)
                CHECK (get<editor::AbSwitch> (editor, ids::ab[(std::size_t) i]).isEnabled());
            CHECK (get<editor::AbSwitch> (editor, ids::kickAb).labelA() == "Kick Split");
            CHECK (get<editor::AbSwitch> (editor, ids::kickAb).labelB() == "Kick Garage");

            click (fut02);
            CHECK (rig.controller.selectedKits == juce::StringArray { "FUT02" });
            editor.tick();
            CHECK (fut02.isSelected());
            CHECK_FALSE (fut01.isSelected());

            // FUT02 has only a bass alternative.
            CHECK_FALSE (get<editor::AbSwitch> (editor, ids::kickAb).isEnabled());
            CHECK (get<editor::AbSwitch> (editor, ids::bassAb).isEnabled());
            CHECK_FALSE (get<editor::AbSwitch> (editor, ids::stabAb).isEnabled());
            CHECK (get<editor::AbSwitch> (editor, ids::bassAb).labelA() == "Roll Bass Clean");
            CHECK (get<editor::AbSwitch> (editor, ids::bassAb).labelB() == "Roll Bass Warm");
            CHECK (editor.model().accent == editor::accentForKit ("FUT02"));
            CHECK (editor.model().accent != editor::accentForKit ("FUT01"));
        });
}

TEST_CASE ("every kit card reads its direction in full at 100 %", "[editor]")
{
    Rig rig;
    addKit (rig.controller, "FUT03", "Glass Current", "garage-house / deep tech house", "G minor", "6A", 128, {}, 0, {}, {});
    addKit (rig.controller, "FUT04", "Furnace Code", "dark industrial / techno", "E minor", "9A", 136, {}, 0, {}, {});

    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            for (const auto* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
            {
                INFO (id);
                auto& card = get<editor::KitPanel::Card> (editor, "kit_" + juce::String (id));
                const auto font = card.directionFont();
                CHECK (juce::GlyphArrangement::getStringWidth (font, card.card().direction) <= card.directionArea().getWidth());
                CHECK (font.getHeight() >= 8.0f);
            }
        });
}

TEST_CASE ("lanes are labelled the kit's way, and lanes the kit lacks are dimmed", "[editor]")
{
    Rig rig;
    rig.controller.kitCards[0].laneLabels[3] = ""; // FUT01 without a perc lane

    withEditor (rig, [&] (PluginEditor& editor)
        {
            CHECK (lane (editor, engine::Lane::perc).isDimmed());
            CHECK (lane (editor, engine::Lane::perc).getAlpha() < 0.5f);
            CHECK_FALSE (get<editor::DragChip> (editor, "drag_perc").isEnabled());
            for (const auto l : { engine::Lane::kick, engine::Lane::snare, engine::Lane::bass, engine::Lane::fx })
            {
                CHECK_FALSE (lane (editor, l).isDimmed());
                CHECK (juce::exactlyEqual (lane (editor, l).getAlpha(), 1.0f));
            }
            CHECK (get<editor::DragChip> (editor, "drag_snare").label() == "Clap");

            rig.controller.selectKit ("FUT02");
            editor.tick();
            CHECK_FALSE (lane (editor, engine::Lane::perc).isDimmed());
            CHECK (get<editor::DragChip> (editor, "drag_lead").label() == "Arp");
        });
}

TEST_CASE ("phrase lanes say whether they play their recording or the multisample", "[editor]")
{
    Rig rig; // FUT01 at 134 BPM: lead and vox are phrase lanes

    withEditor (rig, [&] (PluginEditor& editor)
        {
            const auto tags = [&]
            {
                editor.tick();
                return juce::StringArray { lane (editor, engine::Lane::lead).phraseTag(), lane (editor, engine::Lane::vox).phraseTag() };
            };

            CHECK (lane (editor, engine::Lane::bass).phraseTag().isEmpty()); // not a phrase lane in FUT01
            CHECK (tags() == juce::StringArray { "recorded", "recorded" }); // stopped, at the kit's tempo

            rig.controller.transportState.bpm = 136.0; // within 2 %
            CHECK (tags() == juce::StringArray { "recorded", "recorded" });
            rig.controller.transportState.bpm = 120.0;
            CHECK (tags() == juce::StringArray { "multisample", "multisample" });

            rig.controller.transportState.bpm = 134.0;
            rig.controller.phrases = engine::PhraseMode::off;
            CHECK (tags() == juce::StringArray { "multisample", "multisample" });

            // While playing, the engine's own answer wins.
            rig.controller.phrases = engine::PhraseMode::automatic;
            rig.controller.nextSnapshot.playing = true;
            rig.controller.nextSnapshot.bpm = 134.0;
            rig.controller.nextSnapshot.phraseRecorded = engine::laneBit (engine::Lane::lead);
            CHECK (tags() == juce::StringArray { "recorded", "multisample" });

            // The player off: only MIDI in plays, which never plays recordings.
            rig.controller.nextSnapshot.playing = false;
            rig.processor.set (ids::playMode, 0.0f);
            CHECK (tags() == juce::StringArray { "multisample", "multisample" });
        });
}

TEST_CASE ("the content panel says what to do for every status", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& panel = get<editor::ContentPanel> (editor, "content");
            panel.platform = editor::ContentPanel::Platform::macOS;
            auto& card = get<editor::KitPanel::Card> (editor, "kit_FUT01");
            const auto show = [&] (wubansia::ContentState state, const juce::String& problem = {}, int percent = 0)
            {
                rig.controller.contentStatus = {};
                rig.controller.contentStatus.state = state;
                rig.controller.contentStatus.problem = problem;
                rig.controller.contentStatus.percent = percent;
                rig.controller.contentStatus.folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Kits");
                rig.controller.contentStatus.searched = { "/tmp/env-content", "/Users/someone/Library/Application Support/Wubansia Audio/Wubansia/Content" };
                editor.tick();
            };

            show (wubansia::ContentState::ready);
            CHECK (panel.headline() == "FUT01's samples are ready");
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("~/Kits"));
            CHECK_FALSE (editor.isContentPanelShown());
            CHECK (card.statusText() == "READY");

            show (wubansia::ContentState::loading, {}, 42);
            CHECK_THAT (panel.headline().toStdString(), ContainsSubstring ("Loading FUT01") && ContainsSubstring ("42 %"));
            CHECK_FALSE (editor.isContentPanelShown());
            CHECK (card.statusText() == "LOADING 42%");
            CHECK (get<editor::Lcd> (editor, "lcd").lines().status == "LOADING 42%");

            show (wubansia::ContentState::missing, "no folder holds FUT01");
            CHECK (panel.headline() == "FUT01's samples aren't installed");
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("install-macos.sh") && ContainsSubstring ("'fut01' folder")
                                                           && ContainsSubstring ("drags out"));
            CHECK (editor.isContentPanelShown());
            CHECK (card.statusText() == "NO SAMPLES");

            show (wubansia::ContentState::lfsPointers, "Drums/Kick Garage v1 rr1.wav is a Git LFS pointer");
            CHECK (panel.headline() == "FUT01's samples are Git LFS pointers, not audio");
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("Kick Garage v1 rr1.wav is a Git LFS pointer.")
                                                           && ContainsSubstring ("git lfs pull"));
            CHECK (editor.isContentPanelShown());

            show (wubansia::ContentState::mismatch, "Bass/Warp Warm F#1 v1.wav is 44.1 kHz");
            CHECK (panel.headline() == "FUT01's samples don't match this version");
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("44.1 kHz.") && ContainsSubstring ("content v1"));
            CHECK (card.statusText() == "WRONG FILES");

            show (wubansia::ContentState::failed, "Vox/Vox Synth A4 v1.wav can't be read");
            CHECK (panel.headline() == "FUT01's samples couldn't be loaded");
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("can't be read.") && ContainsSubstring ("another folder"));
            CHECK (editor.isContentPanelShown());

            // Linux gets its own folder.
            panel.platform = editor::ContentPanel::Platform::linuxOS;
            show (wubansia::ContentState::missing);
            CHECK_THAT (panel.message().toStdString(), ContainsSubstring ("~/.local/share/Wubansia Audio/Wubansia/Content"));

            // Closing hides it until the status changes; Content... in the Samples drawer brings it back.
            CHECK (get<juce::Button> (editor, "content_close").getTooltip() == editor::utf8 ("Hide this panel; Content\xe2\x80\xa6 in the Samples drawer brings it back"));
            click (get<juce::Button> (editor, "content_close"));
            editor.tick();
            CHECK_FALSE (editor.isContentPanelShown());
            click (get<juce::Button> (editor, "samples")); // the drawer, not the install panel
            editor.tick();
            CHECK (editor.isSamplesPanelShown());
            CHECK_FALSE (editor.isContentPanelShown());
            click (get<juce::Button> (editor, "samples_content"));
            editor.tick();
            CHECK_FALSE (editor.isSamplesPanelShown());
            CHECK (editor.isContentPanelShown());
            click (get<juce::Button> (editor, "content_close"));
            editor.tick();
            CHECK_FALSE (editor.isContentPanelShown());
            show (wubansia::ContentState::failed, "something else");
            CHECK (editor.isContentPanelShown());

            // Choosing a folder hands it to the Controller.
            const auto chosen = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WubansiaChosenContent");
            panel.chooseFolder = [&] (std::function<void (const juce::File&)> done) { done (chosen); };
            click (get<juce::Button> (editor, "content_folder"));
            REQUIRE (rig.controller.contentFolders.size() == 1);
            CHECK (rig.controller.contentFolders[0] == chosen);
        });
}

TEST_CASE ("drag chips write their MIDI file and hand it to the OS", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            juce::Array<juce::File> dragged, revealed;
            const auto hook = [&] (editor::DragChip& chip)
            {
                chip.startDrag = [&] (const juce::File& file, juce::Component&)
                {
                    dragged.add (file);
                    return true;
                };
                chip.revealFile = [&] (const juce::File& file) { revealed.add (file); };
            };

            auto& bass = get<editor::DragChip> (editor, "drag_bass");
            hook (bass);
            const auto start = bass.getLocalBounds().toFloat().getCentre();
            bass.mouseDown (mouseAt (bass, start, start));
            bass.mouseDrag (mouseAt (bass, start.translated (12.0f, 0.0f), start, true));
            bass.mouseUp (mouseAt (bass, start.translated (12.0f, 0.0f), start, true));
            REQUIRE (rig.controller.dragRequests.size() == 1);
            CHECK (rig.controller.dragRequests[0].what == wubansia::DragWhat::lane);
            CHECK (rig.controller.dragRequests[0].lane == engine::Lane::bass);
            REQUIRE (dragged.size() == 1);
            CHECK (dragged[0].existsAsFile());
            CHECK (dragged[0].getFileName() == "FUT01 - bass.mid");

            auto& loop = get<editor::DragChip> (editor, "drag_loop");
            auto& song = get<editor::DragChip> (editor, "drag_song");
            hook (loop);
            hook (song);
            CHECK (loop.beginDrag());
            CHECK (song.beginDrag());
            CHECK (rig.controller.dragRequests[1].what == wubansia::DragWhat::loop);
            CHECK (rig.controller.dragRequests[2].what == wubansia::DragWhat::song);

            auto& phrase = get<editor::DragChip> (editor, "drag_phrase_1");
            hook (phrase);
            CHECK (phrase.label() == "Vox Synth" + editor::middleDot() + "2 bars");
            CHECK (phrase.beginDrag());
            CHECK (rig.controller.dragRequests[3].what == wubansia::DragWhat::phrase);
            CHECK (rig.controller.dragRequests[3].phraseFile == "FUT01_Phrase_VoxSynth_2bars.mid");

            // A click without a drag shows the file instead.
            click (loop);
            REQUIRE (revealed.size() == 1);
            CHECK (revealed[0].getFileName() == "FUT01 - Loop.mid");

            // A file that can't be written starts no drag and says so.
            rig.controller.dragFails = true;
            CHECK_FALSE (song.beginDrag());
            CHECK (song.hasFailed());
            CHECK (dragged.size() == 4);

            for (const auto& file : dragged)
                file.deleteFile();
        });
}

namespace
{
    // FUT01's five phrase files, the most any kit has, in the order phraseFiles () gives them.
    juce::StringArray fut01PhraseFiles()
    {
        return { "FUT01_Phrase_EPianoStabRhythm_2bars.mid", "FUT01_Phrase_OrganRiff_4bars.mid", "FUT01_Phrase_PluckRiff_4bars.mid",
            "FUT01_Phrase_VoxSynthRiff_2bars.mid", "FUT01_Phrase_WarpRiff_2bars.mid" };
    }

    // The drag strip's 1 drum track switch, or nullptr after a failed check: a failed REQUIRE inside withEditor would
    // leave the editor ticking after its test.
    editor::LedButton* drumSwitch (PluginEditor& editor)
    {
        const auto found = findAll (editor, "drag_drums");
        CHECK (found.size() == 1);
        auto* drums = found.size() == 1 ? dynamic_cast<editor::LedButton*> (found.front()) : nullptr;
        CHECK (drums != nullptr);
        return drums;
    }

    // The drag strip's For my synths switch, or nullptr after a failed check, as drumSwitch ().
    editor::LedButton* synthSwitch (PluginEditor& editor)
    {
        const auto found = findAll (editor, "drag_synths");
        CHECK (found.size() == 1);
        auto* synths = found.size() == 1 ? dynamic_cast<editor::LedButton*> (found.front()) : nullptr;
        CHECK (synths != nullptr);
        return synths;
    }
}

TEST_CASE ("the drag strip's For my synths switch is lit by default, asks the Controller and follows it", "[editor]")
{
    Rig rig;
    rig.controller.phraseFileNames = fut01PhraseFiles();

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto* found = synthSwitch (editor);
            if (found == nullptr)
                return;
            auto& synths = *found;
            CHECK (synths.getButtonText() == "For my synths");
            CHECK (synths.isEnabled());
            CHECK (synths.getToggleState());
            CHECK (editor.model().dragForSynths);
            const auto tooltip = synths.getTooltip().toStdString();
            CHECK_THAT (tooltip, ContainsSubstring ("octave") && ContainsSubstring ("16th") && ContainsSubstring ("overlap")
                                     && ContainsSubstring ("velocity 72") && ContainsSubstring ("Kept for your user"));
            // What it leaves, that a chord sample's track comes as whole chords, the way back into Wubansia and where to
            // read more: no promise that every drag plays well, nor that the Accents lane stays whole (ADR 0027).
            CHECK_THAT (tooltip, ContainsSubstring ("one-key Accents and FX tracks stay as they are") && ContainsSubstring ("whole chords")
                                     && ContainsSubstring ("back into Wubansia") && ContainsSubstring ("INSTALL.md"));
            CHECK_THAT (tooltip, !ContainsSubstring ("root") && !ContainsSubstring ("Chord voicing") && !ContainsSubstring ("plays well")
                                     && !ContainsSubstring ("Drums, accents and FX"));
            CHECK (synths.getTooltip().length() <= 480);

            click (synths);
            CHECK (rig.controller.synthOptionRequests == std::vector<bool> { false });
            editor.tick();
            CHECK_FALSE (synths.getToggleState());
            CHECK_FALSE (editor.model().dragForSynths);

            // Another instance lit it again: lit at the next tick, with nothing asked.
            rig.controller.synthsOn = true;
            editor.tick();
            CHECK (synths.getToggleState());
            CHECK (editor.model().dragForSynths);
            CHECK (rig.controller.synthOptionRequests.size() == 1);

            // Every chip asks for the same file as ever: the option is the Controller's to apply.
            juce::Array<juce::File> dragged;
            auto& loop = get<editor::DragChip> (editor, "drag_loop");
            auto& bass = get<editor::DragChip> (editor, "drag_bass");
            auto& phrase = get<editor::DragChip> (editor, "drag_phrase_0");
            for (auto* chip : { &loop, &bass, &phrase })
                chip->startDrag = [&] (const juce::File& file, juce::Component&)
                {
                    dragged.add (file);
                    return true;
                };
            CHECK (loop.beginDrag());
            CHECK (bass.beginDrag());
            CHECK (phrase.beginDrag());
            REQUIRE (rig.controller.dragRequests.size() == 3);
            CHECK (rig.controller.dragRequests[0] == wubansia::DragRequest { wubansia::DragWhat::loop, engine::Lane::kick, {} });
            CHECK (rig.controller.dragRequests[1] == wubansia::DragRequest { wubansia::DragWhat::lane, engine::Lane::bass, {} });
            CHECK (rig.controller.dragRequests[2] == wubansia::DragRequest { wubansia::DragWhat::phrase, engine::Lane::kick, fut01PhraseFiles()[0] });
            for (const auto& file : dragged)
                file.deleteFile();
        });
}

TEST_CASE ("the drag strip's 1 drum track switch asks the Controller and follows it", "[editor]")
{
    Rig rig;
    rig.controller.phraseFileNames = fut01PhraseFiles();

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto* found = drumSwitch (editor);
            if (found == nullptr)
                return;
            auto& drums = *found;
            CHECK (drums.getButtonText() == "1 drum track");
            CHECK (drums.isEnabled());
            CHECK_FALSE (drums.getToggleState());
            CHECK_FALSE (editor.model().dragDrumsOnOneTrack);
            CHECK_THAT (drums.getTooltip().toStdString(), ContainsSubstring ("Drum Rack") && ContainsSubstring ("Drum Machine Designer"));

            click (drums);
            CHECK (rig.controller.drumOptionRequests == std::vector<bool> { true });
            editor.tick();
            CHECK (drums.getToggleState());
            CHECK (editor.model().dragDrumsOnOneTrack);

            // Another instance turned it off: dark at the next tick, with nothing asked.
            rig.controller.drumsOneTrack = false;
            editor.tick();
            CHECK_FALSE (drums.getToggleState());
            CHECK_FALSE (editor.model().dragDrumsOnOneTrack);
            CHECK (rig.controller.drumOptionRequests.size() == 1);

            // Lit, the Loop and Song chips ask for the same files as ever: the option is the Controller's to apply.
            rig.controller.drumsOneTrack = true;
            editor.tick();
            juce::Array<juce::File> dragged;
            auto& loop = get<editor::DragChip> (editor, "drag_loop");
            auto& song = get<editor::DragChip> (editor, "drag_song");
            for (auto* chip : { &loop, &song })
                chip->startDrag = [&] (const juce::File& file, juce::Component&)
                {
                    dragged.add (file);
                    return true;
                };
            CHECK (loop.beginDrag());
            CHECK (song.beginDrag());
            REQUIRE (rig.controller.dragRequests.size() == 2);
            CHECK (rig.controller.dragRequests[0] == wubansia::DragRequest { wubansia::DragWhat::loop, engine::Lane::kick, {} });
            CHECK (rig.controller.dragRequests[1] == wubansia::DragRequest { wubansia::DragWhat::song, engine::Lane::kick, {} });
            CHECK_THAT (loop.getTooltip().toStdString(), ContainsSubstring ("1 drum track puts its drums on one track"));
            CHECK_THAT (song.getTooltip().toStdString(), ContainsSubstring ("1 drum track puts its drums on one track"));
            for (const auto& file : dragged)
                file.deleteFile();
        });
}

TEST_CASE ("the drag strip fits both switches beside Loop and Song, and every label in full at 100 %", "[editor]")
{
    Rig rig;
    rig.controller.phraseFileNames = fut01PhraseFiles();

    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            editor.tick();
            auto& strip = get<editor::DragStrip> (editor, "drag");
            auto& loop = get<editor::DragChip> (editor, "drag_loop");
            auto& song = get<editor::DragChip> (editor, "drag_song");
            auto* foundDrums = drumSwitch (editor);
            auto* foundSynths = synthSwitch (editor);
            if (foundDrums == nullptr || foundSynths == nullptr)
                return;
            auto& drums = *foundDrums;
            auto& synths = *foundSynths;
            REQUIRE (strip.numPhraseChips() == 5);

            // For my synths, Loop, Song, 1 drum track, then the phrase chips, in that order: none overlapping, all inside
            // the strip.
            std::vector<juce::Component*> parts { &synths, &loop, &song, &drums };
            for (int i = 0; i < strip.numPhraseChips(); ++i)
                parts.push_back (strip.phraseChip (i));
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                INFO (parts[i]->getComponentID());
                CHECK (parts[i]->getParentComponent() == &strip);
                CHECK (strip.getLocalBounds().contains (parts[i]->getBounds()));
                if (i > 0)
                    CHECK (parts[i]->getX() >= parts[i - 1]->getRight());
                for (std::size_t j = i + 1; j < parts.size(); ++j)
                    CHECK_FALSE (parts[i]->getBounds().intersects (parts[j]->getBounds()));
            }

            // Every label reads in full, in the font and area its paint code uses.
            const auto width = [] (const juce::Font& font, const juce::String& text) { return juce::GlyphArrangement::getStringWidth (font, text); };
            const auto engraved = editor::uiFont (9.5f, true, 0.12f); // draw::engraved's font, at the strip's 9.5 px
            CHECK (width (engraved, "DRAG MIDI") <= strip.labelArea().getWidth());
            CHECK (strip.labelArea().getRight() <= (float) synths.getX());
            CHECK (width (drums.labelFont(), "1 DRUM TRACK") <= drums.labelArea().getWidth());
            CHECK (drums.labelFont().getHeight() >= 10.0f);
            CHECK (width (synths.labelFont(), "FOR MY SYNTHS") <= synths.labelArea().getWidth());
            CHECK (synths.labelFont().getHeight() >= 10.0f);
            for (auto* chip : { &loop, &song })
            {
                INFO (chip->label());
                CHECK (width (chip->labelFont(), chip->label()) <= chip->labelArea().getWidth());
            }

            // Every phrase chip shows a title it can hold, squeezed to 0.8 at most, in whatever font this machine gives
            // (Verdana on Windows and DejaVu Sans on some Linux machines are 8 and 12 % wider than Liberation Sans): the
            // whole title where it fits, else the title without its bar count, which the tooltip's file name still has.
            // "E Piano Stab Rhythm · 2 bars" fits in no default font. The two switches leave no room for five equal chips
            // that wide, so a chip whose title needs more than an equal share gets what it needs, and the others share
            // the rest.
            for (int i = 0; i < strip.numPhraseChips(); ++i)
            {
                auto* chip = strip.phraseChip (i);
                const auto title = chip->label();
                INFO (title);
                const auto fits = [&] (const juce::String& text) { return width (chip->labelFont(), text) * 0.8f < chip->labelArea().getWidth(); };
                const auto shown = chip->shownLabel();
                CHECK (shown == (fits (title) ? title : title.upToLastOccurrenceOf (editor::middleDot(), false, false)));
                CHECK (fits (shown));
                CHECK_THAT (chip->getTooltip().toStdString(), ContainsSubstring (fut01PhraseFiles()[i].toStdString()));
            }
            CHECK (strip.phraseChip (0)->shownLabel() == "E Piano Stab Rhythm");
        });
}

TEST_CASE ("in a host the transport offers Preview", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            CHECK (findAll (editor, "transport_play").empty());
            auto& preview = get<juce::Button> (editor, "preview");
            click (preview);
            CHECK (rig.controller.previewRequests == std::vector<bool> { true });
            editor.tick();
            CHECK (preview.getToggleState());
            CHECK (get<editor::Lcd> (editor, "lcd").lines().sync == "PREVIEW");
            click (preview);
            CHECK (rig.controller.previewRequests == std::vector<bool> { true, false });
        });
}

TEST_CASE ("in Standalone the transport plays, stops and sets the tempo", "[editor]")
{
    FakeController controller;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    TestProcessor processor ([&controller] (TestProcessor& p) { return new PluginEditor (p, controller); });
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    REQUIRE (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    withEditor (processor, [&] (PluginEditor& editor)
        {
            CHECK (findAll (editor, "preview").empty());
            click (get<juce::Button> (editor, "transport_play"));
            REQUIRE (controller.transportRequests.size() == 1);
            CHECK (controller.transportRequests[0] == std::pair { true, 0.0 });
            editor.tick();
            CHECK (get<juce::Button> (editor, "transport_play").getToggleState());
            CHECK (get<editor::Lcd> (editor, "lcd").lines().sync == "STANDALONE");

            click (get<juce::Button> (editor, "tempo_up"));
            CHECK (controller.transportRequests.back() == std::pair { true, 135.0 });
            editor.tick();
            click (get<juce::Button> (editor, "tempo_down"));
            CHECK (controller.transportRequests.back() == std::pair { true, 134.0 });
            editor.tick();
            click (get<juce::Button> (editor, "tempo_kit"));
            CHECK (controller.transportRequests.back() == std::pair { true, 0.0 });
            editor.tick();
            click (get<juce::Button> (editor, "transport_play"));
            CHECK (controller.transportRequests.back() == std::pair { false, 0.0 });
        });
}

TEST_CASE ("MIDI-in mode and the phrase switch go to the Controller", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& mode = get<editor::SegmentedChoice> (editor, "midi_in_mode");
            auto& laneBox = get<juce::ComboBox> (editor, "midi_in_lane");
            CHECK_FALSE (laneBox.isEnabled());

            click (mode, { (float) mode.getWidth() * 0.75f, (float) mode.getHeight() * 0.5f });
            CHECK (rig.controller.midiMode == engine::MidiInMode::lane);
            editor.tick();
            CHECK (laneBox.isEnabled());
            CHECK (laneBox.getItemText (4) == "Bass");

            laneBox.setSelectedItemIndex (4, juce::sendNotificationSync);
            CHECK (rig.controller.midiLane == engine::Lane::bass);
            editor.tick();
            CHECK (editor.model().midiInLane == engine::Lane::bass);

            auto& phrases = get<editor::SegmentedChoice> (editor, "phrase_mode");
            CHECK (phrases.getSelectedItemIndex() == 0);
            click (phrases, { (float) phrases.getWidth() * 0.75f, (float) phrases.getHeight() * 0.5f });
            CHECK (rig.controller.phrases == engine::PhraseMode::off);
            click (phrases, { (float) phrases.getWidth() * 0.25f, (float) phrases.getHeight() * 0.5f });
            CHECK (rig.controller.phrases == engine::PhraseMode::automatic);

            click (mode, { (float) mode.getWidth() * 0.25f, (float) mode.getHeight() * 0.5f });
            CHECK (rig.controller.midiMode == engine::MidiInMode::multi);
        });
}

TEST_CASE ("the display, lanes and meters follow the snapshot", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& lcd = get<editor::Lcd> (editor, "lcd");
            auto& strip = get<editor::SectionStrip> (editor, "sections");
            auto& fx = get<editor::FxPanel> (editor, "fx");

            CHECK (lcd.lines().transport == "STOPPED");
            CHECK (lcd.lines().position == "---.-");
            CHECK (lcd.lines().tempo == "134.0");
            CHECK (lcd.lines().kitTitle == "ALLOY SIGNAL");
            CHECK (lcd.lines().key == "F# MINOR" + editor::middleDot() + "11A");
            CHECK (strip.litBeat() == -1);

            auto& snapshot = rig.controller.nextSnapshot;
            snapshot.playing = true;
            snapshot.bpm = 134.0;
            snapshot.songTicks = 3840.0 * 2.0 + 960.0 + 10.0; // bar 3, beat 2
            snapshot.laneHits = engine::laneBit (engine::Lane::kick) | engine::laneBit (engine::Lane::bass);
            snapshot.lanePeak[0] = 0.5f;
            snapshot.gainReductionDb = 2.5f;
            snapshot.voices = 17;
            editor.tick();

            CHECK (lcd.lines().transport == "PLAYING");
            CHECK (lcd.lines().position == "  3.2");
            CHECK (lcd.lines().status == "VOICES 17");
            CHECK (strip.litBeat() == 9);
            CHECK (juce::exactlyEqual (lane (editor, engine::Lane::kick).ledLevel(), 1.0f));
            CHECK (juce::exactlyEqual (lane (editor, engine::Lane::bass).ledLevel(), 1.0f));
            CHECK (juce::exactlyEqual (lane (editor, engine::Lane::snare).ledLevel(), 0.0f));
            CHECK_THAT (lane (editor, engine::Lane::kick).meterDb(), WithinAbs (-6.02, 0.01));
            CHECK_THAT (fx.shownGainReductionDb(), WithinAbs (2.5, 1.0e-4));

            // The next snapshot has no hits: the LED fades and the meter falls.
            editor.tick();
            CHECK (lane (editor, engine::Lane::kick).ledLevel() < 1.0f);
            CHECK (lane (editor, engine::Lane::kick).ledLevel() > 0.0f);
            CHECK (lane (editor, engine::Lane::kick).meterDb() < -6.5f);

            // Song mode: the section strip lights the section playing.
            rig.processor.set (ids::playMode, 2.0f);
            snapshot.songTicks = 3840.0 * 70.0;
            snapshot.section = 2;
            editor.tick();
            CHECK (strip.litSection() == 2);
            CHECK (lcd.lines().position == " 71.1");
            CHECK_THAT (lcd.lines().mode.toStdString(), ContainsSubstring ("BREAK") && ContainsSubstring ("BAR 7/16"));

            rig.processor.set (ids::laneMute[0], 1.0f);
            editor.tick();
            CHECK (get<juce::Button> (editor, ids::laneMute[0]).getToggleState());
        });
}

TEST_CASE ("the zoom offers 100 to 200 % and scales the whole face", "[editor]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& zoom = get<juce::ComboBox> (editor, "zoom");
            juce::Array<int> offered;
            for (int i = 0; i < zoom.getNumItems(); ++i)
                offered.add (zoom.getItemId (i));
            for (const auto percent : { 100, 125, 150, 200 })
                CHECK (offered.contains (percent));

            zoom.setSelectedId (150, juce::sendNotificationSync);
            CHECK (editor.getWidth() == 1680);
            CHECK (editor.getHeight() == 1050);
            CHECK_THAT (editor.getZoom(), WithinAbs (1.5, 1.0e-6));

            juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
            {
                juce::Graphics g (image);
                editor.paintEntireComponent (g, false);
            }
            CHECK (image.getPixelAt (editor.getWidth() - 3, editor.getHeight() - 3).getAlpha() == 255);
        });

    // The next editor opens at the zoom the last one was left at.
    withEditor (rig, [] (PluginEditor& editor)
        {
            CHECK_THAT (editor.getZoom(), WithinAbs (1.5, 1.0e-6));
            editor.setZoom (1.0f);
            CHECK (editor.getWidth() == 1120);
        });
}

TEST_CASE ("phrase file names read as titles", "[editor]")
{
    const auto dot = editor::middleDot();
    CHECK (editor::phraseTitle ("FUT01_Phrase_ChromePluck_4bars.mid", "FUT01") == "Chrome Pluck" + dot + "4 bars");
    CHECK (editor::phraseTitle ("FUT03_Phrase_DubStabGm9_4bars.mid", "FUT03") == "Dub Stab Gm9" + dot + "4 bars");
    CHECK (editor::phraseTitle ("FUT04_Phrase_AccentAcidStabE4_2bars.mid", "FUT04") == "Accent Acid Stab E4" + dot + "2 bars");
    CHECK (editor::phraseTitle ("odd.mid", "FUT01") == "odd");
    CHECK (editor::shortKey ("F# minor") == "F#m");
    CHECK (editor::shortKey ("Bb major") == "Bb");
}

// The Samples drawer (ADR 0024), against the FakeController's slice of FUT01's and FUT02's catalogs:
// FUT01 Accents are Reverse Stab (files 158-159, END), Rave Stab (165-166) and Alloy Bell (171-178, four
// layers of two round robins); one sound in each other category. FUT02's samples are missing.

TEST_CASE ("the Samples button opens the drawer over the lanes, A/B and FX; the editor stays 1120 x 700", "[editor][sounds]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            auto& button = get<juce::Button> (editor, "samples");
            CHECK (button.getTooltip() == "Browse every kit's sounds, hear them, and drag them into your DAW as WAV files");
            auto& drawer = get<editor::SamplesPanel> (editor, "samples_drawer");
            CHECK_FALSE (editor.isSamplesPanelShown());
            CHECK_FALSE (drawer.isVisible());

            click (button);
            editor.tick();
            CHECK (editor.isSamplesPanelShown());
            CHECK (drawer.isVisible());
            CHECK (button.getToggleState()); // its LED is lit while the drawer is open
            CHECK (drawer.getBounds() == juce::Rectangle<int> (20, 246, 1080, 396));
            CHECK (editor.getWidth() == 1120);
            CHECK (editor.getHeight() == 700);

            // It covers the lanes, A/B and FX; the header, display, section strip and MIDI drag strip stay in reach.
            for (const auto l : { engine::Lane::kick, engine::Lane::bass, engine::Lane::fx })
                CHECK (coveredByDrawer (editor, lane (editor, l)));
            CHECK (coveredByDrawer (editor, get<juce::Component> (editor, "ab")));
            CHECK (coveredByDrawer (editor, get<juce::Component> (editor, "fx")));
            for (const auto* id : { "drag", "drag_loop", "sections", "lcd", "kit_FUT02", "samples", "zoom" })
            {
                INFO (id);
                CHECK_FALSE (coveredByDrawer (editor, get<juce::Component> (editor, id)));
                CHECK (get<juce::Component> (editor, id).isVisible());
            }

            // At 200 % it covers the same, scaled with the whole face.
            editor.setZoom (2.0f);
            CHECK (editor.getWidth() == 2240);
            CHECK (drawer.getBounds() == juce::Rectangle<int> (20, 246, 1080, 396));
            CHECK (editor.getLocalPoint (&drawer, juce::Point<int> (0, 0)) == juce::Point<int> (40, 492));
            CHECK (editor.getLocalPoint (&drawer, juce::Point<int> (1080, 396)) == juce::Point<int> (2200, 1284));
            CHECK (coveredByDrawer (editor, lane (editor, engine::Lane::snare)));
            CHECK_FALSE (coveredByDrawer (editor, get<juce::Component> (editor, "drag")));
            editor.setZoom (1.0f);

            // A second click closes it, and its LED goes out.
            click (button);
            editor.tick();
            CHECK_FALSE (editor.isSamplesPanelShown());
            CHECK_FALSE (button.getToggleState());
            CHECK_FALSE (coveredByDrawer (editor, lane (editor, engine::Lane::kick)));
        });
}

TEST_CASE ("while the drawer is open, Tab and screen readers reach nothing it covers", "[editor][sounds]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            // What the drawer hides from sight: a component wholly inside its bounds, and not its own.
            auto& drawer = openDrawer (editor);
            auto* face = drawer.getParentComponent();
            REQUIRE (face != nullptr);
            const auto under = [&] (juce::Component* c) {
                return c != &drawer && ! drawer.isParentOf (c) && drawer.getBounds().contains (face->getLocalArea (c, c->getLocalBounds()));
            };
            const auto name = [] (juce::Component* c) { return c->getComponentID().isNotEmpty() ? c->getComponentID() : "'" + c->getTitle() + "'"; };

            juce::StringArray tabStops, covered;
            for (auto* c : juce::KeyboardFocusTraverser().getAllComponents (&editor))
            {
                tabStops.add (name (c));
                if (under (c))
                    covered.add (name (c));
            }
            for (auto* c : juce::FocusTraverser().getAllComponents (&editor)) // what a screen reader walks
                if (under (c))
                    covered.addIfNotAlreadyThere (name (c));
            INFO ("under the drawer: " << covered.joinIntoString (" "));
            CHECK (covered.isEmpty());

            // The drawer's controls and those around it stay in reach.
            INFO ("tab stops: " << tabStops.joinIntoString (" "));
            for (const auto* id : { "samples_kit_FUT01", "sound_list", "sound_drag", "samples_content", "samples", "zoom", "play_mode" })
            {
                INFO (id);
                CHECK (tabStops.contains (id));
            }

            // Closed, the lanes, A/B and FX are back, and so are their keys.
            click (get<juce::Button> (editor, "samples_close"));
            editor.tick();
            for (const auto l : { engine::Lane::kick, engine::Lane::fx })
                CHECK (lane (editor, l).isVisible());
            CHECK (get<juce::Component> (editor, "ab").isVisible());
            CHECK (get<juce::Component> (editor, "fx").isVisible());
            juce::StringArray after;
            for (auto* c : juce::KeyboardFocusTraverser().getAllComponents (&editor))
                after.add (name (c));
            CHECK (after.contains (ids::laneMute[0]));
            CHECK (after.contains (ids::clubMaster));
        });
}

TEST_CASE ("the drawer and the content panel never show together; Content... opens the install panel", "[editor][sounds]")
{
    Rig rig;
    rig.controller.contentStatus.state = wubansia::ContentState::missing;
    rig.controller.contentStatus.problem = "no folder holds FUT01";
    rig.controller.kitCards[0].content = rig.controller.contentStatus;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.tick();
            CHECK (editor.isContentPanelShown()); // FUT01's samples are missing: the install panel opens by itself

            auto& drawer = openDrawer (editor);
            CHECK_FALSE (editor.isContentPanelShown());
            for (int i = 0; i < 5; ++i)
                editor.tick();
            CHECK_FALSE (editor.isContentPanelShown()); // never by itself while the drawer is open...
            CHECK_THAT (drawer.bannerText().toStdString(), ContainsSubstring ("FUT01's samples aren't installed")); // ...whose banner says so

            // Content... closes the drawer and opens the install panel.
            auto& content = get<juce::Button> (editor, "samples_content");
            CHECK (content.getTooltip().isNotEmpty());
            click (content);
            editor.tick();
            CHECK_FALSE (editor.isSamplesPanelShown());
            CHECK (editor.isContentPanelShown());
            CHECK_FALSE (get<juce::Button> (editor, "samples").getToggleState());

            // The Samples button brings the drawer back, and the install panel goes.
            click (get<juce::Button> (editor, "samples"));
            editor.tick();
            CHECK (editor.isSamplesPanelShown());
            CHECK_FALSE (editor.isContentPanelShown());

            // With the drawer closed, the install panel opens by itself again: the kit still can't play...
            click (get<juce::Button> (editor, "samples_close"));
            editor.tick();
            CHECK_FALSE (editor.isSamplesPanelShown());
            CHECK (editor.isContentPanelShown());

            // ...until the person closes it, and the drawer leaves it closed.
            click (get<juce::Button> (editor, "content_close"));
            editor.tick();
            CHECK_FALSE (editor.isContentPanelShown());
            openDrawer (editor);
            click (get<juce::Button> (editor, "samples_close"));
            editor.tick();
            CHECK_FALSE (editor.isContentPanelShown());

            // Whatever the clicks, never both.
            for (const auto* id : { "samples", "samples_content", "samples", "samples", "samples_content", "content_close", "samples" })
            {
                INFO (id);
                click (get<juce::Button> (editor, id));
                editor.tick();
                CHECK_FALSE ((editor.isSamplesPanelShown() && editor.isContentPanelShown()));
            }
        });
}

TEST_CASE ("the drawer lists the kit's sounds by category, Accents first, with counts", "[editor][sounds]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            CHECK (drawer.browsedKit() == "FUT01");
            CHECK (drawer.category() == wubansia::SoundCategory::accents);
            REQUIRE (drawer.numRows() == 3);
            CHECK (drawer.selectedRow() == 0);
            CHECK (rig.controller.auditions.empty()); // opening plays nothing

            // Each row: the name, the length, END for an end-aligned sound, and how many files.
            const auto& reverse = soundRow (editor, 0).look();
            CHECK (reverse.name == "Reverse Stab");
            CHECK (reverse.length == "1.30 s");
            CHECK (reverse.endAligned);
            CHECK (reverse.files == 2);
            CHECK (reverse.selected);
            const auto& rave = soundRow (editor, 1).look();
            CHECK (rave.name == "Rave Stab");
            CHECK (rave.length == "0.71 s");
            CHECK_FALSE (rave.endAligned);
            CHECK_FALSE (rave.selected);
            CHECK (rave.spoken == "Rave Stab, 0.71 seconds");
            CHECK (soundRow (editor, 1).getTitle() == "Rave Stab, 0.71 seconds");
            CHECK (soundRow (editor, 2).look().name == "Alloy Bell");
            CHECK (soundRow (editor, 2).look().files == 8);
            CHECK (soundRow (editor, 0).getHeight() == editor::SoundRow::height);
            CHECK (soundRow (editor, 1).getY() == soundRow (editor, 0).getBottom());

            // The rail: the seven categories top to bottom, Accents first, each with its count and words.
            const std::array<std::pair<const char*, int>, 7> rail { { { "accents", 3 }, { "fx", 1 }, { "vox", 1 }, { "synths", 1 },
                { "bass", 1 }, { "drums", 1 }, { "loops", 1 } } };
            auto previousY = -1;
            for (const auto& [id, count] : rail)
            {
                INFO (id);
                auto& category = get<editor::CategoryButton> (editor, juce::String ("samples_category_") + id);
                CHECK (category.count() == count);
                CHECK (category.isEnabled());
                CHECK (category.getY() > previousY);
                CHECK (category.getTooltip().isNotEmpty());
                previousY = category.getY();
            }
            CHECK (get<editor::CategoryButton> (editor, "samples_category_accents").isSelected());
            CHECK (get<juce::Button> (editor, "samples_category_accents").getTooltip() == "The kit's accent synths and hits: stabs, chords, sweeps, bells");
            CHECK (get<juce::Button> (editor, "samples_category_fx").getTooltip() == "Risers, fills and drops");
            CHECK (get<juce::Button> (editor, "samples_category_loops").getTooltip() == "The kit's recorded phrases, whole bars at its tempo");

            // A category shows its sounds and selects the first one, without playing it.
            click (get<juce::Button> (editor, "samples_category_fx"));
            CHECK (drawer.category() == wubansia::SoundCategory::fx);
            CHECK (get<editor::CategoryButton> (editor, "samples_category_fx").isSelected());
            CHECK_FALSE (get<editor::CategoryButton> (editor, "samples_category_accents").isSelected());
            REQUIRE (drawer.numRows() == 1);
            CHECK (soundRow (editor, 0).look().name == "Shift Riser");
            CHECK (soundRow (editor, 0).look().length == "8 bars");
            CHECK (soundRow (editor, 0).look().spoken == "Shift Riser, 8 bars, ends on the beat");
            CHECK (drawer.selectedRow() == 0);
            CHECK (findAll (editor, "sound_row_1").empty());
            click (get<juce::Button> (editor, "samples_category_loops"));
            REQUIRE (drawer.numRows() == 1);
            CHECK (soundRow (editor, 0).look().name == "Chrome Pluck Phrase");
            CHECK (soundRow (editor, 0).look().length == "2 bars");
            CHECK (rig.controller.auditions.empty());

            // The sounds are asked for once per kit, not on every tick.
            CHECK (rig.controller.soundsCalls == 1);
            for (int i = 0; i < 10; ++i)
                editor.tick();
            CHECK (rig.controller.soundsCalls == 1);
        });
}

TEST_CASE ("the detail pane shows the selected sound: its file, length and format, and one hint", "[editor][sounds]")
{
    Rig rig;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            const auto detail = [&] { return drawer.detailText(); };

            // Reverse Stab ends on the beat.
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("Reverse Stab") && ContainsSubstring ("Accents" + editor::middleDot().toStdString() + "FUT01"));
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("F#m" + editor::middleDot().toStdString() + "11A"));
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("1.30 s" + editor::middleDot().toStdString() + "stereo" + editor::middleDot().toStdString() + "48 kHz 24-bit"));
            CHECK (drawer.hintText() == "Ends on the beat: put its end on the downbeat");

            // Alloy Bell: eight files of one note, so no hint.
            press (soundRow (editor, 2));
            CHECK (drawer.hintText().isEmpty());

            // A riser that ends on the beat, whole bars: the bars too.
            click (get<juce::Button> (editor, "samples_category_fx"));
            CHECK (drawer.hintText() == "Ends on the beat: put its end on the downbeat (8 bars)");

            // The vox synth starts before its beat, and has notes to pick from (the lead-in comes first).
            click (get<juce::Button> (editor, "samples_category_vox"));
            CHECK (drawer.hintText() == "Starts 6 ms before its beat: slide it 6 ms earlier");

            // Nine recorded notes.
            click (get<juce::Button> (editor, "samples_category_synths"));
            CHECK (drawer.hintText() == "9 recorded notes: pick one in File");

            // A loop: what it plays, and its bars at the kit's tempo; with an echo tail beyond its cycle, both.
            click (get<juce::Button> (editor, "samples_category_loops"));
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("Synth loop" + editor::middleDot().toStdString() + "FUT01"));
            CHECK (soundRow (editor, 0).getTitle() == "Chrome Pluck Phrase, synth loop, 2 bars");
            CHECK (drawer.hintText() == "2 bars at 134 BPM");
            auto& loops = rig.controller.soundLists["FUT02"];
            loops[2].bars = 6;
            loops[2].loopBars = 4;
            click (get<editor::KitChip> (editor, "samples_kit_FUT02"));
            click (get<juce::Button> (editor, "samples_category_loops"));
            CHECK (drawer.hintText() == "6 bars at 130 BPM: a 4-bar loop, then a 2-bar echo tail");
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("mono"));
            CHECK_THAT (detail().toStdString(), ContainsSubstring ("Bass loop" + editor::middleDot().toStdString() + "FUT02" + editor::middleDot().toStdString() + "Dm"));
        });
}

TEST_CASE ("a click auditions the row's chosen file; Stop, Esc and closing the drawer stop it", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            press (soundRow (editor, 1));
            REQUIRE (c.auditions.size() == 1);
            CHECK (c.auditions[0] == wubansia::SoundRef { "FUT01", 166 }); // Rave Stab's default: its loud layer
            CHECK (drawer.selectedRow() == 1);
            editor.tick();
            CHECK (soundRow (editor, 1).look().playing);

            // The header's Stop.
            auto& stop = get<juce::Button> (editor, "sound_stop");
            click (stop);
            CHECK (c.stops == 1);
            editor.tick();
            CHECK_FALSE (soundRow (editor, 1).look().playing);

            // Esc.
            press (soundRow (editor, 2));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 177 }); // Alloy Bell v4 rr1
            CHECK (drawer.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
            CHECK (c.stops == 2);

            // On the row that plays, its stop mark stops it; a press anywhere else plays it again from the top.
            press (soundRow (editor, 0));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 159 });
            editor.tick();
            press (soundRow (editor, 0));
            CHECK (c.auditions.size() == 4);
            CHECK (c.stops == 2);
            pressMark (soundRow (editor, 0));
            CHECK (c.stops == 3);
            CHECK (c.auditions.size() == 4);
            pressMark (soundRow (editor, 0)); // on a row that doesn't play, the mark plays it
            CHECK (c.auditions.size() == 5);

            // Closing the drawer, with its x or with the Samples button.
            click (get<juce::Button> (editor, "samples_close"));
            CHECK (c.stops == 4);
            CHECK_FALSE (editor.isSamplesPanelShown());
            openDrawer (editor);
            press (soundRow (editor, 1));
            click (get<juce::Button> (editor, "samples"));
            CHECK (c.stops == 5);
            CHECK_FALSE (editor.isSamplesPanelShown());

            // Esc with nothing to stop is left to the host.
            auto& again = openDrawer (editor);
            CHECK_FALSE (again.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
            CHECK (c.stops == 5);

            // The editor closing with the drawer open stops what it plays.
            press (soundRow (editor, 1));
        });
    CHECK (rig.controller.stops == 6);

    // With the drawer closed, closing the editor has nothing to stop.
    withEditor (rig, [] (PluginEditor& editor) { editor.tick(); });
    CHECK (rig.controller.stops == 6);
}

TEST_CASE ("Up and Down move through the rows and audition each; Space is left to the host", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            auto& list = get<juce::Component> (editor, "sound_list");
            CHECK (list.getWantsKeyboardFocus());
            const auto key = [&list] (int code, juce::ModifierKeys mods = {}) { return list.keyPressed (juce::KeyPress (code, mods, 0)); };

            CHECK (key (juce::KeyPress::downKey));
            CHECK (drawer.selectedRow() == 1);
            REQUIRE (c.auditions.size() == 1);
            CHECK (c.auditions[0] == wubansia::SoundRef { "FUT01", 166 });
            CHECK (key (juce::KeyPress::downKey));
            CHECK (drawer.selectedRow() == 2);
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 177 });
            CHECK (key (juce::KeyPress::downKey)); // the last row stays, and plays nothing new
            CHECK (drawer.selectedRow() == 2);
            CHECK (c.auditions.size() == 2);
            CHECK (key (juce::KeyPress::upKey));
            CHECK (drawer.selectedRow() == 1);
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 166 });
            CHECK (key (juce::KeyPress::returnKey)); // plays it again
            CHECK (c.auditions.size() == 4);
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 166 });

            // Space, shortcuts and every other key go on to the host.
            CHECK_FALSE (key (juce::KeyPress::spaceKey));
            CHECK_FALSE (key ('A', juce::ModifierKeys::commandModifier));
            CHECK_FALSE (key ('S'));
            CHECK_FALSE (key (juce::KeyPress::downKey, juce::ModifierKeys::commandModifier));
            CHECK_FALSE (key (juce::KeyPress::escapeKey)); // the drawer's, not the list's
            CHECK_FALSE (drawer.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
            CHECK_FALSE (drawer.keyPressed (juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0)));
            CHECK (c.auditions.size() == 4);
            CHECK (c.stops == 0);
        });
}

TEST_CASE ("opening the drawer gives its list the keyboard: Down plays the next sound, Return plays it again", "[editor][sounds]")
{
    // Keyboard focus needs the editor in a window the OS lets this process focus: an X display here (xvfb-run does).
#if ! (JUCE_LINUX || JUCE_BSD)
    SKIP ("needs a window this test can focus: run it on Linux with a display");
#else
    if (juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isEmpty())
        SKIP ("no display: run it under xvfb-run");

    // JUCE leaves Xlib's errors to Xlib's default handler, which ends the process, unless it runs as a standalone app;
    // a window on a display with no window manager (Xvfb) draws a few harmless ones. They're ignored from here on.
    using XHandler = int (*) (void*, void*);
    if (auto* x11 = dlopen ("libX11.so.6", RTLD_NOW | RTLD_GLOBAL))
        if (auto setHandler = reinterpret_cast<XHandler (*) (XHandler)> (dlsym (x11, "XSetErrorHandler")))
            setHandler ([] (void*, void*) -> int { return 0; });

    Rig rig;
    auto& c = rig.controller;
    withEditor (rig, [&] (PluginEditor& editor)
        {
            juce::DocumentWindow window ("Wubansia", juce::Colours::black, 0, true);
            window.setUsingNativeTitleBar (false);
            window.setContentNonOwned (&editor, true);
            window.setVisible (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            auto* peer = editor.getPeer();
            REQUIRE (peer != nullptr);

            // A click on Samples leaves the keyboard on it, as a real one does; then the drawer opens.
            auto& samples = get<juce::Button> (editor, "samples");
            samples.grabKeyboardFocus();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            if (! samples.hasKeyboardFocus (false))
                SKIP ("the window didn't get the keyboard focus");
            click (samples);
            editor.tick();
            REQUIRE (editor.isSamplesPanelShown());
            auto& drawer = get<editor::SamplesPanel> (editor, "samples_drawer");
            auto& list = get<juce::Component> (editor, "sound_list");
            CHECK (list.hasKeyboardFocus (false));
            CHECK (c.auditions.empty()); // opening plays nothing

            // The keys reach the list, not the host or the Samples button.
            CHECK (peer->handleKeyPress (juce::KeyPress (juce::KeyPress::downKey)));
            CHECK (drawer.selectedRow() == 1);
            REQUIRE (c.auditions.size() == 1);
            CHECK (peer->handleKeyPress (juce::KeyPress (juce::KeyPress::returnKey)));
            CHECK (c.auditions.size() == 2);
            CHECK (editor.isSamplesPanelShown()); // Return played; it didn't close the drawer

            window.clearContentComponent();
        });
#endif
}

TEST_CASE ("the File box changes what plays, drags and reveals", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            Handoffs os;
            os.hook (drawer);
            auto& files = get<juce::ComboBox> (editor, "sound_file");
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");
            CHECK (files.getTooltip() == "v1 is the softest layer, rr a round robin. Notes count middle C as C4; Live and Logic call it C3.");

            press (soundRow (editor, 2)); // Alloy Bell: four layers of two round robins
            REQUIRE (files.getNumItems() == 8);
            CHECK (files.getItemText (0) == "Alloy Bell v1 rr1");
            CHECK (files.getText() == "Alloy Bell v4 rr1"); // the default: the top layer's first round robin
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 177 });
            CHECK (tile.fileName() == "Alloy Bell v4 rr1 - FUT01 134 F#m 11A.wav");

            // Picking a file plays it...
            files.setSelectedItemIndex (2, juce::sendNotificationSync);
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 173 }); // Alloy Bell v2 rr1
            CHECK (tile.fileName() == "Alloy Bell v2 rr1 - FUT01 134 F#m 11A.wav");

            // ...and from then on the row, Play, the drag and Show in Finder use it.
            press (soundRow (editor, 2));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 173 });
            click (get<juce::Button> (editor, "sound_play"));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 173 });
            drag (tile, 8.0f);
            REQUIRE_FALSE (c.exportRequests.empty());
            CHECK (c.exportRequests.back() == wubansia::SoundRef { "FUT01", 173 });
            REQUIRE (os.dragged.size() == 1);
            CHECK (os.dragged[0].getFileName() == "Alloy Bell v2 rr1 - FUT01.wav"); // the fake's name for the copy
            click (get<juce::Button> (editor, "sound_reveal"));
            CHECK (c.exportRequests.back() == wubansia::SoundRef { "FUT01", 173 });
            REQUIRE (os.revealed.size() == 1);
            CHECK (os.revealed[0] == os.dragged[0]);

            // Other sounds keep their own; the choice stays while the editor is open.
            press (soundRow (editor, 1));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 166 });
            CHECK (files.getNumItems() == 2);
            CHECK (files.getText() == "Rave Stab v2");
            click (get<juce::Button> (editor, "samples_close"));
            openDrawer (editor);
            press (soundRow (editor, 2));
            CHECK (files.getText() == "Alloy Bell v2 rr1");
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 173 });
        });
}

TEST_CASE ("a 4-pixel drag exports once and hands that one WAV to the OS; a click on the tile reveals it", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            Handoffs os;
            os.hook (drawer);
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");
            CHECK (tile.getTooltip() == "Drag this WAV into Live, Logic or Finder. Click to show it.");

            // The tile names the file a drag gives: the installed name, then the kit, its tempo, key and Camelot code.
            press (soundRow (editor, 1));
            CHECK (tile.fileName() == "Rave Stab v2 - FUT01 134 F#m 11A.wav");

            // A row: the press plays it; 3 pixels is still a press; at 4 its file is copied and handed over, once.
            auto& rave = soundRow (editor, 1);
            const auto start = rave.getLocalBounds().toFloat().getCentre();
            rave.mouseDown (mouseAt (rave, start, start));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 166 });
            rave.mouseDrag (mouseAt (rave, start.translated (3.0f, 0.0f), start, true));
            CHECK (c.exportRequests.empty());
            rave.mouseDrag (mouseAt (rave, start.translated (4.0f, 0.0f), start, true));
            REQUIRE (c.exportRequests.size() == 1);
            CHECK (c.exportRequests[0] == wubansia::SoundRef { "FUT01", 166 });
            REQUIRE (os.dragged.size() == 1);
            CHECK (os.dragged[0].existsAsFile());
            CHECK (os.dragged[0].getFileName() == "Rave Stab v2 - FUT01.wav");
            rave.mouseDrag (mouseAt (rave, start.translated (40.0f, 10.0f), start, true));
            rave.mouseUp (mouseAt (rave, start.translated (40.0f, 10.0f), start, true));
            CHECK (c.exportRequests.size() == 1);
            CHECK (os.dragged.size() == 1);
            CHECK (os.revealed.isEmpty()); // a drag is never a click
            CHECK (drawer.statusText() == "Saved Rave Stab v2 - FUT01.wav");
            CHECK (tile.fileName() == "Rave Stab v2 - FUT01.wav"); // the copy as written

            // The tile: a click copies the file (here, finds it) and shows it...
            click (tile);
            CHECK (c.exportRequests.size() == 2);
            REQUIRE (os.revealed.size() == 1);
            CHECK (os.revealed[0] == os.dragged[0]);

            // ...and a drag of 4 pixels hands it over.
            drag (tile, 4.0f);
            CHECK (c.exportRequests.size() == 3);
            CHECK (os.dragged.size() == 2);
            CHECK (os.revealed.size() == 1);

            // A tick while startDrag hands the drag over doesn't follow another kit, so the row the drag starts from
            // stays; the next tick follows it. (JUCE 9.0.4 ticks nothing inside the call: its OS drags return as soon
            // as they begin.)
            auto followedDuringDrag = true;
            drawer.startDrag = [&] (const juce::File& file, juce::Component&)
            {
                os.dragged.add (file);
                c.selectKit ("FUT02");
                editor.tick();
                followedDuringDrag = drawer.browsedKit() != "FUT01";
                return true;
            };
            drag (soundRow (editor, 1), 6.0f);
            CHECK (os.dragged.size() == 3);
            CHECK_FALSE (followedDuringDrag);
            editor.tick();
            CHECK (drawer.browsedKit() == "FUT02");
        });
}

TEST_CASE ("an export that fails starts no drag and says why", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    rig.controller.exportFails = true;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            Handoffs os;
            os.hook (drawer);
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");

            drag (tile, 6.0f);
            REQUIRE (c.exportRequests.size() == 1);
            CHECK (c.exportRequests[0] == wubansia::SoundRef { "FUT01", 159 }); // Reverse Stab v2, the first row's
            CHECK (os.dragged.isEmpty());
            CHECK (tile.hasFailed());
            CHECK (drawer.statusText() == editor::utf8 ("Couldn't save Reverse Stab v2: the fake's disk is full. Choose another folder with Change\xe2\x80\xa6"));

            // A row's drag, a click on the tile and Show in Finder fail the same way.
            drag (soundRow (editor, 1), 6.0f);
            CHECK (c.exportRequests.size() == 2);
            CHECK (tile.hasFailed());
            CHECK_THAT (drawer.statusText().toStdString(), ContainsSubstring ("Couldn't save Rave Stab v2: the fake's disk is full."));
            click (tile);
            click (get<juce::Button> (editor, "sound_reveal"));
            CHECK (c.exportRequests.size() == 4);
            CHECK (os.dragged.isEmpty());
            CHECK (os.revealed.isEmpty());

            // Once the disk takes it, the drag goes ahead and the tile is well again.
            c.exportFails = false;
            drag (tile, 6.0f);
            CHECK (os.dragged.size() == 1);
            CHECK_FALSE (tile.hasFailed());
            CHECK (drawer.statusText() == "Saved Rave Stab v2 - FUT01.wav");
        });
}

TEST_CASE ("an export the kit's own file fails says what fixes the kit, not the folder", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;
    const auto refusal = [] (const char* problem, const char* detail, bool reinstall)
    {
        wubansia::SoundExport copy;
        copy.problem = problem;
        copy.detail = detail;
        copy.sourceProblem = true;
        copy.reinstall = reinstall;
        return copy;
    };

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            juce::StringArray said;
            drawer.announce = [&said] (const juce::String& text) { said.add (text); };
            Handoffs os;
            os.hook (drawer);
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");

            // The installed file is gone (an unplugged drive, say): another samples folder wouldn't help. The line names
            // the file in the kit and what helps, and nothing else; where it was looked for is in the tooltip, and said.
            const auto gone = refusal ("FUT01's Music/Reverse Stab v2.wav is missing",
                "missing: Music/Reverse Stab v2.wav: no such file in /Users/someone/Library/Application Support/Wubansia Audio/Wubansia/Content/fut01/v1",
                false);
            c.exportOutcome = [&gone] (const wubansia::SoundRef&) { return gone; };
            drag (tile, 6.0f);
            CHECK (os.dragged.isEmpty());
            CHECK (tile.hasFailed());
            const juce::String line ("Couldn't save Reverse Stab v2: FUT01's Music/Reverse Stab v2.wav is missing. Check that FUT01's samples are still installed.");
            CHECK (drawer.statusText() == line);
            const juce::String where ("Music/Reverse Stab v2.wav: no such file in /Users/someone/Library/Application Support/Wubansia Audio/Wubansia/Content/fut01/v1.");
            CHECK (drawer.statusTooltip() == line + "\n" + where);
            REQUIRE_FALSE (said.isEmpty());
            CHECK (said[said.size() - 1] == line + " " + where);

            // A content folder whose name holds "install" changes nothing: the advice comes from the result, not its words.
            c.exportOutcome = [&refusal] (const wubansia::SoundRef&)
            {
                return refusal ("FUT01's Music/Reverse Stab v2.wav is missing",
                    "missing: Music/Reverse Stab v2.wav: no such file in /Users/someone/Music/Installed Kits/fut01/v1", false);
            };
            click (tile);
            CHECK (drawer.statusText() == line);
            CHECK (drawer.statusTooltip().endsWith ("/Users/someone/Music/Installed Kits/fut01/v1."));

            // Bytes that aren't kit.json's: the problem already says what to do, and there's no more to it.
            c.exportOutcome = [&refusal] (const wubansia::SoundRef&)
            { return refusal ("Reverse Stab v2.wav isn't the file FUT01's content v1 lists: reinstall FUT01", "", true); };
            click (get<juce::Button> (editor, "sound_reveal"));
            CHECK (os.revealed.isEmpty());
            CHECK (drawer.statusText() == "Couldn't save Reverse Stab v2: Reverse Stab v2.wav isn't the file FUT01's content v1 lists: reinstall FUT01.");
            CHECK (drawer.statusTooltip() == drawer.statusText());
            CHECK (said[said.size() - 1] == drawer.statusText());

            // A check's code never shows ("lfs_pointer: ..." from the kit's content status, say).
            c.exportOutcome = [&refusal] (const wubansia::SoundRef&)
            { return refusal ("lfs_pointer: Music/Reverse Stab v2.wav: a Git LFS pointer file", "", false); };
            click (tile);
            CHECK (drawer.statusText() == "Couldn't save Reverse Stab v2: Music/Reverse Stab v2.wav: a Git LFS pointer file. Check that FUT01's samples are still installed.");

            // A failure at the samples folder still points to Change...
            c.exportOutcome = nullptr;
            c.exportFails = true;
            drag (tile, 6.0f);
            CHECK_THAT (drawer.statusText().toStdString(), EndsWith (editor::utf8 ("Choose another folder with Change\xe2\x80\xa6").toStdString()));
            CHECK (drawer.statusTooltip() == drawer.statusText());
        });
}

TEST_CASE ("an export the samples folder fails names no path: the line keeps Change..., the path is in the tooltip", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;

    // exportCopy's refusals for a samples folder deep in the home folder, as the Controller hands them on.
    const auto kitFolder = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                               .getChildFile ("Music/Sample Libraries/Wubansia drags for the next album/FUT01 Alloy Signal");
    const auto target = kitFolder.getChildFile ("Reverse Stab v2 - FUT01 134 F#m 11A.wav");
    const auto refusal = [] (wubansia::sounds::FolderProblem problem, const juce::File& at)
    { return wubansia::sounds::soundExportOf (wubansia::sounds::folderRefusal (problem, at)); };

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            juce::StringArray said;
            drawer.announce = [&said] (const juce::String& text) { said.add (text); };
            Handoffs os;
            os.hook (drawer);
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");

            // The copy can't be written: the line says so in a few words and keeps Change...; the path it couldn't write
            // is in the tooltip, and said.
            const auto unwritten = refusal (wubansia::sounds::FolderProblem::write, target);
            c.exportOutcome = [&unwritten] (const wubansia::SoundRef&) { return unwritten; };
            drag (tile, 6.0f);
            CHECK (os.dragged.isEmpty());
            CHECK (tile.hasFailed());
            const auto line = editor::utf8 ("Couldn't save Reverse Stab v2: the samples folder can't be written to. Choose another folder with Change\xe2\x80\xa6");
            CHECK (drawer.statusText() == line);
            const auto where = "Can't write " + target.getFullPathName() + ".";
            CHECK (drawer.statusTooltip() == line + "\n" + where);
            REQUIRE_FALSE (said.isEmpty());
            CHECK (said[said.size() - 1] == line + " " + where);

            // The kit's folder in it can't be made, or every name is taken: the same, in their own words.
            const auto unmade = refusal (wubansia::sounds::FolderProblem::makeFolder, kitFolder);
            c.exportOutcome = [&unmade] (const wubansia::SoundRef&) { return unmade; };
            click (tile);
            CHECK (drawer.statusText() == line);
            CHECK (drawer.statusTooltip() == line + "\nCan't make the folder " + kitFolder.getFullPathName() + ".");

            const auto taken = refusal (wubansia::sounds::FolderProblem::namesTaken, target);
            c.exportOutcome = [&taken] (const wubansia::SoundRef&) { return taken; };
            click (get<juce::Button> (editor, "sound_reveal"));
            CHECK (os.revealed.isEmpty());
            const auto takenLine = editor::utf8 ("Couldn't save Reverse Stab v2: every name for it is taken in the samples folder. Choose another folder with Change\xe2\x80\xa6");
            CHECK (drawer.statusText() == takenLine);
            CHECK (drawer.statusTooltip()
                   == takenLine + "\nEvery name from Reverse Stab v2 - FUT01 134 F#m 11A.wav to its (999) is taken in " + kitFolder.getFullPathName() + ".");
            CHECK (said[said.size() - 1] == drawer.statusTooltip().replace ("\n", " "));
        });
}

TEST_CASE ("an audition the kit's own file fails says what fixes the kit, as a failed copy does", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;
    // What the sound thread reports for the latest audition, as the fake would hand it on after its ticket.
    const auto failWith = [&c] (const char* problem, const char* detail, bool sourceProblem, bool reinstall)
    {
        const auto ticket = c.auditionStatus.latest;
        c.auditionStatus = {};
        c.auditionStatus.latest = ticket;
        c.auditionStatus.problem = problem;
        c.auditionStatus.detail = detail;
        c.auditionStatus.sourceProblem = sourceProblem;
        c.auditionStatus.reinstall = reinstall;
    };

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            juce::StringArray said;
            drawer.announce = [&said] (const juce::String& text) { said.add (text); };

            // The installed file is gone (an unplugged drive, say): the line names the file in the kit and what helps, and
            // nothing else; where it was looked for is in the tooltip, and said.
            press (soundRow (editor, 0));
            failWith ("FUT01's Music/Reverse Stab v2.wav is missing",
                "missing: Music/Reverse Stab v2.wav: no such file in /Users/someone/Library/Application Support/Wubansia Audio/Wubansia/Content/fut01/v1",
                true, false);
            editor.tick();
            CHECK (soundRow (editor, 0).look().failed);
            const juce::String line ("Couldn't play Reverse Stab v2: FUT01's Music/Reverse Stab v2.wav is missing. Check that FUT01's samples are still installed.");
            CHECK (drawer.statusText() == line);
            const juce::String where ("Music/Reverse Stab v2.wav: no such file in /Users/someone/Library/Application Support/Wubansia Audio/Wubansia/Content/fut01/v1.");
            CHECK (drawer.statusTooltip() == line + "\n" + where);
            REQUIRE_FALSE (said.isEmpty());
            CHECK (said[said.size() - 1] == line + " " + where);

            // A content folder whose name holds "install" changes nothing: the advice comes from the state, not its words.
            press (soundRow (editor, 0));
            failWith ("FUT01's Music/Reverse Stab v2.wav is missing", "missing: Music/Reverse Stab v2.wav: no such file in /Users/someone/Music/Installed Kits/fut01/v1",
                true, false);
            editor.tick();
            CHECK (drawer.statusText() == line);
            CHECK (drawer.statusTooltip().endsWith ("/Users/someone/Music/Installed Kits/fut01/v1."));

            // Bytes that aren't kit.json's: the problem already says what to do, and there's no more to it.
            press (soundRow (editor, 0));
            failWith ("Reverse Stab v2.wav isn't the file FUT01's content v1 lists: reinstall FUT01", "", true, true);
            editor.tick();
            CHECK (drawer.statusText() == "Couldn't play Reverse Stab v2: Reverse Stab v2.wav isn't the file FUT01's content v1 lists: reinstall FUT01.");
            CHECK (drawer.statusTooltip() == drawer.statusText());
            CHECK (said[said.size() - 1] == drawer.statusText());

            // A check's code never shows.
            press (soundRow (editor, 0));
            failWith ("unreadable: Music/Reverse Stab v2.wav: permission denied", "", true, false);
            editor.tick();
            CHECK (drawer.statusText() == "Couldn't play Reverse Stab v2: Music/Reverse Stab v2.wav: permission denied. Check that FUT01's samples are still installed.");

            // Not the kit's file (a rate the clip can't be made at): no advice, and never Change...: no samples folder plays.
            press (soundRow (editor, 0));
            failWith ("Reverse Stab v2.wav can't be played at 800000 Hz", "", false, false);
            editor.tick();
            CHECK (drawer.statusText() == "Couldn't play Reverse Stab v2: Reverse Stab v2.wav can't be played at 800000 Hz.");
            CHECK (drawer.statusTooltip() == drawer.statusText());

            // A request turned down before a ticket reads the same way; Esc puts it away, tooltip and all.
            c.auditionFails = true;
            press (soundRow (editor, 0));
            CHECK (drawer.statusText() == "Couldn't play Reverse Stab v2: the fake can't play FUT01 file 159.");
            CHECK (drawer.statusTooltip() == drawer.statusText());
            CHECK (drawer.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
            editor.tick();
            CHECK (drawer.statusText().isEmpty());
            CHECK (drawer.statusTooltip().isEmpty());

            // The next audition clears the line and its tooltip.
            c.auditionFails = false;
            press (soundRow (editor, 0));
            failWith ("FUT01's Music/Reverse Stab v2.wav is missing", "missing: Music/Reverse Stab v2.wav: no such file in /Volumes/Kits/fut01/v1", true, false);
            editor.tick();
            CHECK (drawer.statusTooltip().endsWith ("/Volumes/Kits/fut01/v1."));
            press (soundRow (editor, 1));
            editor.tick();
            CHECK (soundRow (editor, 1).look().playing);
            CHECK (drawer.statusText().isEmpty());
            CHECK (drawer.statusTooltip().isEmpty());
        });
}

TEST_CASE ("a failed copy's or audition's line keeps what helps on the footer's two lines, for every file of the four kits", "[editor][sounds]")
{
    // Every file of the four kits, refused as exportCopy refuses one that's gone from the default macOS content folder
    // under a long user name, as the sound thread then refuses its audition, and as exportCopy refuses a copy, each way,
    // that a samples folder deep in the home folder can't take: the drawer's line must show all of itself, the advice
    // last, in the footer's font and in the status area the drawer lays out, beside the default samples folder and beside
    // one long enough to take the folder text's full width.
    const wubansia::KitCatalog catalog (wubansia::EmbeddedContent::sources());
    const auto contentFolder = [] (const std::string& kit)
    {
        return "/Users/christopher.henderson/Library/Application Support/Wubansia Audio/Wubansia/Content/" + juce::String (kit).toLowerCase().toStdString() + "/v1";
    };
    const auto refused = [&] (const wubansia::KitCatalog::Entry& entry, int file)
    {
        const auto& listed = entry.manifest.files[(std::size_t) file];
        const engine::FileSampleSource gone (std::filesystem::path (contentFolder (entry.id)));
        const auto read = gone.readBytes (listed.path);
        REQUIRE_FALSE (read.ok());
        return wubansia::sounds::soundExportOf (wubansia::sounds::unreadableSource (entry.info, listed, read.error));
    };

    // The sound thread's state for an audition of the same file: the copy's refusal, word for word and flag for flag.
    const auto unplayable = [&] (const wubansia::KitCatalog::Entry& entry, int file)
    {
        const auto copy = refused (entry, file);
        wubansia::AuditionState state;
        state.problem = copy.problem;
        state.detail = copy.detail;
        state.sourceProblem = copy.sourceProblem;
        state.reinstall = copy.reinstall;
        return state;
    };

    // The samples folder's refusals, every kind, for a copy into a folder deep in the home folder.
    using wubansia::sounds::FolderProblem;
    constexpr std::array<FolderProblem, 4> folderProblems { FolderProblem::makeFolder, FolderProblem::write, FolderProblem::move,
        FolderProblem::namesTaken };
    const auto deepSamples = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                 .getChildFile ("Music/Sample Libraries/Wubansia drags for the next album, kept here until it ships");
    const auto unwritable = [&] (const wubansia::KitCatalog::Entry& entry, int file, FolderProblem problem)
    {
        const auto& listed = entry.manifest.files[(std::size_t) file];
        const auto kitFolder = deepSamples.getChildFile (juce::String::fromUTF8 (wubansia::sounds::kitFolderName (entry.info).c_str()));
        const auto target = kitFolder.getChildFile (juce::String::fromUTF8 (wubansia::sounds::exportName (entry.info, listed).c_str()));
        return wubansia::sounds::soundExportOf (wubansia::sounds::folderRefusal (problem, problem == FolderProblem::makeFolder ? kitFolder : target));
    };
    const auto entryOf = [&] (const wubansia::SoundRef& ref) -> const wubansia::KitCatalog::Entry&
    {
        const auto* entry = catalog.find (ref.kit.toStdString());
        REQUIRE (entry != nullptr);
        return *entry;
    };

    Rig rig;
    auto& c = rig.controller;
    c.kitCards.clear();
    for (const auto& entry : catalog.entries())
    {
        REQUIRE (entry.ok());
        auto card = entry.card;
        card.content = {};
        card.content.state = wubansia::ContentState::ready;
        card.content.folder = juce::File (contentFolder (entry.id));
        c.kitCards.push_back (card);
        c.soundLists[entry.card.id] = entry.sounds;
    }
    c.contentStatus = c.kitCards.front().content;

    // The rail's categories in SoundCategory's order, as their buttons are named.
    constexpr std::array<const char*, wubansia::kSoundCategories> categoryIds { "accents", "fx", "vox", "synths", "bass", "drums", "loops" };

    // The glyph drawFittedText ends a cut line with, and dots no narrower than it, to stand in for a line's own.
    const auto font = editor::uiFont (11.5f, true);
    const auto ellipsisText = juce::String::charToString ((juce::juce_wchar) 0x2026);
    juce::GlyphArrangement dots;
    dots.addLineOfText (font, ellipsisText, 0.0f, 0.0f);
    REQUIRE (dots.getNumGlyphs() == 1);
    const auto ellipsis = dots.getGlyph (0).getGlyphIndex();
    juce::String standIn ("...");
    while (juce::GlyphArrangement::getStringWidth (font, standIn) < juce::GlyphArrangement::getStringWidth (font, ellipsisText))
        standIn << ".";

    for (const auto* samplesFolder : { "", "/Volumes/Studio Archive 2026/Sample Libraries/Wubansia drags for the next album" })
    {
        c.samplesFolderChosen = juce::String (samplesFolder).isEmpty() ? wubansia::sounds::defaultSamplesFolder() : juce::File (samplesFolder);
        withEditor (rig, [&] (PluginEditor& editor)
            {
                editor.setZoom (1.0f);
                auto& drawer = openDrawer (editor);
                Handoffs os;
                os.hook (drawer);
                const auto area = drawer.statusBounds();
                INFO ("status area " << area.toString());
                REQUIRE (area.getWidth() > 400);

                // A line on the footer's two lines, word by word: what one line leaves for the next is never more than a
                // word. And as drawFittedText lays it out here: nothing cut. Its width.
                const auto fits = [&] (const juce::String& line)
                {
                    INFO (line);
                    const auto width = juce::GlyphArrangement::getStringWidth (font, line);
                    float longestWord = 0.0f;
                    for (const auto& word : juce::StringArray::fromTokens (line, " ", ""))
                        longestWord = juce::jmax (longestWord, juce::GlyphArrangement::getStringWidth (font, word + " "));
                    CHECK (width + longestWord <= 2.0f * (float) area.getWidth());

                    // The line's own ellipsis ("Change…") laid out as dots no narrower than it, so that any ellipsis glyph
                    // drawn is drawFittedText's cut.
                    juce::GlyphArrangement drawn;
                    drawn.addFittedText (font, line.replace (ellipsisText, standIn), (float) area.getX(), (float) area.getY(), (float) area.getWidth(),
                        (float) area.getHeight(), juce::Justification::centredLeft, 2, 0.9f);
                    bool cut = false;
                    for (int g = 0; g < drawn.getNumGlyphs(); ++g)
                        cut = cut || drawn.getGlyph (g).getGlyphIndex() == ellipsis;
                    CHECK_FALSE (cut);
                    return width;
                };

                // The widest line of each kind, and the sound and file that give it.
                struct Widest
                {
                    juce::String line;
                    float width = 0.0f;
                    juce::String kit, name;
                    int file = 0; // into the sound's files: the File box's item
                };
                const auto keep = [] (Widest& widest, const juce::String& line, float width, const juce::String& kit, const wubansia::SoundInfo& sound, int file)
                {
                    if (width > widest.width)
                        widest = { line, width, kit, sound.name, file };
                };
                Widest copy, play;
                std::array<Widest, folderProblems.size()> folder;

                int lines = 0;
                for (const auto& entry : catalog.entries())
                    for (const auto& sound : entry.sounds)
                        for (int i = 0; i < (int) sound.files.size(); ++i)
                        {
                            const auto& file = sound.files[(std::size_t) i];
                            const auto& kit = entry.card.id;
                            ++lines;

                            // The kit's own file is why: the kit's samples help, and no code or folder shows.
                            const auto copyLine = editor::failedCopyLine (file.label, kit, refused (entry, file.file));
                            CHECK (copyLine.endsWith (". Check that " + kit + "'s samples are still installed."));
                            CHECK_FALSE (copyLine.contains ("missing:"));
                            CHECK_FALSE (copyLine.contains ("/Users/"));
                            keep (copy, copyLine, fits (copyLine), kit, sound, i);

                            // The same file, auditioned: the same words and advice, as "Couldn't play".
                            const auto playLine = editor::failedPlayLine (file.label, kit, unplayable (entry, file.file));
                            CHECK (playLine.startsWith ("Couldn't play " + file.label + ": "));
                            CHECK (playLine.endsWith (". Check that " + kit + "'s samples are still installed."));
                            CHECK_FALSE (playLine.contains ("missing:"));
                            CHECK_FALSE (playLine.contains ("/Users/"));
                            keep (play, playLine, fits (playLine), kit, sound, i);

                            // The samples folder is why: another one helps, and no path shows.
                            for (std::size_t k = 0; k < folderProblems.size(); ++k)
                            {
                                const auto folderLine = editor::failedCopyLine (file.label, kit, unwritable (entry, file.file, folderProblems[k]));
                                CHECK (folderLine.endsWith (editor::utf8 (". Choose another folder with Change\xe2\x80\xa6")));
                                CHECK_FALSE (folderLine.containsChar ('/'));
                                keep (folder[k], folderLine, fits (folderLine), kit, sound, i);
                            }
                        }
                CHECK (lines >= 618);

                // Each widest, through the drawer itself: its kit, its category, its row and its file (which plays it),
                // then a click on the tile, or the sound thread's refusal of that audition.
                const auto pick = [&] (const Widest& widest)
                {
                    INFO ("widest: " << widest.line);
                    click (get<editor::KitChip> (editor, "samples_kit_" + widest.kit));
                    const auto* entry = catalog.find (widest.kit.toStdString());
                    REQUIRE (entry != nullptr);
                    const auto it = std::find_if (entry->sounds.begin(), entry->sounds.end(), [&] (const wubansia::SoundInfo& s) { return s.name == widest.name; });
                    REQUIRE (it != entry->sounds.end());
                    click (get<editor::CategoryButton> (editor, "samples_category_" + juce::String (categoryIds[(std::size_t) it->category])));
                    editor.tick();
                    int row = -1;
                    for (int i = 0; i < drawer.numRows(); ++i)
                        if (soundRow (editor, i).look().name == widest.name)
                            row = i;
                    REQUIRE (row >= 0);
                    press (soundRow (editor, row));
                    get<juce::ComboBox> (editor, "sound_file").setSelectedItemIndex (widest.file, juce::sendNotificationSync);
                };
                const auto save = [&] (const Widest& widest)
                {
                    INFO ("widest: " << widest.line);
                    pick (widest);
                    click (get<editor::SoundDragTile> (editor, "sound_drag"));
                    CHECK (drawer.statusText() == widest.line);
                };
                c.exportOutcome = [&] (const wubansia::SoundRef& ref) { return refused (entryOf (ref), ref.file); };
                save (copy);
                for (std::size_t k = 0; k < folderProblems.size(); ++k)
                {
                    const auto problem = folderProblems[k];
                    c.exportOutcome = [&, problem] (const wubansia::SoundRef& ref) { return unwritable (entryOf (ref), ref.file, problem); };
                    save (folder[k]);
                }
                {
                    INFO ("widest: " << play.line);
                    pick (play);
                    const auto& shown = c.auditions.back();
                    auto failed = unplayable (entryOf (shown), shown.file);
                    failed.latest = c.auditionStatus.latest;
                    c.auditionStatus = failed;
                    editor.tick();
                    CHECK (drawer.statusText() == play.line);
                }
                CHECK (os.revealed.isEmpty());
            });
    }
}

TEST_CASE ("a kit chip browses another kit without selecting it; a kit without samples shows why", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            drawer.platform = editor::ContentPanel::Platform::macOS;
            Handoffs os;
            os.hook (drawer);
            auto& fut01 = get<editor::KitChip> (editor, "samples_kit_FUT01");
            auto& fut02 = get<editor::KitChip> (editor, "samples_kit_FUT02");
            CHECK (fut01.isBrowsed());
            CHECK (fut01.marksPlayingKit());
            CHECK_FALSE (fut02.isBrowsed());
            CHECK_FALSE (fut02.marksPlayingKit());
            CHECK (drawer.bannerText().isEmpty());

            click (fut02);
            editor.tick();
            CHECK (drawer.browsedKit() == "FUT02");
            CHECK (fut02.isBrowsed());
            CHECK_FALSE (fut01.isBrowsed());
            CHECK (fut01.marksPlayingKit()); // FUT01 still plays
            CHECK (c.selectedKits.isEmpty());
            CHECK (editor.model().kit.id == "FUT01");

            // FUT02's samples are missing: a banner says so and what to do, and its rows are dimmed, playing and
            // dragging nothing.
            CHECK (drawer.category() == wubansia::SoundCategory::accents);
            REQUIRE (drawer.numRows() == 1);
            CHECK (soundRow (editor, 0).look().name == "Glass Ping");
            CHECK (soundRow (editor, 0).look().dimmed);
            const auto banner = drawer.bannerText().toStdString();
            CHECK_THAT (banner, ContainsSubstring ("FUT02's samples aren't installed") && ContainsSubstring ("install-macos.sh")
                                    && ContainsSubstring ("the folder that holds 'fut02'"));
            CHECK_THAT (banner, ! ContainsSubstring ("no folder holds FUT02", Catch::CaseSensitive::No)); // the headline says it
            CHECK (get<juce::Button> (editor, "samples_content_folder").isVisible());
            press (soundRow (editor, 0));
            CHECK (c.auditions.empty());
            drag (soundRow (editor, 0), 8.0f);
            drag (get<editor::SoundDragTile> (editor, "sound_drag"), 8.0f);
            CHECK (c.exportRequests.empty());
            CHECK (os.dragged.isEmpty());
            CHECK_FALSE (get<editor::SoundDragTile> (editor, "sound_drag").isEnabled());
            CHECK_FALSE (get<juce::Button> (editor, "sound_play").isEnabled());
            CHECK_FALSE (get<juce::Button> (editor, "sound_reveal").isEnabled());

            // Linux gets its own folder.
            drawer.platform = editor::ContentPanel::Platform::linuxOS;
            editor.tick();
            CHECK_THAT (drawer.bannerText().toStdString(), ContainsSubstring ("~/.local/share/Wubansia Audio/Wubansia/Content"));

            // Its button chooses a content folder, as the install panel's does.
            const auto chosen = samples.folder.getChildFile ("Content");
            get<editor::ContentPanel> (editor, "content").chooseFolder = [&] (std::function<void (const juce::File&)> done) { done (chosen); };
            click (get<juce::Button> (editor, "samples_content_folder"));
            REQUIRE (c.contentFolders.size() == 1);
            CHECK (c.contentFolders[0] == chosen);

            // Once FUT02's samples are found, its sounds play, and FUT01 still plays the groove.
            c.kitCards[1].content.state = wubansia::ContentState::ready;
            c.kitCards[1].content.problem.clear();
            c.kitCards[1].content.folder = chosen.getChildFile ("fut02");
            editor.tick();
            CHECK (drawer.bannerText().isEmpty());
            CHECK_FALSE (soundRow (editor, 0).look().dimmed);
            CHECK (get<juce::Button> (editor, "sound_play").isEnabled());
            press (soundRow (editor, 0));
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT02", 111 });
            drag (get<editor::SoundDragTile> (editor, "sound_drag"), 8.0f);
            CHECK (c.exportRequests.back() == wubansia::SoundRef { "FUT02", 111 });
            CHECK (c.selectedKits.isEmpty());

            // While another kit's samples are looked for, or load.
            c.kitCards[1].content = {};
            c.kitCards[1].content.state = wubansia::ContentState::loading;
            editor.tick();
            CHECK (drawer.statusText() == editor::utf8 ("Looking for FUT02's samples\xe2\x80\xa6"));
            CHECK (soundRow (editor, 0).look().dimmed);
            CHECK (drawer.bannerText().isEmpty());
            c.kitCards[1].content.folder = chosen.getChildFile ("fut02");
            c.kitCards[1].content.percent = 42;
            editor.tick();
            CHECK (drawer.statusText() == editor::utf8 ("Loading FUT02's samples\xe2\x80\xa6 42 %"));

            // Back to FUT01.
            click (fut01);
            CHECK (drawer.browsedKit() == "FUT01");
            CHECK_FALSE (soundRow (editor, 0).look().dimmed);
            CHECK (drawer.bannerText().isEmpty());
            CHECK (drawer.statusText().isEmpty());
        });
}

TEST_CASE ("the banner says what's wrong once, in words, without the content check's codes", "[editor][sounds]")
{
    // The problems as the content check words them (ContentLocator, P4's FileSampleSource and checkKitFiles).
    struct Case
    {
        const char* what;
        wubansia::ContentState state;
        bool folder; // a kit folder was found, with something wrong in it
        const char* problem;
        const char* shows; // what the banner's second line holds; "" for nothing but its own advice
    };
    const Case cases[] {
        { "no folder holds it", wubansia::ContentState::missing, false,
            "No content folder holds FUT02 (fut02/v1 or fut02): install its content, or choose the folder that holds it", "" },
        { "a file missing from its folder", wubansia::ContentState::missing, true,
            "missing: Drums/Kick Split v1 rr1.wav: no such file in /x/fut02/v1", "Drums/Kick Split v1 rr1.wav: no such file in /x/fut02/v1. " },
        { "Git LFS pointers", wubansia::ContentState::lfsPointers, true,
            "lfs_pointer: Drums/Kick Split v1 rr1.wav: a Git LFS pointer file for 65729 bytes of audio, not the audio itself: fetch the "
            "samples (git lfs pull) or install the content",
            "" },
        { "another version's file", wubansia::ContentState::mismatch, true,
            "mismatch: Drums/Kick Split v1 rr1.wav: 1000 frames and 1 channel; kit.json lists 19518 frames and 1 channel",
            "Drums/Kick Split v1 rr1.wav: 1000 frames and 1 channel; kit.json lists 19518 frames and 1 channel. " },
        { "a 44.1 kHz file", wubansia::ContentState::mismatch, true, "rate: Drums/Kick Split v1 rr1.wav: 44100 Hz; the content is 48000 Hz",
            "Drums/Kick Split v1 rr1.wav: 44100 Hz; the content is 48000 Hz. " },
        { "a file that can't be read", wubansia::ContentState::failed, true, "unreadable: Drums/Kick Split v1 rr1.wav: permission denied",
            "Drums/Kick Split v1 rr1.wav: permission denied. " },
    };

    Rig rig;
    auto& c = rig.controller;
    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            drawer.platform = editor::ContentPanel::Platform::macOS;
            click (get<editor::KitChip> (editor, "samples_kit_FUT02"));
            for (const auto& k : cases)
            {
                CAPTURE (k.what);
                c.kitCards[1].content = {};
                c.kitCards[1].content.state = k.state;
                c.kitCards[1].content.problem = k.problem;
                if (k.folder)
                    c.kitCards[1].content.folder = juce::File ("/x/fut02/v1");
                editor.tick();
                const auto lines = juce::StringArray::fromLines (drawer.bannerText());
                REQUIRE (lines.size() == 2);
                const auto body = lines[1];
                CAPTURE (body.toStdString());
                CHECK (body.startsWith (k.shows));
                for (const auto* code : { "Missing:", "Mismatch:", "Lfs_pointer:", "Rate:", "Unreadable:", "missing:", "lfs_pointer:" })
                    CHECK_FALSE (body.contains (code));
                // Each piece of advice once: the drawer's own, not the check's too.
                CHECK (body.indexOf ("choose") == body.lastIndexOf ("choose"));
                CHECK (body.indexOf ("git lfs pull") == body.lastIndexOf ("git lfs pull"));
                CHECK_FALSE (body.contains ("No content folder holds"));
            }
        });
}

TEST_CASE ("the drawer follows the playing kit until a chip is picked", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;
    addKit (c, "FUT03", "Glass Current", "garage-house / deep tech house", "G minor", "6A", 128, {}, 0, {}, {});

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            CHECK (drawer.browsedKit() == "FUT01");

            // A kit card picks the kit that plays: the drawer follows it.
            click (get<editor::KitPanel::Card> (editor, "kit_FUT02"));
            editor.tick();
            CHECK (c.selectedKits == juce::StringArray { "FUT02" });
            CHECK (drawer.browsedKit() == "FUT02");
            CHECK (get<editor::KitChip> (editor, "samples_kit_FUT02").marksPlayingKit());

            // A chip picked: the drawer stays on that kit whatever plays.
            click (get<editor::KitChip> (editor, "samples_kit_FUT03"));
            CHECK (drawer.browsedKit() == "FUT03");
            c.selectKit ("FUT01");
            editor.tick();
            CHECK (drawer.browsedKit() == "FUT03");
            CHECK (get<editor::KitChip> (editor, "samples_kit_FUT01").marksPlayingKit());

            // The playing kit's own chip follows again.
            click (get<editor::KitChip> (editor, "samples_kit_FUT01"));
            c.selectKit ("FUT02");
            editor.tick();
            CHECK (drawer.browsedKit() == "FUT02");

            // A drawer opened again starts on the playing kit, at the category last picked.
            click (get<editor::KitChip> (editor, "samples_kit_FUT03"));
            click (get<juce::Button> (editor, "samples_close"));
            c.selectKit ("FUT01");
            editor.tick();
            openDrawer (editor);
            CHECK (drawer.browsedKit() == "FUT01");
            click (get<juce::Button> (editor, "samples_category_loops"));
            click (get<juce::Button> (editor, "samples_close"));
            openDrawer (editor);
            CHECK (drawer.category() == wubansia::SoundCategory::loops);

            // A kit without that category shows its first one, and the pick holds for the next kit: FUT02 has no FX.
            click (get<juce::Button> (editor, "samples_category_fx"));
            click (get<editor::KitChip> (editor, "samples_kit_FUT02"));
            CHECK (drawer.category() == wubansia::SoundCategory::accents);
            CHECK_FALSE (get<juce::Button> (editor, "samples_category_fx").isEnabled());
            click (get<editor::KitChip> (editor, "samples_kit_FUT01"));
            CHECK (drawer.category() == wubansia::SoundCategory::fx);

            // A kit with no sounds at all (this fake's FUT03) shows none, and nothing to drag.
            click (get<editor::KitChip> (editor, "samples_kit_FUT03"));
            CHECK (drawer.numRows() == 0);
            CHECK (drawer.selectedRow() == -1);
            CHECK_FALSE (get<juce::Button> (editor, "samples_category_accents").isEnabled());
            CHECK_FALSE (get<editor::SoundDragTile> (editor, "sound_drag").isEnabled());
            CHECK (c.selectedKits == juce::StringArray { "FUT02", "FUT01", "FUT02", "FUT01" });
        });
}

TEST_CASE ("the playing row shows Stop while its ticket sounds; a silent Logic gets the Logic hint", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.setLogicHostForTesting (true); // the plugin in Logic Pro
            auto& drawer = openDrawer (editor);
            auto& stop = get<juce::Button> (editor, "sound_stop");
            press (soundRow (editor, 1));
            editor.tick();
            CHECK (soundRow (editor, 1).look().playing);
            CHECK_FALSE (soundRow (editor, 0).look().playing);
            CHECK (stop.getToggleState()); // lit while something plays
            CHECK (drawer.statusText().isEmpty());

            // The clip ends.
            c.auditionStatus.sounding = 0;
            editor.tick();
            CHECK_FALSE (soundRow (editor, 1).look().playing);
            CHECK_FALSE (stop.getToggleState());

            // Being read, or waiting for the host's next block.
            press (soundRow (editor, 1));
            c.auditionStatus.sounding = 0;
            c.auditionStatus.waiting = true;
            editor.tick();
            CHECK (soundRow (editor, 1).look().waiting);
            CHECK_FALSE (soundRow (editor, 1).look().playing);

            // No block for 300 ms: the row shows play again, and the Logic hint shows.
            c.auditionStatus.waiting = false;
            c.auditionStatus.hostSilent = true;
            editor.tick();
            CHECK_FALSE (soundRow (editor, 1).look().waiting);
            CHECK_FALSE (soundRow (editor, 1).look().playing);
            CHECK_FALSE (soundRow (editor, 1).look().failed);
            CHECK (drawer.statusText()
                   == "No sound from your DAW: Logic only runs a stopped instrument track while it's selected or armed. Select this track, or press Play.");
            CHECK (drawer.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey))); // Esc puts the hint away
            editor.tick();
            CHECK (drawer.statusText().isEmpty());

            // A request turned down: the row turns the warning colour, and the status line says why.
            c.auditionFails = true;
            press (soundRow (editor, 0));
            CHECK (soundRow (editor, 0).look().failed);
            CHECK (drawer.statusText() == "Couldn't play Reverse Stab v2: the fake can't play FUT01 file 159.");
            editor.tick();
            CHECK (soundRow (editor, 0).look().failed);

            // One that fails on the sound thread, after its ticket: the same.
            c.auditionFails = false;
            press (soundRow (editor, 2));
            CHECK_FALSE (soundRow (editor, 0).look().failed);
            const auto ticket = c.auditionStatus.latest;
            c.auditionStatus = {};
            c.auditionStatus.latest = ticket;
            c.auditionStatus.problem = "Alloy Bell v4 rr1.wav isn't the file FUT01's content v1 lists: reinstall FUT01";
            c.auditionStatus.sourceProblem = true;
            c.auditionStatus.reinstall = true;
            editor.tick();
            CHECK (soundRow (editor, 2).look().failed);
            CHECK (drawer.statusText() == "Couldn't play Alloy Bell v4 rr1: Alloy Bell v4 rr1.wav isn't the file FUT01's content v1 lists: reinstall FUT01.");

            // The next audition clears it.
            press (soundRow (editor, 1));
            editor.tick();
            CHECK_FALSE (soundRow (editor, 2).look().failed);
            CHECK (soundRow (editor, 1).look().playing);
            CHECK (drawer.statusText().isEmpty());
        });
}

TEST_CASE ("no sound names Logic only in Logic: any other host gets words that fit every DAW", "[editor][sounds]")
{
    Rig rig;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            // The editor asks juce::PluginHostType which host runs it: this test binary is no DAW, as Live isn't Logic.
            // Its words send no one to a track's selection or to Play (Live runs no audio with its engine off, say).
            auto& drawer = openDrawer (editor);
            press (soundRow (editor, 1));
            c.auditionStatus.sounding = 0;
            c.auditionStatus.hostSilent = true;
            editor.tick();
            const juce::String anyDaw ("No sound from your DAW: it isn't running Wubansia's audio right now. Check that its audio engine is on and this "
                                       "track is active.");
            CHECK (drawer.statusText() == anyDaw);
            CHECK_FALSE (drawer.statusText().contains ("Logic"));
            CHECK_FALSE (drawer.statusText().contains ("Play"));

            // In Logic, Logic's own words; and a host given up mid-clip says the same as one that never took it.
            editor.setLogicHostForTesting (true);
            editor.tick();
            CHECK (drawer.statusText()
                   == "No sound from your DAW: Logic only runs a stopped instrument track while it's selected or armed. Select this track, or press Play.");
            editor.setLogicHostForTesting (false);
            editor.tick();
            CHECK (drawer.statusText() == anyDaw);
            CHECK (drawer.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey))); // Esc puts it away, as the others
            editor.tick();
            CHECK (drawer.statusText().isEmpty());
        });
}

TEST_CASE ("in the Standalone app, no sound says its audio output isn't running, not Logic's hint", "[editor][sounds]")
{
    FakeController controller;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    TestProcessor processor ([&controller] (TestProcessor& p) { return new PluginEditor (p, controller); });
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    REQUIRE (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    withEditor (processor, [&] (PluginEditor& editor)
        {
            // The app runs no blocks while no audio output is on: there is no DAW, track or Play to point at.
            auto& drawer = openDrawer (editor);
            press (soundRow (editor, 1));
            controller.auditionStatus.sounding = 0;
            controller.auditionStatus.hostSilent = true;
            editor.tick();
            CHECK (drawer.statusText() == "No sound: the app's audio output isn't running. Choose one in Options > Audio/MIDI Settings...");
            CHECK_FALSE (drawer.statusText().contains ("Logic"));
            CHECK (drawer.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey))); // Esc puts it away, as the hint
            editor.tick();
            CHECK (drawer.statusText().isEmpty());
        });
}

TEST_CASE ("Change... hands the chosen folder to the Controller", "[editor][sounds]")
{
    Rig rig;
    TempFolder temp;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            CHECK (drawer.folderText().startsWith ("Drags save to "));
            CHECK_THAT (drawer.folderText().toStdString(), ContainsSubstring ("WubansiaFakeSamples"));
            auto& change = get<juce::Button> (editor, "samples_folder");
            CHECK (change.getTooltip().isNotEmpty());

            const auto chosen = temp.folder.getChildFile ("Chosen Samples");
            drawer.chooseFolder = [&] (std::function<void (const juce::File&)> done) { done (chosen); };
            click (change);
            REQUIRE (c.samplesFolderRequests.size() == 1);
            CHECK (c.samplesFolderRequests[0] == chosen);
            CHECK_THAT (drawer.folderText().toStdString(), ContainsSubstring ("Chosen Samples"));
            CHECK (drawer.statusText().isEmpty());

            // A folder the Controller refuses (inside a content folder, say) changes nothing, and the drawer says why.
            c.refuseSamplesFolder = true;
            const auto refused = temp.folder.getChildFile ("Refused Samples");
            drawer.chooseFolder = [&] (std::function<void (const juce::File&)> done) { done (refused); };
            click (change);
            CHECK (c.samplesFolderRequests.size() == 2);
            CHECK_THAT (drawer.folderText().toStdString(), ContainsSubstring ("Chosen Samples"));
            CHECK_THAT (drawer.statusText().toStdString(), ContainsSubstring ("Refused Samples") && ContainsSubstring ("Choose another folder"));

            // A chooser closed without a pick changes nothing.
            drawer.chooseFolder = [] (std::function<void (const juce::File&)>) {};
            click (change);
            CHECK (c.samplesFolderRequests.size() == 2);
        });
}

TEST_CASE ("the drawer's rows and tile work without a mouse, in the drawer's order", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            Handoffs os;
            os.hook (drawer);

            // A row reads its name and length; its press action plays it.
            auto& rave = soundRow (editor, 1);
            CHECK (rave.getTitle() == "Rave Stab, 0.71 seconds");
            const auto rowHandler = rave.createAccessibilityHandler();
            REQUIRE (rowHandler != nullptr);
            CHECK (rowHandler->getRole() == juce::AccessibilityRole::listItem);
            CHECK (rowHandler->getActions().invoke (juce::AccessibilityActionType::press));
            REQUIRE_FALSE (c.auditions.empty());
            CHECK (c.auditions.back() == wubansia::SoundRef { "FUT01", 166 });
            CHECK (drawer.selectedRow() == 1);

            // The tile's press action copies the file and shows it: the keyboard's way to a file. Return does the same.
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");
            CHECK (tile.getWantsKeyboardFocus());
            const auto tileHandler = tile.createAccessibilityHandler();
            REQUIRE (tileHandler != nullptr);
            CHECK (tileHandler->getActions().invoke (juce::AccessibilityActionType::press));
            REQUIRE_FALSE (c.exportRequests.empty());
            CHECK (c.exportRequests.back() == wubansia::SoundRef { "FUT01", 166 });
            CHECK (os.revealed.size() == 1);
            CHECK (tile.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
            CHECK (os.revealed.size() == 2);
            CHECK_FALSE (tile.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
            CHECK (os.dragged.isEmpty());

            // The order: chips, categories, rows, File, the tile, Play, Show in Finder.
            juce::StringArray order;
            for (auto* component : juce::FocusTraverser().getAllComponents (&drawer))
                if (component->getComponentID().isNotEmpty())
                    order.add (component->getComponentID());
            INFO (order.joinIntoString (" "));
            const juce::StringArray expected { "samples_kit_FUT01", "samples_kit_FUT02", "samples_category_accents", "samples_category_loops",
                "sound_row_0", "sound_row_2", "sound_file", "sound_drag", "sound_play", "sound_reveal" };
            for (int i = 0; i < expected.size(); ++i)
            {
                INFO (expected[i]);
                CHECK (order.contains (expected[i]));
                if (i > 0)
                    CHECK (order.indexOf (expected[i - 1]) < order.indexOf (expected[i]));
            }
        });
}

TEST_CASE ("screen readers hear which kit and category show, what the tile does, and the drawer's messages", "[editor][sounds]")
{
    Rig rig;
    TempFolder samples;
    rig.controller.samplesFolderChosen = samples.folder;
    auto& c = rig.controller;
    // A vox sound without a lead-in ahead of Vox Synth (6 ms), and a loop with an echo tail, as FUT03's Context Vox Riff
    // has: 6 bars, 4 of them loop.
    auto& list = c.soundLists["FUT01"];
    REQUIRE (list[4].name == "Vox Synth");
    auto chop = list[4];
    chop.id = "FUT01/vox_chop";
    chop.name = "Vox Chop";
    chop.leadInMs = 0;
    list.insert (list.begin() + 4, chop);
    auto& phrase = list.back();
    REQUIRE (phrase.category == wubansia::SoundCategory::loops);
    phrase.bars = 6;
    phrase.loopBars = 4;

    withEditor (rig, [&] (PluginEditor& editor)
        {
            auto& drawer = openDrawer (editor);
            drawer.platform = editor::ContentPanel::Platform::macOS;
            juce::StringArray said;
            drawer.announce = [&said] (const juce::String& text) { said.add (text); };
            Handoffs os;
            os.hook (drawer);
            auto& fut01 = get<editor::KitChip> (editor, "samples_kit_FUT01");
            auto& fut02 = get<editor::KitChip> (editor, "samples_kit_FUT02");

            // The chips: the kit, then whether it shows and whether it plays. Not toggles: a press browses.
            CHECK (fut01.getTitle() == "FUT01 Alloy Signal, shown, playing");
            CHECK (fut02.getTitle() == "FUT02 Orbit Relay");
            CHECK_FALSE (fut01.isToggleable());

            // The categories: the name, how many, and the one shown.
            CHECK (get<juce::Button> (editor, "samples_category_accents").getTitle() == "Accents, 3 sounds, shown");
            CHECK (get<juce::Button> (editor, "samples_category_fx").getTitle() == "FX, 1 sound");
            click (get<juce::Button> (editor, "samples_category_fx"));
            CHECK (get<juce::Button> (editor, "samples_category_fx").getTitle() == "FX, 1 sound, shown");
            CHECK (get<juce::Button> (editor, "samples_category_accents").getTitle() == "Accents, 3 sounds");
            click (get<juce::Button> (editor, "samples_category_accents"));

            // The tile: its visible words first, then the file; its help is the detail pane's hint (how the sound sits on
            // the grid), then what a press does.
            auto& tile = get<editor::SoundDragTile> (editor, "sound_drag");
            CHECK (tile.getTitle() == "Drag into your DAW: Reverse Stab v2 - FUT01 134 F#m 11A.wav");
            CHECK (drawer.hintText() == "Ends on the beat: put its end on the downbeat");
            CHECK (tile.getHelpText() == "Ends on the beat: put its end on the downbeat. Press to save it into your samples folder and show it in Finder");

            // Every row's title says what its hint says of the grid, selected or not: an end on the beat, a lead-in (slide
            // it earlier), a loop's echo tail (not a plain loop of its length).
            CHECK (soundRow (editor, 0).getTitle() == "Reverse Stab, 1.30 seconds, ends on the beat");
            CHECK (soundRow (editor, 1).getTitle() == "Rave Stab, 0.71 seconds");
            click (get<juce::Button> (editor, "samples_category_vox"));
            CHECK (soundRow (editor, 0).getTitle() == "Vox Chop, 1.90 seconds");
            CHECK_FALSE (soundRow (editor, 1).look().selected);
            CHECK (soundRow (editor, 1).getTitle() == "Vox Synth, 1.90 seconds, starts 6 ms before its beat");
            press (soundRow (editor, 1));
            CHECK (soundRow (editor, 1).look().selected);
            CHECK (soundRow (editor, 1).getTitle() == "Vox Synth, 1.90 seconds, starts 6 ms before its beat");
            CHECK (drawer.hintText() == "Starts 6 ms before its beat: slide it 6 ms earlier");
            CHECK (tile.getHelpText() == "Starts 6 ms before its beat: slide it 6 ms earlier. Press to save it into your samples folder and show it in Finder");
            click (get<juce::Button> (editor, "samples_category_loops"));
            CHECK (soundRow (editor, 0).getTitle() == "Chrome Pluck Phrase, synth loop, 6 bars, a 4-bar loop, then a 2-bar echo tail");
            CHECK (drawer.hintText() == "6 bars at 134 BPM: a 4-bar loop, then a 2-bar echo tail");
            CHECK (tile.getHelpText() == "6 bars at 134 BPM: a 4-bar loop, then a 2-bar echo tail. Press to save it into your samples folder and show it in Finder");
            click (get<juce::Button> (editor, "samples_category_accents"));
            CHECK (tile.getTitle() == "Drag into your DAW: Reverse Stab v2 - FUT01 134 F#m 11A.wav");

            // What the status line says is said aloud once: a copy saved, a copy that failed, the host's silence.
            click (tile);
            CHECK (said.contains ("Saved Reverse Stab v2 - FUT01.wav"));
            c.exportFails = true;
            click (tile);
            CHECK (tile.getTitle() == "Couldn't save " + tile.fileName()); // the copy as last written
            CHECK (said.contains (editor::utf8 ("Couldn't save Reverse Stab v2: the fake's disk is full. Choose another folder with Change\xe2\x80\xa6")));
            c.exportFails = false;
            press (soundRow (editor, 1));
            c.auditionStatus.sounding = 0;
            c.auditionStatus.hostSilent = true;
            const auto before = said.size();
            for (int i = 0; i < 5; ++i)
                editor.tick();
            CHECK (said.size() == before + 1); // once, not every tick
            CHECK (said[said.size() - 1].startsWith ("No sound from your DAW"));

            // A kit that can't play: its chips say so, the banner is said, and its button reads it too.
            click (fut02);
            editor.tick();
            CHECK (fut02.getTitle() == "FUT02 Orbit Relay, shown");
            CHECK (fut01.getTitle() == "FUT01 Alloy Signal, playing");
            CHECK (said.contains ("FUT02's samples aren't installed. Install it with install-macos.sh, or choose the folder that holds 'fut02'."));
            CHECK (get<juce::Button> (editor, "samples_content_folder").getDescription() == said[said.size() - 1]);

            // While its samples load, that is said once, not every percent.
            c.kitCards[1].content = {};
            c.kitCards[1].content.state = wubansia::ContentState::loading;
            c.kitCards[1].content.folder = samples.folder.getChildFile ("fut02");
            const auto loading = said.size();
            for (const auto percent : { 10, 42, 77 })
            {
                c.kitCards[1].content.percent = percent;
                editor.tick();
            }
            CHECK (said.size() == loading + 1);
            CHECK (said[said.size() - 1] == editor::utf8 ("Loading FUT02's samples\xe2\x80\xa6"));
        });
}

TEST_CASE ("the tile names exactly the file a drag gives, for every file of the four kits", "[editor][sounds]")
{
    const wubansia::KitCatalog catalog (wubansia::EmbeddedContent::sources());
    std::set<std::pair<std::string, int>> files;
    for (const auto& entry : catalog.entries())
    {
        REQUIRE (entry.ok());
        for (const auto& sound : entry.sounds)
            for (const auto& file : sound.files)
            {
                const auto& listed = entry.manifest.files[(std::size_t) file.file];
                INFO (listed.path);
                CHECK (editor::exportFileName (entry.card, file).toStdString() == wubansia::sounds::exportName (entry.info, listed));
                files.insert ({ entry.id, file.file });
            }
    }
    CHECK (files.size() == 618);

    // From content v2 on, the name says which.
    const auto& fut01 = catalog.entries().front();
    auto card = fut01.card;
    card.contentVersion = 2;
    const auto& rave = fut01.sounds.front().files.front();
    CHECK (editor::exportFileName (card, rave).endsWith (" c2.wav"));
}

// Renders the editor at 2x into PNGs for the lead: each kit in Loop mode while playing, FUT02 in
// Song mode, FUT01 with its content missing, and the Samples drawer (at 1x too) on FUT01's Accents and on
// FUT02 browsed while its samples are missing. Run with:
//   WUBANSIA_SCREENSHOTS=<folder> WubansiaPluginTests "[.screenshot]"
TEST_CASE ("editor screenshots", "[.screenshot]")
{
    const auto folderPath = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_SCREENSHOTS", {});
    if (folderPath.isEmpty())
        SKIP ("WUBANSIA_SCREENSHOTS names no folder");
    const juce::File folder (folderPath);
    REQUIRE (folder.createDirectory());

    Rig rig;
    auto& c = rig.controller;
    c.kitCards[1].content.state = wubansia::ContentState::ready;
    addKit (c, "FUT03", "Glass Current", "garage-house / deep tech house", "G minor", "6A", 128,
        { "Kick", "Clap", "Hats", "Perc", "Bass", "Keys", "Stab", "Vox", "Accents", "FX" },
        engine::laneBit (engine::Lane::bass) | engine::laneBit (engine::Lane::vox), { "", "Rubber Clean", "" }, { "", "Rubber Warm", "" });
    addKit (c, "FUT04", "Furnace Code", "dark industrial / techno", "E minor", "9A", 136,
        { "Kick", "Snare", "Hats", "Perc", "Acid", "Blip", "Drone", "Vox", "Accents", "FX" },
        engine::laneBit (engine::Lane::bass) | engine::laneBit (engine::Lane::lead), { "", "Acid Clean", "" }, { "", "Acid Warm", "" });

    const std::map<juce::String, juce::StringArray> phraseFiles {
        { "FUT01", { "FUT01_Phrase_EPianoStabRhythm_2bars.mid", "FUT01_Phrase_OrganRiff_4bars.mid", "FUT01_Phrase_PluckRiff_4bars.mid", "FUT01_Phrase_VoxSynthRiff_2bars.mid", "FUT01_Phrase_WarpRiff_2bars.mid" } },
        { "FUT02", { "FUT02_Phrase_GlassMelody_4bars.mid", "FUT02_Phrase_LaserStabRiff_2bars.mid", "FUT02_Phrase_VoxChoirChords_4bars.mid", "FUT02_Phrase_VoxSynthRiff_2bars.mid" } },
        { "FUT03", { "FUT03_Phrase_DubChords_4bars.mid", "FUT03_Phrase_KeysArp_4bars.mid", "FUT03_Phrase_RubberRiff_2bars.mid", "FUT03_Phrase_VoxSynthRiff_2bars.mid" } },
        { "FUT04", { "FUT04_Phrase_AcidStabRiff_2bars.mid", "FUT04_Phrase_MetalChords_4bars.mid", "FUT04_Phrase_RaveStabRiff_2bars.mid", "FUT04_Phrase_VoxSynthRiff_2bars.mid" } },
    };
    using Song = std::vector<wubansia::SectionInfo>;
    const Song houseForm { { "Intro", 1, 32 }, { "Groove", 33, 32 }, { "Break", 65, 16 }, { "Build", 81, 16 }, { "Drop", 97, 32 },
        { "Switch Up", 129, 16 }, { "Break 2", 145, 16 }, { "Drop 2", 161, 32 }, { "Outro", 193, 32 } };
    const std::map<juce::String, Song> songs {
        { "FUT01", { { "Intro", 1, 16 }, { "Groove", 17, 48 }, { "Break", 65, 16 }, { "Build", 81, 16 }, { "Drop", 97, 32 }, { "Switch Up", 129, 16 }, { "Break 2", 145, 16 }, { "Drop 2", 161, 32 }, { "Outro", 193, 32 } } },
        { "FUT02", houseForm },
        { "FUT03", houseForm },
        { "FUT04", { { "Intro", 1, 32 }, { "Groove", 33, 64 }, { "Break", 97, 16 }, { "Build", 113, 16 }, { "Drop", 129, 32 }, { "Drop 2", 161, 32 }, { "Outro", 193, 32 } } },
    };
    const auto contentRoot = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                 .getChildFile ("Library/Application Support/Wubansia Audio/Wubansia/Content");

    const auto save = [&] (PluginEditor& editor, const juce::String& name, int scale = 2)
    {
        juce::Image image (juce::Image::ARGB, editor.getWidth() * scale, editor.getHeight() * scale, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            editor.paintEntireComponent (g, false);
        }
        const auto file = folder.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream out (file);
        REQUIRE (out.openedOk());
        juce::PNGImageFormat png;
        REQUIRE (png.writeImageToStream (image, out));
        std::cout << "screenshot: " << file.getFullPathName() << std::endl;
    };

    // A few ticks of a groove: hits on the beat, peaks that move, the club master working.
    const auto play = [&] (PluginEditor& editor, int bpm, double startTicks, std::uint32_t recorded, float grDb)
    {
        juce::Random random (bpm);
        for (int step = 0; step < 14; ++step)
        {
            auto& s = c.nextSnapshot;
            s.playing = true;
            s.bpm = bpm;
            s.songTicks = startTicks + step * 120.0;
            s.phraseRecorded = recorded;
            s.voices = 18 + step % 7;
            s.gainReductionDb = grDb + 0.4f * std::sin ((float) step);
            std::uint32_t hits = 0;
            if (step % 4 == 0)
                hits |= engine::laneBit (engine::Lane::kick) | engine::laneBit (engine::Lane::chords);
            if (step % 2 == 1)
                hits |= engine::laneBit (engine::Lane::hats);
            if (step == 9 || step == 13)
                hits |= engine::laneBit (engine::Lane::snare) | engine::laneBit (engine::Lane::accents);
            if (step % 3 == 0)
                hits |= engine::laneBit (engine::Lane::bass) | engine::laneBit (engine::Lane::perc);
            if (step % 5 == 2)
                hits |= engine::laneBit (engine::Lane::lead) | engine::laneBit (engine::Lane::vox);
            if (step == 11)
                hits |= engine::laneBit (engine::Lane::fx);
            s.laneHits = hits;
            const float level[] = { 0.86f, 0.58f, 0.33f, 0.3f, 0.72f, 0.46f, 0.4f, 0.52f, 0.28f, 0.2f };
            for (int l = 0; l < engine::kLanes; ++l)
                s.lanePeak[(std::size_t) l] = ((hits >> l) & 1u) != 0 ? level[l] * (0.85f + 0.15f * random.nextFloat())
                                                                       : level[l] * 0.25f * random.nextFloat();
            editor.tick();
        }
    };

    for (const juce::String kit : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        c.selectKit (kit);
        c.contentStatus.state = wubansia::ContentState::ready;
        c.contentStatus.folder = contentRoot.getChildFile (kit.toLowerCase()).getChildFile ("v1");
        c.phraseFileNames = phraseFiles.at (kit);
        c.sectionList = songs.at (kit);
        c.transportState = {};
        for (const auto& card : c.kitCards)
            if (card.id == kit)
                c.transportState.bpm = card.bpm;
        rig.processor.set (ids::playMode, 1.0f);

        // A mix someone has started on, so the faders aren't all at 0 dB.
        const float levels[] = { 0.0f, -2.5f, -4.0f, -6.5f, 0.0f, -3.0f, -5.0f, -1.5f, -7.5f, -9.0f };
        for (std::size_t l = 0; l < ids::laneLevel.size(); ++l)
            rig.processor.set (ids::laneLevel[l], levels[l]);

        withEditor (rig, [&] (PluginEditor& editor)
            {
                const auto bpm = editor.model().kit.bpm;
                play (editor, bpm, 3840.0 + 960.0 * 2.0, editor.model().kit.phraseLanes, 1.6f);
                save (editor, kit + "-loop-playing.png");

                if (kit == "FUT02")
                {
                    rig.processor.set (ids::playMode, 2.0f);
                    c.nextSnapshot.section = 4;
                    play (editor, bpm, 3840.0 * 108.0, editor.model().kit.phraseLanes, 2.1f);
                    save (editor, "FUT02-song-mode.png");
                    c.nextSnapshot.section = -1;
                    rig.processor.set (ids::playMode, 1.0f);
                }
            });
    }

    // FUT04 in the Standalone app: Play/Stop and the tempo instead of Preview.
    {
        FakeController standaloneController;
        standaloneController.kitCards = c.kitCards;
        standaloneController.selectKit ("FUT04");
        standaloneController.contentStatus.folder = contentRoot.getChildFile ("fut04/v1");
        standaloneController.phraseFileNames = phraseFiles.at ("FUT04");
        standaloneController.sectionList = songs.at ("FUT04");
        standaloneController.transportState = { true, true, 136.0, 0.0 };
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        TestProcessor app ([&standaloneController] (TestProcessor& p) { return new PluginEditor (p, standaloneController); });
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        withEditor (app, [&] (PluginEditor& editor)
            {
                auto& snapshot = standaloneController.nextSnapshot;
                for (int step = 0; step < 6; ++step)
                {
                    snapshot.playing = true;
                    snapshot.bpm = 136.0;
                    snapshot.songTicks = 3840.0 * 3.0 + step * 240.0;
                    snapshot.laneHits = step % 2 == 0 ? 0x3ffu : 0x015u;
                    snapshot.lanePeak.fill (0.4f);
                    snapshot.gainReductionDb = 1.2f;
                    snapshot.phraseRecorded = editor.model().kit.phraseLanes;
                    editor.tick();
                }
                save (editor, "FUT04-standalone.png");
            });
    }

    // FUT01 with its samples missing.
    c.selectKit ("FUT01");
    c.contentStatus = {};
    c.contentStatus.state = wubansia::ContentState::missing;
    c.contentStatus.problem = "no folder holds FUT01";
    c.contentStatus.searched = { contentRoot.getFullPathName(),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Music/Wubansia Content").getFullPathName() };
    c.kitCards[0].content = c.contentStatus;
    c.nextSnapshot = {};
    c.phraseFileNames = phraseFiles.at ("FUT01");
    c.sectionList = songs.at ("FUT01");
    c.transportState = {};
    c.transportState.bpm = 134.0;
    withEditor (rig, [&] (PluginEditor& editor)
        {
            get<editor::ContentPanel> (editor, "content").platform = editor::ContentPanel::Platform::macOS;
            editor.tick();
            REQUIRE (editor.isContentPanelShown());
            save (editor, "FUT01-content-missing.png");
        });

    // The Samples drawer, with the four kits' real sounds: FUT01's Accents with Rave Stab playing, and FUT02
    // browsed while its samples are missing.
    const wubansia::KitCatalog catalog (wubansia::EmbeddedContent::sources());
    for (const auto& entry : catalog.entries())
        c.soundLists[entry.card.id] = entry.sounds;
    c.contentStatus = {};
    c.contentStatus.state = wubansia::ContentState::ready;
    c.contentStatus.folder = contentRoot.getChildFile ("fut01/v1");
    c.kitCards[0].content = c.contentStatus;
    c.kitCards[1].content = {};
    c.kitCards[1].content.state = wubansia::ContentState::missing;
    c.kitCards[1].content.problem = "no content folder holds FUT02";
    c.samplesFolderChosen = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Wubansia/Samples");
    rig.processor.set (ids::playMode, 1.0f);
    withEditor (rig, [&] (PluginEditor& editor)
        {
            editor.setZoom (1.0f);
            play (editor, 134, 3840.0 + 960.0 * 2.0, editor.model().kit.phraseLanes, 1.6f);
            auto& drawer = openDrawer (editor);
            drawer.platform = editor::ContentPanel::Platform::macOS;
            REQUIRE (drawer.category() == wubansia::SoundCategory::accents);
            auto rave = -1;
            for (int i = 0; i < drawer.numRows(); ++i)
                if (soundRow (editor, i).look().name == "Rave Stab")
                    rave = i;
            REQUIRE (rave >= 0);
            press (soundRow (editor, rave));
            play (editor, 134, 3840.0 * 2.0, editor.model().kit.phraseLanes, 1.6f);
            save (editor, "FUT01-samples-accents.png");
            save (editor, "FUT01-samples-accents-1x.png", 1);

            c.auditionStatus.sounding = 0; // Rave Stab has ended
            click (get<editor::KitChip> (editor, "samples_kit_FUT02"));
            editor.tick();
            REQUIRE (drawer.bannerText().isNotEmpty());
            save (editor, "FUT02-samples-missing.png");
            save (editor, "FUT02-samples-missing-1x.png", 1);
        });
}
