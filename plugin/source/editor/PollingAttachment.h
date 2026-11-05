#pragma once

// Binds one control to one parameter without listening to the parameter (ADR 0001 D5, items 3-4).
// The editor's 30 Hz timer calls poll (), which shows the parameter's current value unless the person
// is in the middle of a gesture; the person's own changes write through beginChangeGesture (),
// setValueNotifyingHost () and endChangeGesture (). No parameter listener is ever registered, so host
// automation on the audio thread never posts to the message thread for the editor, and opening or
// closing the editor never takes a parameter's listener lock.

#include <juce_audio_processors/juce_audio_processors.h>

namespace wubansia::editor
{
    class PollingAttachment
    {
    public:
        // A slider takes the parameter's range, text and default (double-click); its drags are gestures.
        PollingAttachment (juce::Slider&, juce::RangedAudioParameter&);
        // A toggle shows on at >= 0.5 and writes 0 or 1: booleans, and two-way choices (B is on).
        PollingAttachment (juce::Button&, juce::RangedAudioParameter&);
        // A combo box's items are the choices (filled from the parameter when it has none).
        PollingAttachment (juce::ComboBox&, juce::RangedAudioParameter&);
        ~PollingAttachment();

        PollingAttachment (const PollingAttachment&) = delete;
        PollingAttachment& operator= (const PollingAttachment&) = delete;

        // The timer's work: show the parameter's value if it changed since the control last showed it.
        void poll();

        juce::RangedAudioParameter& getParameter() const noexcept { return parameter; }
        juce::Component& getControl() const noexcept { return control; }
        bool isInGesture() const noexcept { return inGesture; }

    private:
        void beginGesture();
        void endGesture();
        void write (float normalised); // inside the gesture in progress, or as a gesture of its own
        void show (float normalised);

        juce::RangedAudioParameter& parameter;
        juce::Component& control;
        juce::Slider* slider = nullptr;
        juce::Button* button = nullptr;
        juce::ComboBox* comboBox = nullptr;
        float shown = -1.0f; // the normalised value the control shows; -1 until the first poll
        bool inGesture = false;
        bool updating = false; // set while poll () moves the control, so its callbacks don't write back
    };
}
