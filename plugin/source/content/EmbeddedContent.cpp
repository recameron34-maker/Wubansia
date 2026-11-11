#include "content/EmbeddedContent.h"

#include "dragout/DragFiles.h"

#include "WubansiaContentData.h" // juce_add_binary_data's header (plugin/CMakeLists.txt)

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace wubansia
{
    namespace
    {
        struct EmbeddedFile
        {
            std::string_view name; // the original file name, static
            std::span<const std::uint8_t> bytes;
        };

        struct EmbeddedKit
        {
            std::string_view id;
            std::string_view kitJson;
            std::string_view mixJson;
            std::vector<EmbeddedMidi> midi;
            std::array<std::span<const std::uint8_t>, 4> smfs {};
        };

        struct Index
        {
            std::vector<EmbeddedKit> kits;
            std::vector<EmbeddedContent::KitId> ids;
            std::vector<KitSource> sources;
            std::size_t bytes = 0;
        };

        std::string_view textOf (std::span<const std::uint8_t> bytes)
        {
            return { reinterpret_cast<const char*> (bytes.data()), bytes.size() };
        }

        const EmbeddedFile* fileNamed (const std::vector<EmbeddedFile>& files, std::string_view name)
        {
            const auto at = std::lower_bound (files.begin(), files.end(), name, [] (const EmbeddedFile& f, std::string_view n) { return f.name < n; });
            return at != files.end() && at->name == name ? &*at : nullptr;
        }

        Index build()
        {
            Index index;

            std::vector<EmbeddedFile> files;
            for (int i = 0; i < WubansiaContentData::namedResourceListSize; ++i)
            {
                int size = 0;
                const char* data = WubansiaContentData::getNamedResource (WubansiaContentData::namedResourceList[i], size);
                const char* name = WubansiaContentData::originalFilenames[i];
                if (data == nullptr || name == nullptr || size < 0)
                    continue;
                files.push_back ({ name, { reinterpret_cast<const std::uint8_t*> (data), static_cast<std::size_t> (size) } });
                index.bytes += static_cast<std::size_t> (size);
            }
            std::sort (files.begin(), files.end(), [] (const EmbeddedFile& a, const EmbeddedFile& b) { return a.name < b.name; });

            // A kit for every <KIT>_kit.json; its id points into the file's static name.
            constexpr std::string_view kitSuffix = "_kit.json";
            for (const auto& f : files)
                if (f.name.size() > kitSuffix.size() && f.name.ends_with (kitSuffix))
                {
                    EmbeddedKit kit;
                    kit.id = f.name.substr (0, f.name.size() - kitSuffix.size());
                    kit.kitJson = textOf (f.bytes);
                    index.kits.push_back (std::move (kit));
                }

            for (auto& kit : index.kits)
            {
                const std::string id (kit.id);
                if (const auto* mix = fileNamed (files, id + "_mix.json"))
                    kit.mixJson = textOf (mix->bytes);

                // Every <KIT>_*.mid, by name (files is sorted).
                const auto prefix = id + "_";
                for (const auto& f : files)
                    if (f.name.starts_with (prefix) && f.name.ends_with (".mid"))
                        kit.midi.push_back ({ f.name, f.bytes });

                // The four the player plays, by engine::timelineIndex.
                const std::array<std::string, 4> names { dragout::loopFileName (id, engine::Variant::vox), dragout::loopFileName (id, engine::Variant::novox),
                    dragout::songFileName (id, engine::Variant::vox), dragout::songFileName (id, engine::Variant::novox) };
                for (std::size_t i = 0; i < names.size(); ++i)
                    if (const auto* smf = fileNamed (files, names[i]))
                        kit.smfs[i] = smf->bytes;
            }

            // Views into index.kits, which never changes again.
            for (const auto& kit : index.kits)
            {
                index.ids.push_back (kit.id);
                KitSource source;
                source.id = kit.id;
                source.kitJson = kit.kitJson;
                source.mixJson = kit.mixJson;
                source.smfs = kit.smfs;
                source.midiFiles = kit.midi;
                index.sources.push_back (source);
            }
            return index;
        }

        const Index& index()
        {
            static const Index built = build();
            return built;
        }

        const EmbeddedKit* find (EmbeddedContent::KitId id)
        {
            for (const auto& kit : index().kits)
                if (kit.id == id)
                    return &kit;
            return nullptr;
        }
    }

    std::span<const EmbeddedContent::KitId> EmbeddedContent::kits()
    {
        return index().ids;
    }

    std::string_view EmbeddedContent::kitManifest (KitId kit)
    {
        const auto* k = find (kit);
        return k != nullptr ? k->kitJson : std::string_view {};
    }

    std::string_view EmbeddedContent::mixManifest (KitId kit)
    {
        const auto* k = find (kit);
        return k != nullptr ? k->mixJson : std::string_view {};
    }

    std::span<const std::uint8_t> EmbeddedContent::timeline (KitId kit, engine::PlayMode mode, engine::Variant variant)
    {
        const auto* k = find (kit);
        const auto i = engine::timelineIndex (mode, variant);
        return k != nullptr && i >= 0 ? k->smfs[static_cast<std::size_t> (i)] : std::span<const std::uint8_t> {};
    }

    std::span<const EmbeddedMidi> EmbeddedContent::midiFiles (KitId kit)
    {
        const auto* k = find (kit);
        return k != nullptr ? std::span<const EmbeddedMidi> (k->midi) : std::span<const EmbeddedMidi> {};
    }

    std::span<const KitSource> EmbeddedContent::sources()
    {
        return index().sources;
    }

    std::size_t EmbeddedContent::totalBytes()
    {
        return index().bytes;
    }
}
