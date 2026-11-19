#include "services/DrumKitService.h"

#include "sounds/DrumKit.h"
#include "sounds/DrumKitWriter.h"

#include <algorithm>
#include <string>
#include <utility>

namespace wubansia
{
    namespace
    {
        juce::String text (const std::string& s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // A pad the SFZ can't carry, as the button says it after "Couldn't save FUT05's drum kit: ": "its pad 36 C1 ...".
        juce::String planProblem (const juce::String& kit, const std::string& error)
        {
            const auto words = text (error);
            const auto own = kit + "'s pad ";
            return words.startsWith (own) ? "its pad " + words.substring (own.length()) : words;
        }
    }

    DrumKitService::DrumKitService (ContentSourceFactory& c) : content (c) {}

    DrumKitService::~DrumKitService()
    {
        {
            const std::lock_guard lock (mutex);
            quit = true;
            queued.reset();
            cancel.store (true);
        }
        wake.notify_all();
        if (thread.joinable())
            thread.join(); // after the file in hand: the writer has deleted its staging folder
    }

    std::uint32_t DrumKitService::request (Request r)
    {
        const Key key { r.kit, r.manifest.kit.contentVersion, r.samplesFolder, r.found.folder, r.kickSide };
        std::optional<std::pair<Key, DrumKitStatus>> last;
        {
            const std::lock_guard lock (mutex);
            if (quit)
                return 0;
            if (queued && queued->key == key)
                return queued->ticket; // its click: the same job
            if (inHandKey && *inHandKey == key && ! cancel.load())
            {
                queued.reset(); // back to the job in hand: the one queued since is dropped
                if (wanted != inHandTicket)
                {
                    wanted = inHandTicket;
                    current = {};
                    current.ticket = inHandTicket;
                    current.stage = DrumKitStage::saving;
                    current.kit = key.kit;
                }
                return inHandTicket;
            }
            last = lastReady;
        }

        // The last kit saved, still there: ready at once, with its ticket. Two stats, no byte check, so a drag that follows
        // the press starts inside its mouse event.
        if (last && last->first == key && last->second.folder.isDirectory() && last->second.sfz.existsAsFile())
        {
            const std::lock_guard lock (mutex);
            if (quit)
                return 0;
            queued.reset();
            if (inHandKey)
                cancel.store (true);
            wanted = last->second.ticket;
            current = last->second;
            return wanted;
        }

        std::uint32_t ticket = 0;
        {
            const std::lock_guard lock (mutex);
            if (quit)
                return 0;
            ticket = ++latest; // 4 billion requests before it wraps
            if (inHandKey)
                cancel.store (true); // replaced: the job in hand stops between two files
            queued = Job { ticket, key, std::move (r) };
            wanted = ticket;
            current = {};
            current.ticket = ticket;
            current.stage = DrumKitStage::saving;
            current.kit = key.kit;
            if (! thread.joinable())
            {
                startedFlag.store (true, std::memory_order_release);
                thread = std::thread ([this] { run(); });
            }
        }
        wake.notify_all();
        return ticket;
    }

    void DrumKitService::refuse (const juce::String& kit, const juce::String& problem)
    {
        const std::lock_guard lock (mutex);
        queued.reset();
        if (inHandKey)
            cancel.store (true);
        wanted = 0;
        current = {};
        current.stage = DrumKitStage::failed;
        current.kit = kit;
        current.problem = problem;
        current.sourceProblem = true; // the kit or its samples, never the samples folder
    }

    DrumKitStatus DrumKitService::status() const
    {
        const std::lock_guard lock (mutex);
        return current;
    }

    void DrumKitService::run()
    {
        juce::Thread::setCurrentThreadName ("Wubansia drum kits");
        for (;;)
        {
            Job job;
            {
                std::unique_lock lock (mutex);
                wake.wait (lock, [this] { return quit || queued.has_value(); });
                if (quit)
                    return;
                job = std::move (*queued);
                queued.reset();
                inHandKey = job.key;
                inHandTicket = job.ticket;
                cancel.store (false);
            }

            DrumKitStatus result;
            result.ticket = job.ticket;
            result.kit = job.request.kit;
            bool cancelled = false;
            const auto plan = sounds::planDrumKit (job.request.manifest, job.request.mix);
            if (! plan.ok())
            {
                result.stage = DrumKitStage::failed;
                result.problem = planProblem (job.request.kit, plan.error);
            }
            else
            {
                const auto written = sounds::writeDrumKit (content, job.request.found, job.request.manifest, plan.value, job.request.samplesFolder,
                    cancel, fileProbe);
                if (written.cancelled)
                    cancelled = true;
                else if (written.folder == juce::File())
                {
                    result.stage = DrumKitStage::failed;
                    result.problem = text (written.failure.problem);
                    result.detail = text (written.failure.detail);
                    result.sourceProblem = written.failure.sourceProblem;
                    result.reinstall = written.failure.reinstall;
                }
                else
                {
                    result.stage = DrumKitStage::ready;
                    result.folder = written.folder;
                    result.sfz = written.folder.getChildFile (text (plan.value.sfz[static_cast<std::size_t> (std::clamp (job.request.kickSide, 0, 1))]));
                    result.wrote = written.wrote;
                    result.files = static_cast<int> (plan.value.files.size());
                }
            }

            const std::lock_guard lock (mutex);
            inHandKey.reset();
            inHandTicket = 0;
            if (cancelled || quit)
                continue;
            if (result.stage == DrumKitStage::ready)
                lastReady = std::make_pair (job.key, result);
            if (job.ticket == wanted)
                current = result;
        }
    }
}
