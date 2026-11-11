#include "kit/kit_builder.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        using KitResult = util::Result<std::shared_ptr<const Kit>>;

        std::string channelsText (std::int32_t n) { return std::to_string (n) + (n == 1 ? " channel" : " channels"); }

        std::string at (std::string_view what, std::size_t i) { return std::string (what) + "[" + std::to_string (i) + "]"; }

        // The tables' index types (kit/kit.h) bound what one Kit holds.
        constexpr std::size_t kMaxFiles = std::numeric_limits<std::uint16_t>::max(); // rr and stems are uint16
        constexpr std::size_t kMaxInstruments = std::numeric_limits<std::int16_t>::max(); // KeyTarget, layeredWith
        constexpr std::size_t kMaxTable = std::numeric_limits<std::uint16_t>::max(); // firstZone, firstLayer, firstRr, firstPan
        constexpr std::size_t kMaxSlices = std::numeric_limits<std::int16_t>::max(); // Note::slice
        constexpr std::size_t kMaxPhrases = std::numeric_limits<std::int16_t>::max(); // Note::phrase
        constexpr std::size_t kMaxChokeGroups = std::numeric_limits<std::uint8_t>::max();

        // Lays the manifests out in kit's tables, exactly as kit/kit.h describes (engine/testing's layOutKit is the
        // reference), everything but the audio. Gives "" or the "manifest: ..." problem.
        std::string layOut (const KitManifest& m, const MixConfig& mix, Kit& kit)
        {
            const auto refuse = [] (std::string message) { return "manifest: " + std::move (message); };

            if (mix.kit != m.kit.id)
                return refuse ("mix.json is kit " + mix.kit + "'s, not " + m.kit.id + "'s");
            if (mix.contentVersion != m.kit.contentVersion)
                return refuse ("mix.json is content version " + std::to_string (mix.contentVersion) + ", kit.json " + std::to_string (m.kit.contentVersion));
            if (mix.channels.size() > static_cast<std::size_t> (kMaxMixChannels))
                return refuse ("mix.json has " + std::to_string (mix.channels.size()) + " mix channels; a Kit holds at most " + std::to_string (kMaxMixChannels));
            if (m.files.size() > kMaxFiles)
                return refuse ("kit.json lists " + std::to_string (m.files.size()) + " files; a Kit holds at most " + std::to_string (kMaxFiles));
            if (m.instruments.size() > kMaxInstruments)
                return refuse ("kit.json lists " + std::to_string (m.instruments.size()) + " instruments; a Kit holds at most " + std::to_string (kMaxInstruments));
            if (m.phrases.size() > kMaxPhrases)
                return refuse ("kit.json lists " + std::to_string (m.phrases.size()) + " phrases; a Kit holds at most " + std::to_string (kMaxPhrases));

            auto& info = kit.info;
            info.id = m.kit.id;
            info.title = m.kit.title;
            info.direction = m.kit.direction;
            info.key = m.kit.key;
            info.camelot = m.kit.camelot;
            info.bpm = m.kit.bpm;
            info.swingPm = m.kit.swingPm;
            info.contentVersion = m.kit.contentVersion;
            info.laneLabels = m.kit.lanes;
            info.ab = m.kit.ab;
            for (std::size_t i = 0; i < m.kit.lanes.size(); ++i)
                if (! m.kit.lanes[i].empty())
                    info.lanes |= 1u << i;
            for (std::size_t i = 0; i < m.kit.ab.size(); ++i)
                if (m.kit.ab[i].exists())
                    info.abSwitches |= 1u << i;
            for (std::size_t i = 0; i < m.extras.size(); ++i)
            {
                const auto& e = m.extras[i];
                if (e.ch < 1 || e.ch > 16)
                    return refuse ("kit.json: " + at ("extras", i) + ".ch: channel " + std::to_string (e.ch) + " doesn't exist");
                info.extras.push_back ({ static_cast<std::uint8_t> (e.ch - 1), e.name, e.lane });
            }

            kit.files.reserve (m.files.size());
            for (const auto& f : m.files)
                kit.files.push_back ({ f.path, f.frames, f.channels, f.loopStart, f.loopEnd, f.prerollFrames });

            const auto validFile = [&] (std::int64_t index) { return index >= 0 && static_cast<std::size_t> (index) < m.files.size(); };
            const auto instrumentCount = static_cast<std::int32_t> (m.instruments.size());

            // Instruments, then their zones, layers and round robins, each table appended in order.
            std::vector<std::string> chokeGroups;
            kit.instruments.reserve (m.instruments.size());
            for (std::size_t i = 0; i < m.instruments.size(); ++i)
            {
                const auto& e = m.instruments[i];
                const auto where = "kit.json: " + at ("instruments", i);
                const int mixChannel = mix.channelIndex (e.mixChannel);
                if (mixChannel < 0)
                    return refuse (where + ".mix_channel: mix.json has no channel \"" + e.mixChannel + "\"");
                if (mix.channels[static_cast<std::size_t> (mixChannel)].lane != static_cast<std::uint8_t> (e.lane))
                    return refuse (where + " plays on lane " + std::string (laneId (e.lane)) + ", but mix.json's channel \"" + e.mixChannel + "\" is on lane "
                                   + std::string (laneId (static_cast<Lane> (mix.channels[static_cast<std::size_t> (mixChannel)].lane))));
                if (e.layeredWith < -1 || e.layeredWith >= instrumentCount)
                    return refuse (where + ".layered_with: instrument " + std::to_string (e.layeredWith) + " doesn't exist");
                if (e.zones.size() > std::numeric_limits<std::uint8_t>::max() || e.panCyclePm.size() > std::numeric_limits<std::uint8_t>::max())
                    return refuse (where + ": more zones or pans than an instrument holds");

                Instrument in;
                in.id = e.id;
                in.name = e.name;
                in.lane = e.lane;
                in.mixChannel = static_cast<std::uint8_t> (mixChannel);
                in.mode = e.mode;
                in.align = e.align;
                in.attackUs = e.attackUs;
                in.releaseUs = e.releaseUs;
                in.gainMdb = e.gainMdb;
                in.startFrames = e.startFrames;
                in.velocityLaw = e.velocityLaw;
                in.interp = e.interp;
                in.mono = e.mono;
                in.glide = e.glide;
                in.restrikeContinues = e.restrikeContinues;
                in.choke.role = e.choke.role;
                in.choke.fadeUs = e.choke.fadeUs;
                in.choke.curve = e.choke.curve;
                if (! e.choke.group.empty())
                {
                    // Groups numbered 1, 2... in order of first use.
                    auto found = std::find (chokeGroups.begin(), chokeGroups.end(), e.choke.group);
                    if (found == chokeGroups.end())
                    {
                        if (chokeGroups.size() == kMaxChokeGroups)
                            return refuse (where + ".choke.group: more than " + std::to_string (kMaxChokeGroups) + " choke groups");
                        found = chokeGroups.insert (chokeGroups.end(), e.choke.group);
                    }
                    in.choke.group = static_cast<std::uint8_t> (1 + (found - chokeGroups.begin()));
                }
                in.duckKey = e.duckKey;
                in.firstPan = static_cast<std::uint16_t> (kit.pans.size());
                in.panCount = static_cast<std::uint8_t> (e.panCyclePm.size());
                for (const auto pan : e.panCyclePm)
                    kit.pans.push_back (static_cast<std::int16_t> (std::clamp (pan, -1000, 1000)));
                in.rrBlock16ths = e.rrBlock16ths;
                in.layeredWith = static_cast<std::int16_t> (e.layeredWith);

                in.firstZone = static_cast<std::uint16_t> (kit.zones.size());
                in.zoneCount = static_cast<std::uint8_t> (e.zones.size());
                for (std::size_t zi = 0; zi < e.zones.size(); ++zi)
                {
                    const auto& z = e.zones[zi];
                    const auto zoneWhere = where + "." + at ("zones", zi);
                    if (z.root < 0 || z.root > 127 || z.lo < 0 || z.hi > 127 || z.lo > z.hi || z.layers.size() > std::numeric_limits<std::uint8_t>::max())
                        return refuse (zoneWhere + ": a key out of 0-127, or too many layers");
                    Zone zone;
                    zone.root = static_cast<std::uint8_t> (z.root);
                    zone.lo = static_cast<std::uint8_t> (z.lo);
                    zone.hi = static_cast<std::uint8_t> (z.hi);
                    zone.firstLayer = static_cast<std::uint16_t> (kit.layers.size());
                    zone.layerCount = static_cast<std::uint8_t> (z.layers.size());
                    for (std::size_t li = 0; li < z.layers.size(); ++li)
                    {
                        const auto& l = z.layers[li];
                        if (l.vel < 1 || l.vel > 127 || l.rr.size() > std::numeric_limits<std::uint8_t>::max())
                            return refuse (zoneWhere + "." + at ("layers", li) + ": a velocity out of 1-127, or too many round robins");
                        kit.layers.push_back ({ static_cast<std::uint8_t> (l.vel), static_cast<std::uint16_t> (kit.rr.size()), static_cast<std::uint8_t> (l.rr.size()) });
                        for (std::size_t ri = 0; ri < l.rr.size(); ++ri)
                        {
                            if (! validFile (l.rr[ri]))
                                return refuse (zoneWhere + "." + at ("layers", li) + "." + at ("rr", ri) + ": file " + std::to_string (l.rr[ri]) + " doesn't exist ("
                                               + std::to_string (m.files.size()) + " files)");
                            kit.rr.push_back (static_cast<std::uint16_t> (l.rr[ri]));
                        }
                    }
                    kit.zones.push_back (zone);
                    if (kit.zones.size() > kMaxTable || kit.layers.size() > kMaxTable || kit.rr.size() > kMaxTable || kit.pans.size() > kMaxTable)
                        return refuse ("kit.json: more zones, layers, round robins or pans than a Kit holds (" + std::to_string (kMaxTable) + " each)");
                }
                kit.instruments.push_back (std::move (in));
            }

            // What each (channel, key) plays, and where each channel's CCs reach.
            for (std::size_t i = 0; i < m.keymap.size(); ++i)
            {
                const auto& k = m.keymap[i];
                if (k.ch < 1 || k.ch > 16 || k.lo < 0 || k.hi > 127 || k.lo > k.hi || k.a < 0 || k.a >= instrumentCount || k.b < -1 || k.b >= instrumentCount
                    || k.ab < -1 || k.ab >= kAbSwitches)
                    return refuse ("kit.json: " + at ("keymap", i) + " names a channel, key, instrument or switch that doesn't exist");
                const auto ch = static_cast<std::size_t> (k.ch - 1);
                const KeyTarget target { static_cast<std::int16_t> (k.a), static_cast<std::int16_t> (k.b), static_cast<std::int8_t> (k.b >= 0 ? k.ab : -1) };
                for (int key = k.lo; key <= k.hi; ++key)
                    kit.keys[ch][static_cast<std::size_t> (key)] = target;
                for (const auto t : { k.a, k.b })
                {
                    if (t < 0)
                        continue;
                    const auto& in = kit.instruments[static_cast<std::size_t> (t)];
                    kit.ccMixChannels[ch] |= 1u << in.mixChannel;
                    if (in.layeredWith >= 0)
                        kit.ccMixChannels[ch] |= 1u << kit.instruments[static_cast<std::size_t> (in.layeredWith)].mixChannel;
                }
            }

            // Phrases and their slices.
            for (std::size_t i = 0; i < m.phrases.size(); ++i)
            {
                const auto& e = m.phrases[i];
                const auto where = "kit.json: " + at ("phrases", i);
                const int mixChannel = mix.channelIndex (e.mixChannel);
                if (mixChannel < 0)
                    return refuse (where + ".mix_channel: mix.json has no channel \"" + e.mixChannel + "\"");
                if (mix.channels[static_cast<std::size_t> (mixChannel)].lane != static_cast<std::uint8_t> (e.lane))
                    return refuse (where + " plays on lane " + std::string (laneId (e.lane)) + ", but mix.json's channel \"" + e.mixChannel + "\" doesn't");
                if (! validFile (e.stemA) || (e.stemB != -1 && ! validFile (e.stemB)))
                    return refuse (where + ".stems: a stem file that doesn't exist (" + std::to_string (m.files.size()) + " files)");
                if (e.ch < 1 || e.ch > 16 || e.ab < -1 || e.ab >= kAbSwitches)
                    return refuse (where + ": a channel or switch that doesn't exist");
                Phrase p;
                p.lane = e.lane;
                p.channel = static_cast<std::uint8_t> (e.ch - 1);
                p.ab = static_cast<std::int8_t> (e.stemB >= 0 ? e.ab : -1);
                p.mixChannel = static_cast<std::uint8_t> (mixChannel);
                p.gainMdb = e.gainMdb;
                p.throwMode = e.throwMode;
                p.stem = { static_cast<std::uint16_t> (e.stemA), static_cast<std::uint16_t> (e.stemB >= 0 ? e.stemB : e.stemA) };
                p.cycle16ths = e.cycle16ths;
                p.tolerancePm = e.tolerancePm;
                p.fadeUs = e.fadeUs;
                p.fadeInUs = e.fadeInUs;
                p.firstSlice = static_cast<std::uint16_t> (kit.slices.size());
                p.sliceCount = static_cast<std::uint16_t> (e.slices.size());
                // The voice reads a slice modulo its stem's length: it starts inside the shorter stem and is at most
                // that long, so it wraps at most once (docs/plugin/manifest.md 2.6).
                auto frames = m.files[static_cast<std::size_t> (e.stemA)].frames;
                if (e.stemB >= 0)
                    frames = std::min (frames, m.files[static_cast<std::size_t> (e.stemB)].frames);
                for (std::size_t si = 0; si < e.slices.size(); ++si)
                {
                    const auto& s = e.slices[si];
                    if (s.holdCount < 0 || s.holdCount > kMaxHolds || s.key < 0 || s.key > 127)
                        return refuse (where + "." + at ("slices", si) + ".holds: more than " + std::to_string (kMaxHolds) + " holds, or a key out of 0-127");
                    if (s.start < 0 || s.start >= frames || s.end <= s.start || s.end > s.start + frames || s.fade < s.start || s.fade > s.end)
                        return refuse (where + "." + at ("slices", si) + ": frames " + std::to_string (s.start) + " to " + std::to_string (s.end) + " (fade "
                                       + std::to_string (s.fade) + ") don't fit its " + std::to_string (frames)
                                       + "-frame stem: a slice starts inside it and is at most that long");
                    PhraseSlice slice; // holds past holdCount stay 0
                    slice.step = s.step;
                    slice.key = static_cast<std::uint8_t> (s.key);
                    slice.start = s.start;
                    slice.end = s.end;
                    slice.fade = s.fade;
                    slice.lead = s.lead;
                    slice.holdCount = static_cast<std::uint8_t> (s.holdCount);
                    for (std::size_t h = 0; h < slice.holdCount; ++h)
                        slice.holds[h] = static_cast<std::uint8_t> (std::clamp (s.holds[h], 0, 127));
                    kit.slices.push_back (slice);
                }
                if (kit.slices.size() > kMaxSlices)
                    return refuse ("kit.json: more phrase slices than a Kit holds (" + std::to_string (kMaxSlices) + ")");
                kit.ccMixChannels[p.channel] |= 1u << p.mixChannel;
                kit.phrases.push_back (p);
            }
            return {};
        }
    }

    util::Result<std::shared_ptr<const Kit>> buildKit (const KitManifest& manifest, MixConfig mix, SampleSource& source,
        std::array<Timeline, 4>&& timelines, const std::atomic<bool>& cancel, FunctionRef<void (int pct)> progress, std::int32_t kitIndex)
    {
        const auto files = manifest.files.size();
        const auto cancelled = [files] (std::size_t read) {
            return KitResult::failure ("cancelled: the load was cancelled after " + std::to_string (read) + " of " + std::to_string (files) + " files");
        };
        try
        {
            if (cancel.load (std::memory_order_acquire))
                return cancelled (0);

            auto kit = std::make_unique<Kit>();
            if (auto problem = layOut (manifest, mix, *kit); ! problem.empty())
                return KitResult::failure (std::move (problem));

            // The audio, file by file, weighing progress by each file's samples.
            std::int64_t total = 0;
            for (const auto& f : manifest.files)
                total += f.frames * f.channels;
            total = std::max<std::int64_t> (total, 1);
            int reported = -1;
            const auto report = [&] (int pct) {
                if (pct > reported)
                {
                    reported = pct;
                    progress (pct);
                }
            };
            report (0);
            std::int64_t done = 0;
            kit->audio.reserve (files);
            for (std::size_t i = 0; i < files; ++i)
            {
                if (cancel.load (std::memory_order_acquire))
                    return cancelled (i);
                const auto& f = manifest.files[i];
                auto read = source.read (f.path);
                if (! read.ok())
                    return KitResult::failure (std::move (read.error));
                if (auto mismatch = fileMismatch (f, read.value.frames, read.value.channels, read.value.sampleRate); ! mismatch.empty())
                    return KitResult::failure (std::move (mismatch));
                if (read.value.audio.size() != static_cast<std::size_t> (f.frames * f.channels))
                    return KitResult::failure ("failed: " + f.path + ": the sample source gave " + std::to_string (read.value.audio.size()) + " samples for "
                                               + std::to_string (f.frames) + " frames of " + channelsText (f.channels));
                kit->audio.push_back (std::move (read.value));
                done += f.frames * f.channels;
                report (static_cast<int> (done * 100 / total));
            }
            report (100);

            kit->mix = std::move (mix);
            kit->timelines = std::move (timelines);
            kit->info.index = kitIndex;
            return KitResult::success (std::shared_ptr<const Kit> (std::move (kit)));
        }
        catch (const std::bad_alloc&)
        {
            return KitResult::failure ("failed: out of memory while building kit " + manifest.kit.id);
        }
        catch (const std::exception& e)
        {
            return KitResult::failure ("failed: building kit " + manifest.kit.id + ": " + e.what());
        }
    }

    LoadError loadErrorOf (std::string_view error) noexcept
    {
        if (error.empty())
            return LoadError::none;
        const auto code = error.substr (0, error.find (':'));
        if (code.size() == error.size())
            return LoadError::failed;
        if (code == "missing")
            return LoadError::missing;
        if (code == "lfs_pointer")
            return LoadError::lfsPointers;
        if (code == "mismatch" || code == "rate")
            return LoadError::mismatch;
        if (code == "cancelled")
            return LoadError::cancelled;
        return LoadError::failed;
    }

    std::string fileMismatch (const FileEntry& entry, std::int64_t frames, std::int32_t channels, std::int32_t sampleRate)
    {
        if (sampleRate != kContentSampleRate)
            return "rate: " + entry.path + ": " + std::to_string (sampleRate) + " Hz; the content is " + std::to_string (kContentSampleRate) + " Hz";
        if (frames != entry.frames || channels != entry.channels)
            return "mismatch: " + entry.path + ": " + std::to_string (frames) + " frames and " + channelsText (channels) + "; kit.json lists "
                 + std::to_string (entry.frames) + " frames and " + channelsText (entry.channels);
        return {};
    }

    util::Result<int> checkKitFiles (const KitManifest& manifest, const FileSampleSource& source, const std::atomic<bool>* cancel)
    {
        return checkKitFiles (manifest, [&source] (std::string_view path) { return source.probe (path); }, cancel);
    }

    util::Result<int> checkKitFiles (const KitManifest& manifest, FunctionRef<util::Result<WavInfo> (std::string_view path)> probe,
        const std::atomic<bool>* cancel)
    {
        for (std::size_t i = 0; i < manifest.files.size(); ++i)
        {
            if (cancel != nullptr && cancel->load (std::memory_order_acquire))
                return util::Result<int>::failure ("cancelled: the check was cancelled after " + std::to_string (i) + " of "
                                                   + std::to_string (manifest.files.size()) + " files");
            const auto& f = manifest.files[i];
            const auto info = probe (f.path);
            if (! info.ok())
                return util::Result<int>::failure (info.error);
            if (auto mismatch = fileMismatch (f, info.value.frames, info.value.channels, info.value.sampleRate); ! mismatch.empty())
                return util::Result<int>::failure (std::move (mismatch));
        }
        return util::Result<int>::success (static_cast<int> (manifest.files.size()));
    }
}
