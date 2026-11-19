#pragma once

// What the editor tests share (EditorTests.cpp, LooksTests.cpp): a rig whose editor talks to a FakeController, opening
// and closing the editor as a host does, finding components by ID, mouse events, and the host's view of gestures.

#include "ParameterIds.h"
#include "PluginEditor.h"
#include "helpers/fake_controller.h"
#include "helpers/test_processor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <vector>

namespace wubansia::test::ui
{
    using wubansia::PluginEditor;
    namespace ids = wubansia::ParameterIds;

    // A processor whose editor talks to a FakeController.
    struct Rig
    {
        FakeController controller;
        TestProcessor processor { [this] (TestProcessor& p) { return new PluginEditor (p, controller); } };
    };

    // Creates the editor as a host does (no native window), runs the code, then deletes it, however the code
    // ends: a failed REQUIRE throws, and an editor left behind would go on ticking against a controller that's gone.
    inline void withEditor (TestProcessor& processor, const std::function<void (PluginEditor&)>& code)
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

    inline void withEditor (Rig& rig, const std::function<void (PluginEditor&)>& code) { withEditor (rig.processor, code); }

    inline void collectById (juce::Component& root, const juce::String& id, std::vector<juce::Component*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (child->getComponentID() == id)
                found.push_back (child);
            collectById (*child, id, found);
        }
    }

    inline std::vector<juce::Component*> findAll (juce::Component& root, const juce::String& id)
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
    inline juce::MouseEvent mouseAt (juce::Component& component, juce::Point<float> position, juce::Point<float> downAt, bool dragged = false)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
            juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
            juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, &component, &component, now, downAt, now, 1, dragged };
    }

    // A click: down and up at one point.
    inline void click (juce::Component& component, juce::Point<float> at)
    {
        component.mouseDown (mouseAt (component, at, at));
        component.mouseUp (mouseAt (component, at, at));
    }

    inline void click (juce::Component& component) { click (component, component.getLocalBounds().toFloat().getCentre()); }

    // A vertical drag on a knob or fader: down at its centre, up by `pixels`, release.
    inline void dragUp (juce::Component& component, float pixels)
    {
        const auto start = component.getLocalBounds().toFloat().getCentre();
        component.mouseDown (mouseAt (component, start, start));
        for (int step = 1; step <= 4; ++step)
            component.mouseDrag (mouseAt (component, start.translated (0.0f, -pixels * (float) step / 4.0f), start, true));
        component.mouseUp (mouseAt (component, start.translated (0.0f, -pixels), start, true));
    }

    // A drag along a slider's axis: up a vertical fader or knob, right along a horizontal one (deep glass's bars,
    // machine room's faders).
    inline void dragAlong (juce::Slider& slider, float pixels)
    {
        if (! slider.isHorizontal())
        {
            dragUp (slider, pixels);
            return;
        }
        const auto start = slider.getLocalBounds().toFloat().getCentre();
        slider.mouseDown (mouseAt (slider, start, start));
        for (int step = 1; step <= 4; ++step)
            slider.mouseDrag (mouseAt (slider, start.translated (pixels * (float) step / 4.0f, 0.0f), start, true));
        slider.mouseUp (mouseAt (slider, start.translated (pixels, 0.0f), start, true));
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
    inline float shownValue (juce::Component& control, juce::RangedAudioParameter& parameter)
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
    inline float otherValue (juce::RangedAudioParameter& parameter)
    {
        const auto id = parameter.getParameterID();
        const auto def = parameter.convertFrom0to1 (parameter.getDefaultValue());
        if (id.endsWith ("_level") || id == ids::masterGain)
            return -12.5f;
        if (id == ids::space || id == ids::echo || id == ids::pump)
            return 150.0f;
        return std::fmod (def + 1.0f, (float) parameter.getNumSteps());
    }

}
