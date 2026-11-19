#include "ControllerImpl.h"

#include "ParameterIds.h"
#include "PluginProcessor.h"
#include "content/ContentLocator.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "services/PatternView.h"
#include "editor/EditorModel.h"
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

        // A kit's content, found ready by the service, or why it isn't, for people: the Samples drawer's words and the Drum
        // kit button's (ADR 0030).
        struct ReadySource
        {
            ContentStatus found;
            juce::String problem; // "" when found is ready
        };

        ReadySource readySource (const Controller& controller, const PluginProcessor& processor, const KitCatalog::Entry& entry, const juce::String& kit)
        {
            ReadySource ready;
            ready.found = contentOf (controller, processor, entry);
            if (ready.found.state != ContentState::ready || ready.found.folder == juce::File())
                ready.problem = ready.found.state == ContentState::loading ? kit + "'s samples aren't ready yet"
                                : ready.found.problem.isNotEmpty()         ? problemText (ready.found.problem)
                                                                           : kit + "'s samples aren't installed";
            return ready;
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
            auto ready = readySource (controller, processor, *entry, sound.kit);
            source.found = std::move (ready.found);
            if (ready.problem.isNotEmpty())
            {
                source.problem = ready.problem;
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

        // A kit chosen stops the Samples drawer's audition (ADR 0024) and the Phrases drawer's (ADR 0031).
        processor.sounds->stop();
        processor.phraseThread->stop();
        processor.stopPhrasePreview();

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
        processor.previewForPhrase.store (false, std::memory_order_release); // the person's from now on
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

    bool ControllerImpl::dragDrumsOnOneTrack() const
    {
        return settingsFile().dragDrumsOnOneTrack();
    }

    void ControllerImpl::setDragDrumsOnOneTrack (bool on)
    {
        // The person's, not the project's: no changeSettings, so the host isn't told the project changed.
        settingsFile().setDragDrumsOnOneTrack (on);
    }

    bool ControllerImpl::dragForSynths() const
    {
        return settingsFile().dragForSynths();
    }

    void ControllerImpl::setDragForSynths (bool on)
    {
        // The person's, not the project's, as the drum option is.
        settingsFile().setDragForSynths (on);
    }

    DragPart ControllerImpl::dragPart() const
    {
        return dragPartChoice;
    }

    void ControllerImpl::setDragPart (DragPart part)
    {
        dragPartChoice = part; // the instance's own: no settings file, no changeSettings
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
        state.drumsOnOneTrack = settingsFile().dragDrumsOnOneTrack();
        state.forSynths = settingsFile().dragForSynths();
        state.part = dragPartChoice;

        const auto folder = dragFolder != juce::File() ? dragFolder : dragout::cacheFolder();
        if (request.what == DragWhat::generatedPhrase)
        {
            // The phrase thread's file for it, made here: the OS drag starts inside the mouse event. Dragged, it isn't New.
            const auto phrase = processor.phraseThread->file (processor.settings().kit, request.phraseFile);
            if (! phrase.ok())
                return {};
            const auto file = dragout::writeGeneratedPhrase (entry->info, phrase.value.name, phrase.value.kindWord, phrase.value.bytes, state, folder);
            if (file.ok())
                processor.phraseThread->markSeen (processor.settings().kit, request.phraseFile);
            return file.ok() ? file.value : juce::File();
        }
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
        return sounds::soundExportOf (sounds::exportCopy (*processor.content, source.found, source.entry->info, file, samplesFolder()));
    }

    std::uint32_t ControllerImpl::exportDrumKit()
    {
        // The current kit, its ready content, the samples folder and the kick switch as the controls read now. Never
        // changeSettings: a drum kit isn't the project's.
        const auto kit = processor.settings().kit;
        auto& drumKits = processor.drumKitService();
        const auto* entry = entryOf (processor, kit);
        if (entry == nullptr || ! entry->ok())
        {
            drumKits.refuse (kit, "this build of Wubansia has no kit " + kit);
            return 0;
        }
        auto ready = readySource (*this, processor, *entry, kit);
        if (ready.problem.isNotEmpty())
        {
            drumKits.refuse (kit, ready.problem);
            return 0;
        }

        engine::Controls controls;
        processor.raw.read (controls);
        DrumKitService::Request request;
        request.kit = kit;
        request.manifest = entry->manifest;
        request.mix = entry->mix;
        request.found = std::move (ready.found);
        request.samplesFolder = samplesFolder();
        request.kickSide = controls.ab[static_cast<std::size_t> (engine::AbSwitch::kick)];
        return drumKits.request (std::move (request));
    }

    DrumKitStatus ControllerImpl::drumKitStatus() const
    {
        return processor.drumKitService().status();
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

    std::vector<PhraseInfo> ControllerImpl::phrases() const
    {
        // The phrase thread makes the current kit's list (the first call starts it); until it has, none.
        const auto kit = processor.settings().kit;
        processor.phraseThread->show (kit);
        const auto list = processor.phraseThread->list();
        return list != nullptr && list->kit == kit ? list->phrases : std::vector<PhraseInfo> {};
    }

    std::uint64_t ControllerImpl::phrasesGeneration() const
    {
        // phrases () is the current kit's: it moves on when the kit does (a project's included), and with every list the
        // phrase thread makes. Asking starts nothing: the editor asks on every tick, drawer open or not.
        const auto kit = processor.settings().kit;
        if (kit != phrasesKit)
        {
            phrasesKit = kit;
            ++phrasesListed;
        }
        return phrasesListed + processor.phraseThread->generation();
    }

    bool ControllerImpl::morePhrases (const juce::String& kind)
    {
        return processor.phraseThread->more (processor.settings().kit, kind);
    }

    std::uint32_t ControllerImpl::auditionPhrase (const juce::String& id)
    {
        const auto kit = processor.settings().kit;
        const auto ticket = processor.phraseThread->audition (kit, id);
        if (ticket == 0)
            return 0;
        processor.phraseThread->markSeen (kit, id);

        // Nothing plays (the last block was stopped): Preview plays the kit for it, the Standalone's clock or a stopped host's.
        if (! processor.lastPlaying.load (std::memory_order_relaxed) && ! processor.transport.preview())
        {
            processor.transport.setPreview (true);
            processor.previewForPhrase.store (true, std::memory_order_release);
        }
        return ticket;
    }

    void ControllerImpl::stopPhraseAudition()
    {
        processor.phraseThread->stop();
        processor.stopPhrasePreview();
    }

    PhraseAuditionState ControllerImpl::phraseAuditionState() const
    {
        auto s = processor.phraseThread->status();
        s.startedTransport = processor.previewForPhrase.load (std::memory_order_acquire) && processor.transport.preview();
        return s;
    }

    void ControllerImpl::setPhraseKept (const juce::String& id, bool kept)
    {
        processor.phraseThread->setKept (processor.settings().kit, id, kept);
    }

    void ControllerImpl::setUserSettingsForTesting (UserSettings& settings)
    {
        userSettings = &settings;
        processor.phraseThread->setUserSettings (&settings);
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

    std::shared_ptr<const PatternView> ControllerImpl::pattern (engine::PlayMode mode, engine::Variant variant) const
    {
        // The looks' notes (ADR 0032): parsed from the embedded MIDI once per kit, mode and variant, then the same object.
        const auto kit = currentKit();
        const auto wanted = mode == engine::PlayMode::song ? engine::PlayMode::song : engine::PlayMode::loop;
        if (patternCache != nullptr && patternCache->kit == kit && patternCache->mode == wanted && patternCache->variant == variant)
            return patternCache;
        const auto* entry = entryOf (processor, kit);
        if (entry == nullptr)
            return nullptr;
        patternCache = patternOf (*entry, wanted, variant);
        return patternCache;
    }

    void ControllerImpl::setScopeOn (bool on) { processor.scope.setOn (on); }

    int ControllerImpl::readScope (float* dest, int maxSamples) { return processor.scope.pull (dest, maxSamples); }
}
