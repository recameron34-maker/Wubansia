// The editor (P10), built as a host builds it but with no native window, against the FakeController
// (helpers/fake_controller.h) and the test processor holding the real parameter layout
// (helpers/test_processor.h). Painting into an image runs the drawing code; synthetic mouse events
// drive the controls the way a person does.
//
// The hidden [.screenshot] case renders the editor at 2x into PNG files in $WUBANSIA_SCREENSHOTS.

#include "PluginEditor.h"
#include "helpers/fake_controller.h"
#include "helpers/test_processor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <thread>

using Catch::Matchers::ContainsSubstring;
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

    // Creates the editor as a host does (no native window), runs the code, then deletes it.
    void withEditor (TestProcessor& processor, const std::function<void (PluginEditor&)>& code)
    {
        auto* created = processor.createEditorAndMakeActive();
        REQUIRE (created != nullptr);
        auto* editorComponent = dynamic_cast<PluginEditor*> (created);
        REQUIRE (editorComponent != nullptr);
        code (*editorComponent);
        processor.editorBeingDeleted (created);
        delete created;
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

            // Closing hides it until the status changes; the Samples button brings it back.
            click (get<juce::Button> (editor, "content_close"));
            editor.tick();
            CHECK_FALSE (editor.isContentPanelShown());
            click (get<juce::Button> (editor, "samples"));
            editor.tick();
            CHECK (editor.isContentPanelShown());
            click (get<juce::Button> (editor, "samples"));
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

TEST_CASE ("the transitional editor without a Controller still binds the parameters", "[editor]")
{
    TestProcessor processor ([] (TestProcessor& p) { return new PluginEditor (p); });
    withEditor (processor, [&] (PluginEditor& editor)
        {
            CHECK_FALSE (editor.model().hasKit());
            CHECK (findAll (editor, ids::masterGain).size() == 1);
            CHECK (editor.isContentPanelShown());
            juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
            juce::Graphics g (image);
            editor.paintEntireComponent (g, false);
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

// Renders the editor at 2x into PNGs for the lead: each kit in Loop mode while playing, FUT02 in
// Song mode, and FUT01 with its content missing. Run with:
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
        { "FUT01", { "FUT01_Phrase_ChromePluck_4bars.mid", "FUT01_Phrase_OrganLead_4bars.mid", "FUT01_Phrase_VoxSynth_2bars.mid", "FUT01_Phrase_WarpClean_2bars.mid" } },
        { "FUT02", { "FUT02_Phrase_GlassPing_4bars.mid", "FUT02_Phrase_LaserChordStab_2bars.mid", "FUT02_Phrase_VoxSynth_4bars.mid" } },
        { "FUT03", { "FUT03_Phrase_DubStabGm9_4bars.mid", "FUT03_Phrase_KeysEP_4bars.mid", "FUT03_Phrase_RubberBassClean_2bars.mid", "FUT03_Phrase_VoxSynth_2bars.mid" } },
        { "FUT04", { "FUT04_Phrase_AccentAcidStabE4_2bars.mid", "FUT04_Phrase_AccentMetalChordEm_4bars.mid", "FUT04_Phrase_VoxSynth_2bars.mid" } },
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

    const auto save = [&] (PluginEditor& editor, const juce::String& name)
    {
        juce::Image image (juce::Image::ARGB, editor.getWidth() * 2, editor.getHeight() * 2, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (2.0f));
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
}
