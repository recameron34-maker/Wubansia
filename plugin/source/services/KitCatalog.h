#pragma once

// The kits a ContentSourceFactory carries, parsed once (P9): each kit's two manifests (P4's parsers),
// its KitInfo as buildKit fills it, the card the editor shows, its Song sections, its MIDI files as
// drag-out takes them, and its sounds as the Samples drawer lists them (ADR 0024). The kit service
// loads from it on its thread; the controller reads it on the message thread; it never changes after
// it is made.

#include "Controller.h"
#include "content/ContentSource.h"
#include "dragout/DragFiles.h"

#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "sched/timeline.h"
#include "util/result.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia
{
    class KitCatalog
    {
    public:
        struct Entry
        {
            const KitSource* source = nullptr;
            std::string id; // "FUT01"
            int index = 0; // the kit's place in the list: KitInfo::index and Telemetry::kitIndex
            std::string error; // why the manifests don't parse; "" when they do
            engine::KitManifest manifest;
            engine::MixConfig mix;
            engine::KitInfo info; // as buildKit fills it, index included
            KitCard card; // its content status left as it starts (missing): the service knows it
            std::array<std::vector<SectionInfo>, 2> sections; // the Song's, by engine::Variant
            std::vector<dragout::MidiFile> midiFiles; // the kit's MIDI files, as drag-out takes them
            std::vector<SoundInfo> sounds; // sounds::soundsOf (manifest); empty when the manifests don't parse

            bool ok() const noexcept { return error.empty(); }
        };

        explicit KitCatalog (std::span<const KitSource> kits);

        const std::vector<Entry>& entries() const noexcept { return list; }

        // The kit with this id, or nullptr.
        const Entry* find (std::string_view id) const noexcept;

    private:
        std::vector<Entry> list;
    };

    // KitInfo from kit.json, as buildKit lays it out, with the kit's place in the list and its chord tracks (the tracks
    // whose sample plays a whole chord, for MIDI drag-out's For my synths: ADR 0027).
    engine::KitInfo kitInfoOf (const engine::KitManifest& manifest, int index);

    // A Song's sections from its SMF's markers (FF 06): name, 1-based start bar and length in bars, the
    // last one running to the file's end. Empty for a file that doesn't read.
    std::vector<SectionInfo> sectionsOf (std::span<const std::uint8_t> smf);

    // The kit's four timelines (P7's buildTimeline: Loop vox, Loop no-vox, Song vox, Song no-vox; the
    // loops wrap at 4 bars), or the first error. cancel, when raised, stops it between files with a
    // "cancelled: ..." error.
    util::Result<std::array<engine::Timeline, 4>> buildTimelines (const KitCatalog::Entry& kit, const std::atomic<bool>* cancel = nullptr);
}
