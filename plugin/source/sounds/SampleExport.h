#pragma once

// The Samples drawer's copies (ADR 0024): a sound's file copied from the kit's content folder into the
// person's samples folder, under a name that says what it is, for the OS drag to hand to the DAW.
//
//     <samples folder>/<KIT> <Title>/<stem> - <KIT> <BPM> <Key> <Camelot>[ c<N>][ (<k>)].wav
//     ~/Music/Wubansia/Samples/FUT01 Alloy Signal/Rave Stab v2 - FUT01 134 F#m 11A.wav
//
// - <stem> is the installed file's name without folder and ".wav", and without a leading "Accent ", which the
//   drawer drops from names too (18 of FUT03's and FUT04's): the four kits' 618 names are still unique per kit
//   ignoring case, in safe characters, and carry the note, the layer and the round robin. The sound comes first,
//   so a Drum Rack pad or Simpler shows it: "Dub Chord Cm9 - FUT03 128 Gm 6A.wav", not "Accent Dub Ch...".
// - <Key> is short (dragout::shortKey: F#m, Bbmaj). " c<N>" marks content version N from 2 on, so a later
//   version's file never takes an earlier one's name. " (<k>)" is added only when a file of the plain name
//   holds other bytes. Only safe characters (dragout::safeName), and at most kMaxNameStem before " (<k>)".
// - The names are frozen once v0.2 ships: projects refer to the copies by name.
// - The bytes are the installed file's, exactly: checked against kit.json's size and SHA-256 before
//   anything is written, and never decoded, re-encoded, trimmed or tagged.
// - A copy is written once, as a hidden temporary file that is renamed into place after the whole write is
//   checked. A later export of the same file finds it and writes nothing. A file of that name holding other
//   bytes (a copy edited in a DAW, say) is never touched: the copy takes the next name. Nothing here
//   overwrites or deletes a file; the folder only grows, and Wubansia never tidies it.
//
// exportCopy () blocks on the disk (tens of milliseconds for the largest files): the message thread, since the
// OS drag must start inside its mouse event.

#include "Controller.h"
#include "content/ContentSource.h"

#include "kit/kit.h"
#include "kit/manifest.h"

#include <juce_core/juce_core.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace wubansia::sounds
{
    // The longest export name before " (<k>)" and ".wav": a long installed name is cut to fit.
    inline constexpr std::size_t kMaxNameStem = 196;

    // <the user's Music folder>/Wubansia/Samples: ~/Music on macOS and Linux (XDG_MUSIC_DIR), My Music on Windows.
    juce::File defaultSamplesFolder();

    // The kit's folder in the samples folder: "FUT01 Alloy Signal".
    std::string kitFolderName (const engine::KitInfo& kit);

    // The file's export name: "Rave Stab v2 - FUT01 134 F#m 11A.wav".
    std::string exportName (const engine::KitInfo& kit, const engine::FileEntry& file);

    // The size of the file kit.json lists: a 24-bit PCM WAV of its frames and channels holding only its 'fmt '
    // and 'data' chunks, as every content file is: 44 bytes, the audio, and a pad byte when the audio is odd.
    std::uint64_t listedBytes (const engine::FileEntry& file) noexcept;

    // Whether bytes are exactly the file kit.json lists: its size (listedBytes ()), then its SHA-256.
    bool matchesListing (std::span<const std::uint8_t> bytes, const engine::FileEntry& file);

    // Why bytes that don't match their listing are refused, for people:
    // "Rave Stab v2.wav isn't the file FUT01's content v1 lists: reinstall FUT01".
    std::string listingProblem (const engine::KitInfo& kit, const engine::FileEntry& file);

    struct ExportResult
    {
        juce::File file; // the copy; juce::File () on failure
        std::string problem; // why there is none, for people
    };

    // The file's copy in samplesFolder/kitFolderName (kit)/, made if missing (see the top). found is the kit's
    // ready content status, whose folder content.readFile () reads. Blocks on the disk: the message thread.
    ExportResult exportCopy (ContentSourceFactory& content, const ContentStatus& found, const engine::KitInfo& kit,
        const engine::FileEntry& file, const juce::File& samplesFolder);
}
