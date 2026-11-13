#include "ControllerImpl.h"

#include "ParameterIds.h"
#include "PluginProcessor.h"
#include "content/ContentLocator.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "sounds/SampleExport.h"

#include <algorithm>
#include <utility>

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

        // Where a kit's samples stand: the current kit's load (status ()), or the service's last check of another
        // kit, which its idle survey fills.
        ContentStatus contentOf (const Controller& controller, const PluginProcessor& processor, const KitCatalog::Entry& entry)
        {
            if (juce::String::fromUTF8 (entry.id.data(), static_cast<int> (entry.id.size())) == processor.settings().kit)
                return controller.status();
            return processor.kitService().kitStatus (entry.index);
        }

        // A sound's kit entry and its ready content folder, for a copy or an audition; or why there's none, for people.
        struct SoundSource
        {
            const KitCatalog::Entry* entry = nullptr; // nullptr: see problem
            ContentStatus found;
            juce::String problem;
        };

        SoundSource sourceOf (const Controller& controller, const PluginProcessor& processor, const SoundRef& sound)
        {
            SoundSource source;
            const auto* entry = entryOf (processor, sound.kit);
            if (entry == nullptr || ! entry->ok())
            {
                source.problem = "this build of Wubansia has no kit " + sound.kit;
                return source;
            }
            if (sound.file < 0 || static_cast<std::size_t> (sound.file) >= entry->manifest.files.size())
            {
                source.problem = sound.kit + " lists no file " + juce::String (sound.file);
                return source;
            }

            // Only from a folder the service found ready: kit.json's files, checked.
            source.found = contentOf (controller, processor, *entry);
            if (source.found.state != ContentState::ready || source.found.folder == juce::File())
            {
                source.problem = source.found.state == ContentState::loading ? sound.kit + "'s samples aren't ready yet"
                                 : source.found.problem.isNotEmpty()        ? problemText (source.found.problem)
                                                                            : sound.kit + "'s samples aren't installed";
                return source;
            }
            source.entry = entry;
            return source;
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

        // A kit chosen stops the Samples drawer's audition (ADR 0024).
        processor.sounds->stop();

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

    UserSettings& ControllerImpl::settingsFile() const
    {
        return userSettings != nullptr ? *userSettings : UserSettings::instance();
    }

    std::vector<SoundInfo> ControllerImpl::sounds (const juce::String& kit) const
    {
        const auto* entry = entryOf (processor, kit);
        return entry != nullptr ? entry->sounds : std::vector<SoundInfo> {};
    }

    std::uint32_t ControllerImpl::audition (const SoundRef& sound)
    {
        const auto source = sourceOf (*this, processor, sound);
        if (source.entry == nullptr)
        {
            processor.sounds->refuse (source.problem); // what plays stops, and the drawer says why
            return 0;
        }
        SoundService::Request request;
        request.kit = sound.kit;
        request.contentVersion = source.entry->info.contentVersion;
        request.found = source.found;
        request.file = source.entry->manifest.files[static_cast<std::size_t> (sound.file)];
        return processor.sounds->audition (std::move (request));
    }

    void ControllerImpl::stopAudition()
    {
        processor.sounds->stop();
    }

    AuditionState ControllerImpl::auditionState() const
    {
        return processor.sounds->status();
    }

    SoundExport ControllerImpl::exportSound (const SoundRef& sound)
    {
        SoundExport result;
        const auto source = sourceOf (*this, processor, sound);
        if (source.entry == nullptr)
        {
            result.problem = source.problem;
            result.sourceProblem = true; // the kit, its file or its samples: not the samples folder
            return result;
        }

        const auto& file = source.entry->manifest.files[static_cast<std::size_t> (sound.file)];
        const auto copy = sounds::exportCopy (*processor.content, source.found, source.entry->info, file, samplesFolder());
        result.file = copy.file;
        result.problem = juce::String::fromUTF8 (copy.problem.data(), static_cast<int> (copy.problem.size()));
        result.sourceProblem = copy.sourceProblem;
        return result;
    }

    juce::File ControllerImpl::samplesFolder() const
    {
        if (samplesFolderOverride != juce::File())
            return samplesFolderOverride;
        const auto chosen = settingsFile().samplesFolder();
        return chosen != juce::File() ? chosen : sounds::defaultSamplesFolder();
    }

    bool ControllerImpl::setSamplesFolder (const juce::File& folder)
    {
        if (folder == juce::File())
        {
            settingsFile().setSamplesFolder ({}); // back to the default
            return true;
        }

        // Never a content folder (installers expect only their files there, and a DAW writes beside what it
        // takes), nor Wubansia's own folders (ADR 0024): the app data's "Wubansia Audio", which holds Content and
        // settings.xml and which INSTALL.md's Uninstall deletes, and the caches' "Wubansia Audio" around the drag
        // cache, which is trimmed and purgeable.
        const auto inside = [&folder] (const juce::File& place) { return place != juce::File() && (folder == place || folder.isAChildOf (place)); };
        const auto folders = contentFolders();
        const auto ownAppData = ContentLocator::appDataFolder().getParentDirectory();
        const auto ownCaches = dragout::cacheFolder().getParentDirectory().getParentDirectory();
        if (std::any_of (folders.begin(), folders.end(), inside) || inside (ownAppData) || inside (ownCaches) || inside (dragFolder))
            return false;
        if (folder.createDirectory().failed())
            return false;

        settingsFile().setSamplesFolder (folder); // kept even if settings.xml can't be written, until the process ends
        return true;
    }

    std::vector<juce::File> ControllerImpl::contentFolders() const
    {
        std::vector<juce::File> folders;
        const auto add = [&folders] (const juce::File& f) {
            if (f != juce::File() && std::find (folders.begin(), folders.end(), f) == folders.end())
                folders.push_back (f);
        };

        // Where the locator looks, even before its first check: the chosen folder, $WUBANSIA_CONTENT_DIR, the
        // installers' folder and Debug's checkout.
        add (processor.content->userFolder());
        const auto places = ContentLocator::systemPlaces();
        add (places.environment);
        add (places.platformDefault);
        add (places.debug);

        // The root that holds every kit folder a kit's last check searched: <root>/<kit>/v<N> or <root>/<kit>.
        for (const auto& entry : processor.kitService().catalog().entries())
        {
            const auto kitFolder = ContentLocator::kitFolderName (entry.id);
            for (const auto& path : contentOf (*this, processor, entry).searched)
            {
                if (! juce::File::isAbsolutePath (path))
                    continue;
                const juce::File searched (path);
                if (searched.getFileName().equalsIgnoreCase (kitFolder))
                    add (searched.getParentDirectory());
                else if (searched.getParentDirectory().getFileName().equalsIgnoreCase (kitFolder))
                    add (searched.getParentDirectory().getParentDirectory());
                else
                    add (searched);
            }
        }
        return folders;
    }
}
