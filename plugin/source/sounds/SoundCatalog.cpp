#include "sounds/SoundCatalog.h"

#include "kit/sample_source.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace wubansia::sounds
{
    namespace
    {
        juce::String text (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // A kit.json path's file name, without its folder and ".wav": "Rave Stab v2".
        std::string_view stemOf (std::string_view path)
        {
            const auto slash = path.find_last_of ('/');
            auto name = slash == std::string_view::npos ? path : path.substr (slash + 1);
            if (name.ends_with (".wav"))
                name.remove_suffix (4);
            return name;
        }

        bool isFile (const engine::KitManifest& kit, std::int32_t index) noexcept
        {
            return index >= 0 && static_cast<std::size_t> (index) < kit.files.size();
        }

        // The instrument's files: zones low to high, each zone's layers soft to hard, each layer's round robins;
        // each file once, in the order first met.
        std::vector<int> filesOf (const engine::KitManifest& kit, const engine::InstrumentEntry& in)
        {
            std::vector<int> files;
            for (const auto& zone : in.zones)
                for (const auto& layer : zone.layers)
                    for (const auto rr : layer.rr)
                        if (isFile (kit, rr) && std::find (files.begin(), files.end(), rr) == files.end())
                            files.push_back (rr);
            return files;
        }

        // Pitched: some zone spans more than one key.
        bool isPitched (const engine::InstrumentEntry& in)
        {
            return std::any_of (in.zones.begin(), in.zones.end(), [] (const engine::ZoneEntry& z) { return z.lo < z.hi; });
        }

        // The file the kit's loops play: the top layer's first round robin of the zone on the tonic (pitched) or the
        // first zone (one key). -1 when the instrument has none.
        int defaultFileOf (const engine::InstrumentEntry& in, int tonic)
        {
            if (in.zones.empty())
                return -1;
            const auto* zone = &in.zones.front();
            if (isPitched (in))
            {
                const auto onTonic = std::find_if (in.zones.begin(), in.zones.end(), [tonic] (const engine::ZoneEntry& z) {
                    return tonic >= 0 && ((z.root % 12) + 12) % 12 == tonic;
                });
                zone = onTonic != in.zones.end() ? &*onTonic : &in.zones[in.zones.size() / 2];
            }
            if (zone->layers.empty() || zone->layers.back().rr.empty())
                return -1;
            return zone->layers.back().rr.front();
        }

        std::vector<SoundFile> soundFiles (const engine::KitManifest& kit, const std::vector<int>& files)
        {
            std::vector<SoundFile> out;
            out.reserve (files.size());
            for (const auto index : files)
            {
                const auto& f = kit.files[static_cast<std::size_t> (index)];
                out.push_back ({ index, text (stemOf (f.path)), static_cast<double> (f.frames) / static_cast<double> (engine::kContentSampleRate), f.channels });
            }
            return out;
        }

        // Points the sound's default at file, when the sound has it.
        void setDefault (SoundInfo& sound, int file)
        {
            const auto it = std::find_if (sound.files.begin(), sound.files.end(), [file] (const SoundFile& f) { return f.file == file; });
            if (it != sound.files.end())
                sound.defaultFile = static_cast<int> (it - sound.files.begin());
        }

        // The file's whole bars at bpm, when it is within 48 frames (1 ms) of them and at least one; else 0.
        int barsOf (std::int64_t frames, std::int32_t bpm)
        {
            if (bpm <= 0)
                return 0;
            const double framesPerBar = static_cast<double> (engine::kContentSampleRate) * 240.0 / static_cast<double> (bpm);
            const double bars = std::floor (static_cast<double> (frames) / framesPerBar + 0.5);
            if (bars < 1.0 || std::abs (static_cast<double> (frames) - bars * framesPerBar) > 48.0)
                return 0;
            return static_cast<int> (bars);
        }

        // A sound being collected: its info, its file set (sorted, to compare), and whether its zones are pitched.
        struct Collected
        {
            SoundInfo info;
            std::vector<int> set;
            bool pitched = false;
        };

        std::vector<int> sorted (std::vector<int> files)
        {
            std::sort (files.begin(), files.end());
            return files;
        }
    }

    std::vector<SoundInfo> soundsOf (const engine::KitManifest& kit)
    {
        const auto tonic = tonicOf (kit.kit.key);
        const auto kitId = text (kit.kit.id);
        std::vector<Collected> collected;
        const auto sameSet = [&collected] (const std::vector<int>& set) {
            return std::find_if (collected.begin(), collected.end(), [&set] (const Collected& c) { return c.set == set; });
        };

        // 1. The instruments, in manifest order; one sound per file set.
        for (const auto& in : kit.instruments)
        {
            const auto files = filesOf (kit, in);
            if (files.empty())
                continue;
            auto set = sorted (files);
            const auto category = categoryOf (in.lane);
            const auto pitched = isPitched (in);

            if (const auto same = sameSet (set); same != collected.end())
            {
                // The same sound: the earlier keeps its id, name and place, and takes the higher category.
                same->info.category = std::min (same->info.category, category);
                if (pitched && ! same->pitched)
                {
                    same->pitched = true;
                    setDefault (same->info, defaultFileOf (in, tonic));
                }
                continue;
            }

            Collected c;
            c.set = std::move (set);
            c.pitched = pitched;
            c.info.id = kitId + "/" + text (in.id);
            auto name = text (in.name);
            if (name.startsWith ("Accent ") && name.length() > 7)
                name = name.substring (7);
            c.info.name = name;
            c.info.category = category;
            c.info.files = soundFiles (kit, files);
            setDefault (c.info, defaultFileOf (in, tonic));
            c.info.endAligned = in.align == engine::Align::end;
            collected.push_back (std::move (c));
        }

        // 3. Then each phrase stem that isn't a sound already is a Loop.
        for (const auto& phrase : kit.phrases)
            for (const auto stem : { phrase.stemA, phrase.stemB })
            {
                if (! isFile (kit, stem) || sameSet ({ stem }) != collected.end())
                    continue;
                const auto& file = kit.files[static_cast<std::size_t> (stem)];
                Collected c;
                c.set = { stem };
                c.info.id = kitId + "/" + text (file.path);
                c.info.name = text (stemOf (file.path));
                c.info.category = SoundCategory::loops;
                c.info.files = soundFiles (kit, { stem });
                c.info.loopBars = phrase.cycle16ths / 16;
                collected.push_back (std::move (c));
            }

        // 7. The default file's numbers.
        std::vector<SoundInfo> sounds;
        sounds.reserve (collected.size());
        for (auto& c : collected)
        {
            const auto& file = kit.files[static_cast<std::size_t> (c.info.files[static_cast<std::size_t> (c.info.defaultFile)].file)];
            c.info.bars = barsOf (file.frames, kit.kit.bpm);
            c.info.leadInMs = static_cast<int> (std::floor (static_cast<double> (file.prerollFrames) / 48.0 + 0.5)); // 48 frames a millisecond
            sounds.push_back (std::move (c.info));
        }
        return sounds;
    }

    SoundCategory categoryOf (engine::Lane lane) noexcept
    {
        switch (lane)
        {
            case engine::Lane::accents: return SoundCategory::accents;
            case engine::Lane::fx: return SoundCategory::fx;
            case engine::Lane::vox: return SoundCategory::vox;
            case engine::Lane::lead:
            case engine::Lane::chords: return SoundCategory::synths;
            case engine::Lane::bass: return SoundCategory::bass;
            case engine::Lane::kick:
            case engine::Lane::snare:
            case engine::Lane::hats:
            case engine::Lane::perc: break;
        }
        return SoundCategory::drums;
    }

    juce::String categoryName (SoundCategory category)
    {
        switch (category)
        {
            case SoundCategory::accents: return "Accents";
            case SoundCategory::fx: return "FX";
            case SoundCategory::vox: return "Vox";
            case SoundCategory::synths: return "Synths";
            case SoundCategory::bass: return "Bass";
            case SoundCategory::drums: return "Drums";
            case SoundCategory::loops: return "Loops";
        }
        return {};
    }

    int tonicOf (std::string_view key) noexcept
    {
        const auto note = key.substr (0, key.find (' '));
        if (note.empty() || note.size() > 2 || note[0] < 'A' || note[0] > 'G')
            return -1;
        constexpr int naturals[] { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        int pitch = naturals[note[0] - 'A'];
        if (note.size() == 2)
        {
            if (note[1] == '#')
                ++pitch;
            else if (note[1] == 'b')
                --pitch;
            else
                return -1;
        }
        return (pitch + 12) % 12;
    }
}
