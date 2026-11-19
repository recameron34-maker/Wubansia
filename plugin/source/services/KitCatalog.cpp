#include "services/KitCatalog.h"

#include "util/sha256.h"

#include "sounds/SoundCatalog.h"

#include "kit/channel_map.h"
#include "midi/reader.h"
#include "sched/timeline_builder.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace wubansia
{
    namespace
    {
        juce::String text (const std::string& s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        KitCard cardOf (const KitCatalog::Entry& e)
        {
            KitCard card;
            card.id = text (e.id);
            const auto& meta = e.manifest.kit;
            card.title = text (meta.title);
            card.direction = text (meta.direction);
            card.key = text (meta.key);
            card.camelot = text (meta.camelot);
            card.bpm = meta.bpm;
            card.contentVersion = meta.contentVersion;
            for (std::size_t lane = 0; lane < card.laneLabels.size(); ++lane)
                card.laneLabels[lane] = text (meta.lanes[lane]);
            for (const auto& phrase : e.manifest.phrases)
                card.phraseLanes |= engine::laneBit (phrase.lane);
            for (std::size_t i = 0; i < card.abA.size(); ++i)
            {
                card.abA[i] = text (meta.ab[i].a);
                card.abB[i] = text (meta.ab[i].b);
            }
            card.content.state = ContentState::loading; // until the service has looked
            return card;
        }
    }

    engine::KitInfo kitInfoOf (const engine::KitManifest& m, int index)
    {
        engine::KitInfo info;
        info.id = m.kit.id;
        info.title = m.kit.title;
        info.direction = m.kit.direction;
        info.key = m.kit.key;
        info.camelot = m.kit.camelot;
        info.bpm = m.kit.bpm;
        info.swingPm = m.kit.swingPm;
        info.contentVersion = m.kit.contentVersion;
        info.index = index;
        info.laneLabels = m.kit.lanes;
        info.ab = m.kit.ab;
        for (std::size_t i = 0; i < m.kit.lanes.size(); ++i)
            if (! m.kit.lanes[i].empty())
                info.lanes |= 1u << i;
        for (std::size_t i = 0; i < m.kit.ab.size(); ++i)
            if (m.kit.ab[i].exists())
                info.abSwitches |= 1u << i;
        for (const auto& e : m.extras)
            if (e.ch >= 1 && e.ch <= 16)
                info.extras.push_back ({ static_cast<std::uint8_t> (e.ch - 1), e.name, e.lane });

        // The tracks whose sample plays a whole chord (ADR 0027, chords as chords): Chords, from the instrument the
        // keymap puts on its channel (the A side of a switch), then each extra whose instrument's zones are voiced, in
        // channel order. A track is named with the zones that carry a voicing, and only when one does.
        const auto chordTrack = [&] (std::string_view name, int channel) {
            const auto entry = std::find_if (m.keymap.begin(), m.keymap.end(), [channel] (const engine::KeymapEntry& k) { return k.ch == channel; });
            if (entry == m.keymap.end() || entry->a < 0 || static_cast<std::size_t> (entry->a) >= m.instruments.size())
                return;
            engine::KitChordTrack track;
            track.name = std::string (name);
            for (const auto& z : m.instruments[static_cast<std::size_t> (entry->a)].zones)
                if (! z.voicing.empty())
                {
                    engine::KitChordZone zone;
                    zone.lo = static_cast<std::uint8_t> (z.lo);
                    zone.hi = static_cast<std::uint8_t> (z.hi);
                    for (const auto interval : z.voicing)
                        zone.intervals.push_back (static_cast<std::int8_t> (interval));
                    track.zones.push_back (std::move (zone));
                }
            if (! track.zones.empty())
                info.chordTracks.push_back (std::move (track));
        };
        for (const auto& t : engine::kTracks)
            if (t.role == engine::ChannelRole::chords)
                chordTrack (t.name, engine::channelOf (engine::ChannelRole::chords));
        for (const auto& e : m.extras)
            chordTrack (e.name, e.ch);
        return info;
    }

    std::vector<SectionInfo> sectionsOf (std::span<const std::uint8_t> smf)
    {
        const auto file = midi::readSmf (smf);
        return file.ok() ? sectionsOf (file.value) : std::vector<SectionInfo> {};
    }

    std::vector<SectionInfo> sectionsOf (const midi::File& file)
    {
        std::vector<SectionInfo> sections;
        if (file.ppq == 0)
            return sections;

        std::int64_t end = 0;
        for (const auto& track : file.tracks)
            end = std::max (end, track.endTick);
        const std::int64_t bar = std::int64_t { file.ppq } * 4; // 4/4, the only metre the content has

        const auto markers = midi::markersOf (file);
        for (std::size_t i = 0; i < markers.size(); ++i)
        {
            const auto start = markers[i].tick;
            const auto next = i + 1 < markers.size() ? markers[i + 1].tick : std::max (end, start);
            SectionInfo s;
            s.name = juce::String::fromUTF8 (markers[i].text.data(), static_cast<int> (markers[i].text.size()));
            s.startBar = static_cast<int> (start / bar) + 1;
            s.bars = static_cast<int> ((next - start + bar - 1) / bar);
            sections.push_back (s);
        }
        return sections;
    }

    util::Result<std::array<engine::Timeline, 4>> buildTimelines (const KitCatalog::Entry& kit, const std::atomic<bool>* cancel)
    {
        using R = util::Result<std::array<engine::Timeline, 4>>;
        constexpr std::array<const char*, 4> names { "Loop vox", "Loop no-vox", "Song vox", "Song no-vox" };
        std::array<engine::Timeline, 4> timelines;
        if (kit.source == nullptr || ! kit.ok())
            return R::failure ("failed: " + kit.id + " has no manifests to build timelines from");
        for (std::size_t i = 0; i < timelines.size(); ++i)
        {
            if (cancel != nullptr && cancel->load (std::memory_order_acquire))
                return R::failure ("cancelled: the load was cancelled while building " + kit.id + "'s timelines");
            engine::TimelineOptions options;
            options.loopTicks = i < 2 ? engine::kLoopLength / engine::kMilliticksPerTick : 0; // the 4-bar loops wrap
            auto built = engine::buildTimeline (kit.source->smfs[i], kit.manifest, kit.mix, options);
            if (! built.ok())
                return R::failure ("failed: " + kit.id + "'s " + names[i] + " MIDI: " + built.error);
            timelines[i] = std::move (built.value);
        }
        return R::success (std::move (timelines));
    }

    namespace
    {
        // The Phrases drawer's spec and library (ADR 0031), parsed once: the library must be the kit's, chosen for this
        // very spec (its "spec" is the spec file's SHA-256). Without them the kit lists only its own phrases.
        void readPhrases (KitCatalog::Entry& e, const KitSource& source)
        {
            if (source.phraseSpec.empty() || source.phraseLibrary.empty())
            {
                e.phraseError = "the plugin carries no phrase spec and library for " + e.id;
                return;
            }
            auto spec = gen::parsePhraseSpec (source.phraseSpec, e.id + "_phrases.json");
            if (! spec.ok())
            {
                e.phraseError = spec.error;
                return;
            }
            auto library = gen::parsePhraseLibrary (source.phraseLibrary, e.id + "_phrase_library.json");
            if (! library.ok())
                e.phraseError = library.error;
            else if (spec.value.kitId != e.id || library.value.kitId != e.id)
                e.phraseError = e.id + "_phrases.json and " + e.id + "_phrase_library.json are for kit " + spec.value.kitId + " and " + library.value.kitId;
            else if (library.value.specSha256
                     != util::sha256Hex (std::span<const std::uint8_t> (reinterpret_cast<const std::uint8_t*> (source.phraseSpec.data()), source.phraseSpec.size())))
                e.phraseError = e.id + "_phrase_library.json was chosen for another " + e.id + "_phrases.json: run groovecore phrases --library";
            if (! e.phraseError.empty())
                return;
            for (const auto& chord : spec.value.chords)
                e.card.phraseChords.add (juce::String::fromUTF8 (chord.name.data(), static_cast<int> (chord.name.size())));
            e.phraseSpec = std::move (spec.value);
            e.phraseRows = std::move (library.value.rows);
        }
    }

    KitCatalog::KitCatalog (std::span<const KitSource> kits)
    {
        list.reserve (kits.size());
        for (const auto& source : kits)
        {
            Entry e;
            e.source = &source;
            e.id = std::string (source.id);
            e.index = static_cast<int> (list.size());

            auto kit = engine::parseKitManifest (source.kitJson);
            if (! kit.ok())
                e.error = kit.error;
            else if (kit.value.kit.id != e.id)
                e.error = "manifest: the plugin carries kit " + e.id + " with a kit.json for " + kit.value.kit.id;
            else
            {
                e.manifest = std::move (kit.value);
                if (source.mixJson.empty())
                    e.error = "manifest: the plugin carries no mix.json for " + e.id;
                else if (auto mix = engine::parseMixManifest (source.mixJson, e.manifest); ! mix.ok())
                    e.error = mix.error;
                else
                    e.mix = std::move (mix.value);
            }

            if (e.ok())
            {
                e.info = kitInfoOf (e.manifest, e.index);
                e.sounds = sounds::soundsOf (e.manifest);
            }
            else
            {
                e.info.id = e.id;
                e.info.index = e.index;
            }
            e.card = cardOf (e);
            e.card.id = juce::String::fromUTF8 (e.id.data(), static_cast<int> (e.id.size()));
            // Each of the four files read once: the lanes it holds (ADR 0029) and, for the Songs, their sections.
            for (std::size_t i = 0; i < source.smfs.size(); ++i)
            {
                const auto parsed = midi::readSmf (source.smfs[i]);
                if (! parsed.ok())
                    continue;
                if (e.ok())
                    e.card.fileLanes[i] = dragout::trackLanes (e.info, parsed.value);
                if (i == static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::song, engine::Variant::vox)))
                    e.sections[0] = sectionsOf (parsed.value);
                else if (i == static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::song, engine::Variant::novox)))
                    e.sections[1] = sectionsOf (parsed.value);
            }
            for (const auto& m : source.midiFiles)
                e.midiFiles.push_back ({ m.name, m.bytes });
            readPhrases (e, source);
            list.push_back (std::move (e));
        }
    }

    const KitCatalog::Entry* KitCatalog::find (std::string_view id) const noexcept
    {
        for (const auto& e : list)
            if (e.id == id)
                return &e;
        return nullptr;
    }
}
