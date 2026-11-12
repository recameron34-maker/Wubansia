#include "ControllerImpl.h"

#include "ParameterIds.h"
#include "PluginProcessor.h"
#include "content/ContentLocator.h"
#include "dragout/DragFiles.h"

namespace wubansia
{
    namespace
    {
        const KitCatalog::Entry* entryOf (const PluginProcessor& processor, const juce::String& kit)
        {
            return processor.kitService().catalog().find (kit.toStdString());
        }

        bool on (juce::AudioProcessorValueTreeState& apvts, const char* id)
        {
            const auto* raw = apvts.getRawParameterValue (id);
            return raw != nullptr && raw->load (std::memory_order_relaxed) >= 0.5f;
        }
    }

    ControllerImpl::ControllerImpl (PluginProcessor& p) : processor (p) {}

    std::vector<KitCard> ControllerImpl::kits() const
    {
        std::vector<KitCard> cards;
        const auto& service = processor.kitService();
        for (const auto& entry : service.catalog().entries())
        {
            auto card = entry.card;
            card.content = service.kitStatus (entry.index);
            if (! entry.ok())
            {
                card.content.state = ContentState::failed;
                card.content.problem = problemText (juce::String::fromUTF8 (entry.error.data(), static_cast<int> (entry.error.size())));
            }
            cards.push_back (std::move (card));
        }
        return cards;
    }

    juce::String ControllerImpl::currentKit() const
    {
        return processor.settings().kit;
    }

    void ControllerImpl::selectKit (const juce::String& id)
    {
        const auto* entry = entryOf (processor, id);
        if (entry == nullptr)
            return; // not a kit this build carries

        // Already there, or on its way: nothing to do. A kit whose load failed loads again.
        const auto status = processor.kitService().status();
        if (processor.settings().kit == id && status.kit == id
            && (status.content.state == ContentState::ready || status.content.state == ContentState::loading))
            return;

        const auto version = entry->ok() ? entry->manifest.kit.contentVersion : 0; // the version this build carries
        processor.changeSettings ([&id, version] (StateOnly& s) {
            s.kit = id;
            s.kitContent = version;
        });
        {
            const std::lock_guard lock (processor.stateMutex);
            processor.loadQueued = true;
        }
        processor.service->request (id.toStdString(), version);
    }

    ContentStatus ControllerImpl::status() const
    {
        const auto kit = processor.settings().kit;
        const auto& service = processor.kitService();
        if (const auto loading = service.status(); loading.kit == kit)
            return loading.content;

        // Before the first load: what the service last saw, if anything.
        if (const auto* entry = entryOf (processor, kit))
            return service.kitStatus (entry->index);
        ContentStatus unknown;
        unknown.state = ContentState::failed;
        unknown.problem = "This build of Wubansia has no kit " + kit;
        return unknown;
    }

    void ControllerImpl::setContentFolder (const juce::File& folder)
    {
        processor.content->setUserFolder (folder);
        processor.service->contentChanged();
    }

    Snapshot ControllerImpl::snapshot()
    {
        // The telemetry is the engine's; reading it takes the hits and peaks (Telemetry's contract).
        auto& telemetry = processor.player.telemetry();
        engine::Controls controls;
        processor.raw.read (controls);

        Snapshot s;
        s.playing = processor.lastPlaying.load (std::memory_order_relaxed);
        s.bpm = transport().bpm;
        s.mode = controls.mode;
        s.songTicks = telemetry.songTicks.load (std::memory_order_relaxed);
        s.section = controls.mode == engine::PlayMode::song ? telemetry.section.load (std::memory_order_relaxed) : -1;
        s.laneHits = telemetry.laneHits.exchange (0, std::memory_order_relaxed);
        for (std::size_t lane = 0; lane < s.lanePeak.size(); ++lane)
            s.lanePeak[lane] = telemetry.lanePeak[lane].exchange (0.0f, std::memory_order_relaxed);
        s.phraseRecorded = telemetry.phraseRecorded.load (std::memory_order_relaxed);
        s.gainReductionDb = telemetry.gainReduction.load (std::memory_order_relaxed);
        s.voices = telemetry.voices.load (std::memory_order_relaxed);
        return s;
    }

