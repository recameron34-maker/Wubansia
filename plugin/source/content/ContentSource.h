#pragma once

// Where the plugin's kits come from (P9). A kit is its two manifests and its MIDI, which the plugin
// carries in its binary (EmbeddedContent), and its samples, which live in a content folder outside it
// (ContentLocator, ADR 0022). The processor reaches both through one ContentSourceFactory:
// InstalledContent in the plugin, the synthetic kit's in the tests
// (plugin/tests/helpers/synthetic_content.h), so every test runs without samples.

#include "Controller.h"

#include "kit/manifest.h"
#include "kit/sample_source.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace wubansia
{
    // One MIDI file the plugin carries: its file name, "FUT01_Loop_Vox_4bars.mid", and its bytes.
    struct EmbeddedMidi
    {
        std::string_view name;
        std::span<const std::uint8_t> bytes;
    };

    // One kit as the plugin carries it. Every view stays valid for the factory's life.
    struct KitSource
    {
        std::string_view id; // "FUT01"
        std::string_view kitJson; // <KIT>_kit.json (P2)
        std::string_view mixJson; // <KIT>_mix.json (P3); empty when the kit has none
        std::array<std::span<const std::uint8_t>, 4> smfs; // by engine::timelineIndex: Loop vox, Loop no-vox, Song vox, Song no-vox
        std::span<const EmbeddedMidi> midiFiles; // every MIDI file of the kit's, for drag-out
    };

    class ContentSourceFactory
    {
    public:
        virtual ~ContentSourceFactory() = default;

        // Every kit, in the kit picker's order. Constant for the factory's life; any thread.
        virtual std::span<const KitSource> kits() const = 0;

        // Where kit's samples are: ready with the folder, or the first problem met, with every folder
        // searched. The service thread: it reads the disk. cancel, if given, is the service's: once raised,
        // the check stops within one file and reports a problem the service drops ("cancelled: ...").
        virtual ContentStatus locate (const engine::KitManifest& kit, const std::atomic<bool>* cancel = nullptr) = 0;

        // What reads the samples of a kit locate () found ready. The service thread. cancel, if given, stops
        // a read within one chunk (P4's FileSampleSource), so a load gives way even on slow storage.
        virtual std::unique_ptr<engine::SampleSource> open (const engine::KitManifest& kit, const ContentStatus& found,
            const std::atomic<bool>* cancel = nullptr) = 0;

        // The folder chosen with "Choose content folder…": kept per user, never in project state. The
        // setter runs on the message thread and may write a settings file.
        virtual juce::File userFolder() const = 0;
        virtual void setUserFolder (const juce::File& folder) = 0;

        // Changes whenever the folders searched change, in any instance in the process: the service then
        // checks every kit again. Any thread.
        virtual std::uint64_t foldersGeneration() const noexcept = 0;
    };
}
