#pragma once

// Where the processor tests' kits come from: a ContentSourceFactory over P0's synthetic kit SYN01
// (engine/testing/synthetic_kit.h), so every test plays a real Kit through the real engine without a
// sample on disk. It can carry SYN01 under several ids (SYN01, SYN02... each with its own kit.json and
// mix.json), report any content problem, check its folder or read slowly, or hold every read until a test
// lets it go,
// and it counts what the service asks of it. readFile () serves the synthetic WAVs' bytes from memory
// (they match the manifest's SHA-256), and can serve one file with a byte flipped (alteredPath). Every kit
// serves the same bytes, so readFile () also records the folder it was asked to read from (foldersRead ()):
// a test checks that a kit's file came from that kit's own folder.
//
//     auto content = std::make_unique<wubansia::test::SyntheticContent> ();
//     auto& syn = *content;
//     plugin.setContentForTesting (std::move (content));
//
// Kits are shared through P4's process-wide KitCache by id: a test that needs a fresh build of a kit
// another live processor holds uses another id.

#include "content/ContentSource.h"

#include "kit/kit_builder.h"
#include "kit/sample_source.h"
#include "testing/manifest_json.h"
#include "testing/synthetic_kit.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wubansia::test
{
    // What a SyntheticContent's locate () reports for every kit.
    enum class SyntheticSamples {
        ready,
        missing, // no folder holds it
        lfsPointers,
        mismatch,
        failed
    };

    struct SyntheticProbeLog
    {
        std::atomic<int> started { 0 }; // files the slow check started on
        std::atomic<std::chrono::steady_clock::rep> lastEnd { 0 }; // when the last file's check ended (steady_clock ticks)

        std::chrono::steady_clock::time_point lastEndTime() const
        {
            return std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (lastEnd.load()));
        }
    };

    struct SyntheticOptions
    {
        std::vector<std::string> ids { "SYN01" };
        SyntheticSamples samples = SyntheticSamples::ready;
        std::chrono::milliseconds readDelay { 0 }; // per file read
        // Per file locate () checks, through P4's checkKitFiles with the service's cancel flag, as a slow content
        // folder would; 0: locate () reports at once.
        std::chrono::milliseconds probeDelay { 0 };
        // Where the slow check also records what it did, for a test that reads it after the processor (which owns
        // the content) is gone.
        std::shared_ptr<SyntheticProbeLog> probeLog;
        // A kit.json path whose readFile () bytes have one byte flipped, as a damaged or edited install would:
        // the same size, another SHA-256. "" for none.
        std::string alteredPath;
    };

    class SyntheticContent final : public ContentSourceFactory
    {
    public:
        using Samples = SyntheticSamples;
        using Options = SyntheticOptions;

        SyntheticContent() : SyntheticContent (Options {}) {}

        explicit SyntheticContent (Options options)
            : delay (options.readDelay), probeDelay (options.probeDelay), probeLog (options.probeLog), alteredPath (options.alteredPath), samples (options.samples)
        {
            const auto& syn = engine::testing::syntheticContent();
            for (const auto& id : options.ids)
            {
                auto& kit = stored.emplace_back();
                kit.id = id;
                auto manifest = syn.kit;
                manifest.kit.id = id;
                if (id != engine::testing::kSyntheticKitId)
                    manifest.kit.title = "Synthetic " + id.substr (id.size() > 2 ? id.size() - 2 : 0);
                auto mix = syn.mix;
                mix.kit = id;
                kit.kitJson = engine::testing::kitManifestJson (manifest);
                kit.mixJson = engine::testing::mixManifestJson (mix);
                kit.names = { id + "_Loop_Vox_4bars.mid", id + "_Loop_NoVox_4bars.mid", id + "_ExtendedMix_Vox_224bars.mid",
                    id + "_ExtendedMix_NoVox_224bars.mid", id + "_Phrase_Lead_2bars.mid" };
            }
            // Views into stored, which never moves again.
            for (auto& kit : stored)
            {
                for (std::size_t i = 0; i < kit.names.size(); ++i)
                {
                    const auto& bytes = syn.smfs[i < 4 ? i : 0];
                    kit.midi.push_back ({ kit.names[i], std::span<const std::uint8_t> (bytes.data(), bytes.size()) });
                }
                KitSource source;
                source.id = kit.id;
                source.kitJson = kit.kitJson;
                source.mixJson = kit.mixJson;
                for (std::size_t i = 0; i < 4; ++i)
                    source.smfs[i] = std::span<const std::uint8_t> (syn.smfs[i].data(), syn.smfs[i].size());
                source.midiFiles = kit.midi;
                sources.push_back (source);
            }
        }

        ~SyntheticContent() override { release(); }

        // Every read waits until release (): a load that can't finish until the test says.
        void hold()
        {
            const std::lock_guard lock (holdMutex);
            holding = true;
        }

        void release()
        {
            {
                const std::lock_guard lock (holdMutex);
                holding = false;
            }
            holdChanged.notify_all();
        }

        void setSamples (Samples s) { samples.store (s); }
        void setReadDelay (std::chrono::milliseconds d) { delay.store (d); }

        // The folder locate () names for a ready kit.
        static juce::File folderOf (std::string_view id)
        {
            return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WubansiaSynthetic").getChildFile (juce::String (std::string (id)).toLowerCase());
        }

        std::atomic<int> reads { 0 }; // files read to the end
        std::atomic<int> readsStarted { 0 };
        std::atomic<int> locates { 0 };
        std::atomic<int> probesStarted { 0 }; // files the slow check (probeDelay) started on
        std::atomic<int> opens { 0 };
        std::atomic<int> fileReads { 0 }; // readFile () calls

        // The folders readFile () was asked to read from (ContentStatus::folder), in order, since the last clear.
        // Any thread: the sound thread reads too.
        std::vector<juce::File> foldersRead() const
        {
            const std::lock_guard lock (readsMutex);
            return readFrom;
        }

        void clearFoldersRead()
        {
            const std::lock_guard lock (readsMutex);
            readFrom.clear();
        }

        std::span<const KitSource> kits() const override { return sources; }

        ContentStatus locate (const engine::KitManifest& kit, const std::atomic<bool>* cancel = nullptr) override
        {
            ++locates;
            ContentStatus status;
            const auto folder = folderOf (kit.kit.id);
            status.searched.add (folder.getFullPathName());
            switch (samples.load())
            {
                case Samples::ready:
                    if (probeDelay.count() > 0)
                    {
                        // Each file's header, slowly, as listed: the check stops within one file once cancel is raised.
                        const auto checked = engine::checkKitFiles (
                            kit,
                            [&] (std::string_view path) -> util::Result<engine::WavInfo> {
                                ++probesStarted;
                                if (probeLog != nullptr)
                                    ++probeLog->started;
                                std::this_thread::sleep_for (probeDelay);
                                if (probeLog != nullptr)
                                    probeLog->lastEnd.store (std::chrono::steady_clock::now().time_since_epoch().count());
                                for (const auto& f : kit.files)
                                    if (f.path == path)
                                    {
                                        engine::WavInfo info;
                                        info.channels = f.channels;
                                        info.sampleRate = 48000;
                                        info.frames = f.frames;
                                        return util::Result<engine::WavInfo>::success (info);
                                    }
                                return util::Result<engine::WavInfo>::failure ("missing: " + std::string (path));
                            },
                            cancel);
                        if (! checked.ok())
                        {
                            status.state = ContentState::failed;
                            status.folder = folder;
                            status.problem = juce::String (checked.error);
                            break;
                        }
                    }
                    status.state = ContentState::ready;
                    status.folder = folder;
                    break;
                case Samples::missing:
                    status.state = ContentState::missing;
                    status.problem = "no content folder holds " + juce::String (kit.kit.id);
                    break;
                case Samples::lfsPointers:
                    status.state = ContentState::lfsPointers;
                    status.folder = folder;
                    status.problem = "lfs_pointer: Drums/Kick A.wav: a Git LFS pointer file";
                    break;
                case Samples::mismatch:
                    status.state = ContentState::mismatch;
                    status.folder = folder;
                    status.problem = "mismatch: Drums/Kick A.wav: 1000 frames and mono; kit.json lists 12000 frames and mono";
                    break;
                case Samples::failed:
                    status.state = ContentState::failed;
                    status.folder = folder;
                    status.problem = "unreadable: Drums/Kick A.wav: it can't be opened";
                    break;
            }
            return status;
        }

        std::unique_ptr<engine::SampleSource> open (const engine::KitManifest&, const ContentStatus&, const std::atomic<bool>* = nullptr) override
        {
            ++opens;
            return std::make_unique<Source> (*this);
        }

        // The synthetic WAV at path, as its samples setting has it: ready, the file's bytes (alteredPath's with a
        // byte flipped); missing and failed, the reader's errors; LFS pointers, a pointer file's text; mismatch,
        // the bytes with a byte flipped. Never held by hold (): only kit loads are.
        util::Result<std::vector<std::uint8_t>> readFile (const ContentStatus& found, std::string_view path) override
        {
            using R = util::Result<std::vector<std::uint8_t>>;
            ++fileReads;
            {
                const std::lock_guard lock (readsMutex);
                readFrom.push_back (found.folder);
            }
            const auto& syn = engine::testing::syntheticContent();
            const auto shown = std::string (path);
            if (found.folder == juce::File())
                return R::failure ("missing: " + shown + ": no content folder holds the kit");
            const auto folder = found.folder.getFullPathName().toStdString();
            const auto samplesNow = samples.load();
            if (samplesNow == Samples::missing)
                return R::failure ("missing: " + shown + ": no such file in " + folder);
            if (samplesNow == Samples::failed)
                return R::failure ("unreadable: " + shown + ": it can't be opened");
            for (std::size_t i = 0; i < syn.kit.files.size(); ++i)
            {
                if (syn.kit.files[i].path != path)
                    continue;
                if (samplesNow == Samples::lfsPointers)
                {
                    const std::string pointer = "version https://git-lfs.github.com/spec/v1\noid sha256:" + syn.kit.files[i].sha256 + "\nsize "
                                                + std::to_string (syn.wavs[i].size()) + "\n";
                    return R::success (std::vector<std::uint8_t> (pointer.begin(), pointer.end()));
                }
                auto bytes = syn.wavs[i];
                if (samplesNow == Samples::mismatch || path == alteredPath)
                    bytes.back() ^= 0x01;
                return R::success (std::move (bytes));
            }
            return R::failure ("missing: " + shown + ": no such file in " + folder);
        }

        juce::File userFolder() const override
        {
            const std::lock_guard lock (holdMutex);
            return chosen;
        }

        void setUserFolder (const juce::File& folder) override
        {
            {
                const std::lock_guard lock (holdMutex);
                chosen = folder;
            }
            ++folders;
        }

        std::uint64_t foldersGeneration() const noexcept override { return folders.load(); }

        // As if another instance chose a folder.
        void bumpFolders() { ++folders; }

    private:
        struct Stored
        {
            std::string id;
            std::string kitJson;
            std::string mixJson;
            std::vector<std::string> names;
            std::vector<EmbeddedMidi> midi;
        };

        class Source final : public engine::SampleSource
        {
        public:
            explicit Source (SyntheticContent& c) : owner (c) {}

            util::Result<engine::SampleData> read (std::string_view relativePath) override
            {
                ++owner.readsStarted;
                {
                    std::unique_lock lock (owner.holdMutex);
                    owner.holdChanged.wait (lock, [this] { return ! owner.holding; });
                }
                if (const auto d = owner.delay.load(); d.count() > 0)
                    std::this_thread::sleep_for (d);
                auto result = inner.read (relativePath);
                ++owner.reads;
                return result;
            }

        private:
            SyntheticContent& owner;
            engine::testing::SyntheticSampleSource inner;
        };

        std::deque<Stored> stored;
        std::vector<KitSource> sources;
        std::atomic<std::chrono::milliseconds> delay;
        const std::chrono::milliseconds probeDelay;
        const std::shared_ptr<SyntheticProbeLog> probeLog;
        const std::string alteredPath;
        std::atomic<Samples> samples;
        std::atomic<std::uint64_t> folders { 0 };

        mutable std::mutex holdMutex;
        std::condition_variable holdChanged;
        bool holding = false;
        juce::File chosen;

        mutable std::mutex readsMutex;
        std::vector<juce::File> readFrom;
    };
}
