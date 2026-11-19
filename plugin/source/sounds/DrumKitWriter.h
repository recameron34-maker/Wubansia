#pragma once

// Drum kits (ADR 0030): a DrumKitPlan written into the samples folder, once, never over anything.
//
// - A folder of the kit's name whose every planned file is there with the planned bytes (each WAV kit.json's size and
//   SHA-256, each text the plan's) is the kit: nothing is written. Other files there (Live's .asd analysis files, say)
//   don't count.
// - A folder of that name that differs (a sample edited or deleted, the SFZ edited), or a file or a link at the name, is
//   never touched: the kit goes to "<folder> (2)", then "(3)", up to "(999)".
// - Written whole or not at all: each installed file is read (ContentSourceFactory::readFile) and checked against
//   kit.json (matchesListing) as it is copied into a hidden staging folder in the samples folder,
//   ".wubansia-drumkit-<KIT>-<8 hex digits>", which is then renamed to the kit's name with renameNoReplace (never
//   juce::File::moveFileTo, which deletes the target first). On a failure or a cancel the staging folder, which only
//   this wrote, is deleted. Losing a race to another instance: the other's folder is checked like any existing one.
//
// writeDrumKit () blocks on the disk for as long as the disk takes: the drum kit thread (DrumKitService), never the
// message or the audio thread.

#include "sounds/DrumKit.h"
#include "sounds/SampleExport.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>

namespace wubansia::sounds
{
    struct DrumKitWrite
    {
        juce::File folder; // the kit's folder, written or found; juce::File () on failure or cancel
        bool wrote = false; // false: a folder of that name already held every planned file, byte for byte
        bool cancelled = false;
        ExportResult failure; // problem, detail, sourceProblem, reinstall (SampleExport.h's words)
    };

    // The plan's folder in samplesFolder, found or written (see the top). found is the kit's ready content status. Checks
    // cancel between files; progress (may be empty) gets the files done so far.
    DrumKitWrite writeDrumKit (ContentSourceFactory& content, const ContentStatus& found, const engine::KitManifest& kit,
        const DrumKitPlan& plan, const juce::File& samplesFolder, const std::atomic<bool>& cancel,
        const std::function<void (int filesDone)>& progress = {});

    // Whether folder holds every file of the plan with the planned bytes (extra files don't count).
    bool holdsDrumKit (const juce::File& folder, const engine::KitManifest& kit, const DrumKitPlan& plan);

    // A rename that never replaces anything: false, changing nothing, when to exists in any form (a file, a folder, even
    // empty, or a link). macOS renamex_np (RENAME_EXCL), Linux renameat2 (RENAME_NOREPLACE) or, where the file system
    // refuses the flag, a plain rename while to doesn't exist; Windows MoveFileExW without MOVEFILE_REPLACE_EXISTING.
    bool renameNoReplace (const juce::File& from, const juce::File& to);
}
