#pragma once

// The kits the plugin carries in its binary (P9; ADR 0017): each kit's two manifests,
// content/plugin/<kit>/<KIT>_kit.json and <KIT>_mix.json, and every kit-prefixed MIDI file under
// content/midi/<kit>/ (the Loop and Song files, the parts and the phrases; the extended mixes'
// region files stay out). plugin/CMakeLists.txt embeds them with juce_add_binary_data, as the
// WubansiaContent target, about 1.4 MB. So the kit list, the sections, drag-out and the editor work
// before any sample is installed, and in CI.
//
// Kits are listed by id in byte order (FUT01 ... FUT04): the kit picker's order. Everything here is
// built once, on first use, from static data, and never changes. Any thread but the audio thread
// (the first call allocates).

#include "content/ContentSource.h"

#include "engine.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wubansia
{
    class EmbeddedContent
    {
    public:
        using KitId = std::string_view;

        // Every kit, "FUT01" ... "FUT04".
        static std::span<const KitId> kits();

        // A kit's manifests; "" for a kit the plugin doesn't carry (or a manifest it lacks).
        static std::string_view kitManifest (KitId kit);
        static std::string_view mixManifest (KitId kit);

        // The SMF a mode and variant play: <KIT>_Loop_Vox_4bars.mid, <KIT>_Loop_NoVox_4bars.mid,
        // <KIT>_ExtendedMix_Vox_224bars.mid, <KIT>_ExtendedMix_NoVox_224bars.mid. Empty for
        // PlayMode::off and for an unknown kit.
        static std::span<const std::uint8_t> timeline (KitId kit, engine::PlayMode mode, engine::Variant variant);

        // Every MIDI file of the kit's, by file name in byte order; empty for an unknown kit.
        static std::span<const EmbeddedMidi> midiFiles (KitId kit);

        // Every kit as a KitSource, in kits ()'s order: what InstalledContent hands the processor.
        static std::span<const KitSource> sources();

        // How many bytes are embedded, all files together.
        static std::size_t totalBytes();
    };
}
