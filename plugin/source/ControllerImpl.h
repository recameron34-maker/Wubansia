#pragma once

// The editor's Controller (Controller.h) over the processor (P9). Every call comes from the message
// thread and returns at once: loads go to the kit service's thread, and snapshot () reads the
// engine's telemetry atomics. The state-only settings set here (the kit, the Standalone tempo, the
// MIDI-in mode and the phrase mode) tell the host the project changed
// (updateHostDisplay withNonParameterStateChanged); the content and samples folders and the drag
// strip's drum option (drag_drums_one_track) and For my synths (drag_for_synths) are the person's,
// kept in settings.xml, and don't. The drag strip's Part is the instance's own (ADR 0029): a member,
// All when the instance starts, in neither settings.xml nor the project.

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

        bool dragDrumsOnOneTrack() const override;
        void setDragDrumsOnOneTrack (bool on) override;
        bool dragForSynths() const override;
        void setDragForSynths (bool on) override;
        DragPart dragPart() const override;
        void setDragPart (DragPart part) override;
        juce::File writeDragFile (const DragRequest& request) override;

        std::vector<SoundInfo> sounds (const juce::String& kit) const override;
        std::uint32_t audition (const SoundRef& sound) override;
        void stopAudition() override;
        AuditionState auditionState() const override;
        SoundExport exportSound (const SoundRef& sound) override;
        juce::File samplesFolder() const override;
        bool setSamplesFolder (const juce::File& folder) override;
        std::uint32_t exportDrumKit() override;
        DrumKitStatus drumKitStatus() const override;

        std::vector<PhraseInfo> phrases() const override;
        std::uint64_t phrasesGeneration() const override;
        bool morePhrases (const juce::String& kind) override;
        std::uint32_t auditionPhrase (const juce::String& id) override;
        void stopPhraseAudition() override;
        PhraseAuditionState phraseAuditionState() const override;
        void setPhraseKept (const juce::String& id, bool kept) override;

        std::shared_ptr<const PatternView> pattern (engine::PlayMode mode, engine::Variant variant) const override;
        void setScopeOn (bool on) override;
        int readScope (float* dest, int maxSamples) override;

        // Where writeDragFile () writes: dragout::cacheFolder () unless a test sets another.
        void setDragFolderForTesting (const juce::File& folder) { dragFolder = folder; }

        // Where exportSound () writes, whatever the person chose, for tests; juce::File () undoes it.
        void setSamplesFolderForTesting (const juce::File& folder) { samplesFolderOverride = folder; }

        // The settings file that keeps the samples folder and the drum option: UserSettings::instance () unless a test
        // sets another.
        void setUserSettingsForTesting (UserSettings& settings); // the phrase thread's too

    private:
        UserSettings& settingsFile() const;

        // The content folders a samples folder must stay out of: the places the locator looks (ADR 0022), and the
        // root of every kit folder the service has searched.
        std::vector<juce::File> contentFolders() const;

        PluginProcessor& processor;
        juce::File dragFolder;
        juce::File samplesFolderOverride;
        UserSettings* userSettings = nullptr;
        DragPart dragPartChoice = DragPart::all; // what Loop and Song bring (ADR 0029), message thread only

        // The kit phrasesGeneration () last saw, and its count of kit changes (message thread, as every call).
        mutable juce::String phrasesKit;
        mutable std::uint64_t phrasesListed = 0;

        // The last pattern () view (ADR 0032), for its kit, mode and variant (message thread).
        mutable std::shared_ptr<const PatternView> patternCache;
    };
}