    std::vector<SectionInfo> ControllerImpl::sections() const
    {
        const auto* entry = entryOf (processor, processor.settings().kit);
        if (entry == nullptr)
            return {};
        const bool vox = on (processor.apvts, ParameterIds::voxSynth);
        return entry->sections[vox ? 0 : 1];
    }

    TransportState ControllerImpl::transport() const
    {
        TransportState t;
        const auto settings = processor.settings();
        t.internal = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone || processor.lastInternal.load (std::memory_order_relaxed);
        t.playing = processor.lastPlaying.load (std::memory_order_relaxed);
        t.standaloneBpm = settings.standaloneBpm;

        // The tempo the last block ran at; where the engine chose it (no tempo given), the kit's.
        t.bpm = processor.lastBpm.load (std::memory_order_relaxed);
        if (t.bpm <= 0.0 && t.internal && settings.standaloneBpm > 0.0)
            t.bpm = settings.standaloneBpm;
        if (t.bpm <= 0.0)
        {
            const auto adopted = processor.telemetry().kitIndex.load (std::memory_order_relaxed);
            const auto& entries = processor.kitService().catalog().entries();
            const auto* entry = adopted >= 0 && static_cast<std::size_t> (adopted) < entries.size() ? &entries[static_cast<std::size_t> (adopted)]
                                                                                                   : entryOf (processor, settings.kit);
            t.bpm = entry != nullptr ? static_cast<double> (entry->info.bpm) : 0.0;
        }
        return t;
    }

    void ControllerImpl::setTransport (bool playing, double bpm)
    {
        processor.transport.setPlaying (playing);
        const auto clamped = InternalTransport::clampBpm (bpm);
        processor.changeSettings ([clamped] (StateOnly& s) { s.standaloneBpm = clamped; });
    }

    bool ControllerImpl::preview() const
    {
        return processor.transport.preview();
    }

    void ControllerImpl::setPreview (bool on)
    {
        processor.transport.setPreview (on);
    }

    engine::MidiInMode ControllerImpl::midiInMode() const
    {
        return processor.settings().midiIn;
    }

    engine::Lane ControllerImpl::midiInLane() const
    {
        return processor.settings().midiInLane;
    }

    void ControllerImpl::setMidiInMode (engine::MidiInMode mode, engine::Lane lane)
    {
        processor.changeSettings ([mode, lane] (StateOnly& s) {
            s.midiIn = mode;
            s.midiInLane = lane;
        });
    }

    engine::PhraseMode ControllerImpl::phraseMode() const
    {
        return processor.settings().phrases;
    }

    void ControllerImpl::setPhraseMode (engine::PhraseMode mode)
    {
        processor.changeSettings ([mode] (StateOnly& s) { s.phrases = mode; });
    }

    juce::StringArray ControllerImpl::phraseFiles() const
    {
        const auto* entry = entryOf (processor, processor.settings().kit);
        if (entry == nullptr)
            return {};
        return dragout::phraseFiles (entry->id, entry->midiFiles);
    }

    bool ControllerImpl::dragDrumsOnOneTrack() const
    {
        return false; // not yet
    }

    void ControllerImpl::setDragDrumsOnOneTrack (bool on)
    {
        juce::ignoreUnused (on); // not yet
    }

    UserSettings& ControllerImpl::dragSettingsFile() const
    {
        return dragSettings != nullptr ? *dragSettings : UserSettings::instance();
    }

    juce::File ControllerImpl::writeDragFile (const DragRequest& request)
    {
        const auto* entry = entryOf (processor, processor.settings().kit);
        if (entry == nullptr || ! entry->ok())
            return {};

        engine::Controls controls;
        processor.raw.read (controls);
        dragout::DragState state;
        state.mode = controls.mode;
        state.variant = controls.variant;
        for (std::size_t lane = 0; lane < ParameterIds::laneMute.size(); ++lane)
            if (on (processor.apvts, ParameterIds::laneMute[lane]))
                state.mutedLanes |= 1u << lane;

        const auto folder = dragFolder != juce::File() ? dragFolder : dragout::cacheFolder();
        const auto file = dragout::writeDragFile (entry->info, entry->midiFiles, state, request, folder);
        return file.ok() ? file.value : juce::File();
    }
}
