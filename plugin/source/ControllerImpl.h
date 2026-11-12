#pragma once

// The editor's Controller (Controller.h) over the processor (P9). Every call comes from the message
// thread and returns at once: loads go to the kit service's thread, and snapshot () reads the
// engine's telemetry atomics. The state-only settings set here (the kit, the Standalone tempo, the
// MIDI-in mode and the phrase mode) tell the host the project changed
// (updateHostDisplay withNonParameterStateChanged); the content and samples folders are the person's,
// not the project's, and don't.

#include "Controller.h"

#include <vector>

namespace wubansia
{
    class PluginProcessor;
    class UserSettings;

    class ControllerImpl final : public Controller
    {
    public:
        explicit ControllerImpl (PluginProcessor& processor);

        std::vector<KitCard> kits() const override;
        juce::String currentKit() const override;
        void selectKit (const juce::String& id) override;
        ContentStatus status() const override;
        void setContentFolder (const juce::File& folder) override;

        Snapshot snapshot() override;
        std::vector<SectionInfo> sections() const override;
        TransportState transport() const override;
        void setTransport (bool playing, double bpm) override;
        bool preview() const override;
        void setPreview (bool on) override;

        engine::MidiInMode midiInMode() const override;
        engine::Lane midiInLane() const override;
        void setMidiInMode (engine::MidiInMode mode, engine::Lane lane) override;
        engine::PhraseMode phraseMode() const override;
        void setPhraseMode (engine::PhraseMode mode) override;

        juce::StringArray phraseFiles() const override;
        juce::File writeDragFile (const DragRequest& request) override;

        std::vector<SoundInfo> sounds (const juce::String& kit) const override;
        SoundExport exportSound (const SoundRef& sound) override;
        juce::File samplesFolder() const override;
        bool setSamplesFolder (const juce::File& folder) override;

        // Where writeDragFile () writes: dragout::cacheFolder () unless a test sets another.
        void setDragFolderForTesting (const juce::File& folder) { dragFolder = folder; }

        // Where exportSound () writes, whatever the person chose, for tests; juce::File () undoes it.
        void setSamplesFolderForTesting (const juce::File& folder) { samplesFolderOverride = folder; }

        // The settings file that keeps the samples folder: UserSettings::instance () unless a test sets another.
        void setUserSettingsForTesting (UserSettings& settings) { userSettings = &settings; }

    private:
        UserSettings& settingsFile() const;

        PluginProcessor& processor;
        juce::File dragFolder;
        juce::File samplesFolderOverride;
        UserSettings* userSettings = nullptr;
    };
}
