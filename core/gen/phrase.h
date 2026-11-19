#pragma once

// The phrase library (ADR 0031): 8-bar melodic phrases written over a kit's app chord loop, in eight kinds,
// from the hook's material (gen/hook_material.h, ADR 0026). A phrase is a pure function of (spec, kind, seed):
// integers only, every draw below phraseKey (kit, kind, seed), so no song draw moves and the same inputs give
// the same bytes on every platform.
//
// Per kit, two files in content/phrases/<kit>/:
// - <KIT>_phrases.json, the spec (parsePhraseSpec): the chord loop, the register, the swing, the material
//   (the style's hook block, copied), the kinds, the name words, renames and cuts;
// - <KIT>_phrase_library.json, the library (libraryJson): the rows selectLibrary chose, each with its file's
//   SHA-256. Generated, and changed only in its own commit, like the generator snapshots.

#include "gen/content.h"
#include "gen/song.h"
#include "midi/writer.h"
#include "rng/rng.h"
#include "util/result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wubansia::gen
{
    enum class PhraseKind : std::uint8_t {
        hook, // the 2-bar riff stated four times, the last statement varied and ended
        answer, // call and answer: the riff's head, answered by a fresh second half
        period, // the 4-bar period (organ, glass or stab idiom), or the riff then the period
        sparse, // the riff's accents only: room for a vocal
        arp, // chord tones of each bar's chord on a 16th grid, a turnaround in bars 7-8
        stabs, // chord-tone stabs voice-led with the chords, a second voice on the accents
        counter, // a slow guide-tone line: long notes, steps, a held ending
        bounce // root and 5th jumping octaves on a syncopated grid
    };
    inline constexpr std::size_t phraseKindCount = 8;
    inline constexpr std::array<std::string_view, phraseKindCount> phraseKindIds { "hook", "answer", "period", "sparse", "arp", "stabs", "counter", "bounce" };

    std::optional<PhraseKind> phraseKindFromId (std::string_view id) noexcept;
    // "Hook", "Call & answer", "Period", "Sparse", "Arp", "Stabs", "Counter-line", "Bounce" (the drawer's words).
    std::string_view phraseKindWord (PhraseKind kind) noexcept;
    // "Hook", "Answer", "Period", "Sparse", "Arp", "Stabs", "Counter", "Bounce" (file names: safe characters).
    std::string_view phraseKindFileWord (PhraseKind kind) noexcept;
    // The kind's forms (2.4 of the plan): hook 4, answer 3, period 2, sparse 2, arp 2, stabs 2, counter 2, bounce 3.
    std::int32_t phraseFormCount (PhraseKind kind) noexcept;

    inline constexpr std::int32_t phraseBars = 8;
    inline constexpr std::uint64_t firstMoreSeed = 1000000; // More's seeds start here; the library's are below

    // One bar of the app's chord loop, cycling from the phrase's bar 1.
    struct AppChord
    {
        std::string name; // "F#m9"
        std::int32_t degree = 0; // the root's degree in the kit's natural minor, 0-6
        std::vector<std::int32_t> keys; // the app's voicing, low to high: the guard's chord keys, held all bar
    };

    // A kind's construction and what it changes of the material (empty or 0: the material's).
    struct PhraseKindSpec
    {
        std::int32_t quota = 1; // phrases in the library, 1-64 (never 0: a cut is "cut", not a lower quota)
        std::vector<std::int32_t> forms; // weight per form, as many as the kind has
        std::uint8_t channel = 2; // 1-based MIDI channel of its track: 2, the lead's
        std::string track = "Lead"; // its track's name (FUT02's periods: "Glass Ping")
        std::vector<HookCell> cells;
        std::vector<std::int32_t> pool;
        std::array<std::int32_t, 7> start {}, end {}; // all 0: the material's
        std::array<std::int32_t, 8> moves {};
        std::int32_t leap = 0, center = 0, span = 0, velShift = 0;
        std::vector<std::vector<std::int32_t>> ladders; // arp: chord-tone index cells (required there)
        std::vector<std::uint64_t> cut; // library seeds cut later (the second pass refills the quota)
        bool cutAll = false; // "cut": "all": the kind is cut from the library, not refilled
    };

    struct PhraseSpec
    {
        std::string kitId; // "FUT01"
        std::string about;
        std::int32_t tonic = 0; // pitch class; loadPhraseSpec checks it and bpm against content/kits/<kit>.json
        Mode mode = Mode::aeolian; // every kit is minor; the key signature is midi::sharpsFor (tonic, true)
        std::int32_t bpm = 0;
        Range range; // the phrase register
        std::int32_t swing = 0; // odd 16ths late by this many ticks at 960 PPQ
        std::vector<AppChord> chords; // 1-4 bars
        HookSpec material; // the hook block without lane, withhold and plan
        std::array<std::int32_t, 7> ending {}; // VARY's last degree, by weight
        std::int32_t nearDuplicate = 700; // permille
        std::array<PhraseKindSpec, phraseKindCount> kinds;
        std::array<std::vector<std::string>, 2> words; // 24 and 24 name words
        std::vector<std::pair<std::string, std::string>> rename; // id -> name
    };

    // content/phrases/<kit>/<KIT>_phrases.json (strict: integers and strings, every unknown key refused, each
    // refusal naming the file, the JSON path and the line). source names the file in messages.
    [[nodiscard]] util::Result<PhraseSpec> parsePhraseSpec (std::string_view text, std::string_view source);

    std::filesystem::path phraseSpecPath (const std::filesystem::path& contentDir, std::string_view kitId);
    std::filesystem::path phraseLibraryPath (const std::filesystem::path& contentDir, std::string_view kitId);

    // The kit's spec, refused when its kit isn't the folder's or its tonic or bpm isn't content/kits/<kit>.json's.
    [[nodiscard]] util::Result<PhraseSpec> loadPhraseSpec (const std::filesystem::path& contentDir, std::string_view kitId);

    struct Phrase
    {
        PhraseKind kind = PhraseKind::hook;
        std::uint64_t seed = 0;
        std::int32_t form = 0;
        std::int32_t bars = phraseBars;
        std::vector<Note> notes; // 480 PPQ, tick = step x 120 (the score, unswung), by (tick, key); noteFlag::top on
                                 // accents, noteFlag::pedal on FUT02's pedal
    };

    // seedKey (seed).with (tag ("phrase")).with (tag (kit)).with (tag (kind's id)): permanent.
    rng::Key phraseKey (std::string_view kitId, PhraseKind kind, std::uint64_t seed) noexcept;
    Phrase generatePhrase (const PhraseSpec& spec, PhraseKind kind, std::uint64_t seed);

    struct PhraseChecks
    {
        bool inKey = true, inRange = true, everyBar = true, motif = true, variation = true, ending = true, rhythm = true, clean = true,
             synth = true, melody = true, strongChords = true;
        std::int32_t notes = 0, low = 0, high = 0, strong = 0, strongTones = 0, offBeat = 0, maxPerBar = 0, units = 0, maxRun = 0,
                     keys = 0, rubs = 0;

        bool pass() const noexcept;
    };
    PhraseChecks checkPhrase (const PhraseSpec& spec, const Phrase& phrase);

    // Permille: the Jaccard similarity of the two phrases' (16th, key) sets.
    std::int32_t phraseSimilarity (const Phrase& a, const Phrase& b);

    // Density ("sparse", "steady", "busy"), register ("low", "mid", "high"), then "pedal", "dyads" and "breath" where
    // they apply.
    std::vector<std::string> phraseTags (const PhraseSpec& spec, const Phrase& phrase);

    // Names. The rule: first[i % 24] + " " + second[(7i + i / 24) % 24], 576 names per kit. A phrase's index comes from
    // its kind and seed, never from its place in the library: kind k owns indices 72k to 72k + 71; a library seed s
    // takes 72k + s mod 48, a More seed s' = s - firstMoreSeed takes 72k + 48 + s' mod 24, and from a second lap on
    // (s / 48, s' / 24) the name gains " 2", " 3"...
    std::string phraseNameAt (const PhraseSpec& spec, std::size_t index);
    std::string phraseName (const PhraseSpec& spec, PhraseKind kind, std::uint64_t seed); // the spec's rename first

    std::string phraseId (PhraseKind kind, std::uint64_t seed); // "hook-5"
    std::optional<std::pair<PhraseKind, std::uint64_t>> parsePhraseId (std::string_view id);

    // The file: SMF type 1, 960 PPQ, 8 bars; the conductor named "<KIT> <name>", the tempo round (60,000,000 / bpm),
    // 4/4 and the key; one track, the kind's ("Lead" on channel 2), the kit's swing written in, each note a 16th at
    // least unless the next one starts sooner, ending 10 ticks before it, never past bar 8.
    midi::SongSpec phraseSongSpec (const PhraseSpec& spec, const Phrase& phrase, std::string_view name);
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> phraseSmf (const PhraseSpec& spec, const Phrase& phrase, std::string_view name);

    // ---- the library

    struct LibraryRow
    {
        std::string id; // "hook-5": kind and seed
        std::string name; // "Night Bus"
        PhraseKind kind = PhraseKind::hook;
        std::uint64_t seed = 0;
        std::int32_t form = 0, notes = 0, low = 0, high = 0;
        std::vector<std::string> tags;
        std::string sha256; // of phraseSmf (spec, phrase, name)

        friend bool operator== (const LibraryRow&, const LibraryRow&) = default;
    };

    // Two passes. 1: per kind in kind order, the first seeds whose phrase passes every check and is under
    // nearDuplicate against every phrase kept so far (all kinds), until the quota (cuts ignored). 2: the cut rows
    // go; each kind left short, in kind order, scans on from its last pass-1 seed (cuts skipped) for phrases that
    // pass and are no near-duplicate of anything kept. A kind cut "all" goes and isn't refilled. At most scanLimit
    // seeds a kind. Rows in kind order, then seed order.
    std::vector<LibraryRow> selectLibrary (const PhraseSpec& spec, std::uint64_t scanLimit = 4096);

    // The library file: LF line ends, one row per line, its keys in a fixed order; "spec" is specText's SHA-256.
    std::string libraryJson (const PhraseSpec& spec, std::string_view specText, const std::vector<LibraryRow>& rows);

    struct PhraseLibrary
    {
        std::string kitId;
        std::string specSha256;
        std::vector<LibraryRow> rows;
    };
    [[nodiscard]] util::Result<PhraseLibrary> parsePhraseLibrary (std::string_view text, std::string_view source);

    // ---- More

    struct MorePress
    {
        std::vector<Phrase> phrases; // up to count, each passing every check and no near-duplicate of `against` or of each other
        std::uint64_t nextSeed = firstMoreSeed; // where the next press starts
        std::int32_t candidates = 0;
    };

    // One press for a kind: seeds from fromSeed on, at most maxCandidates of them.
    MorePress morePhrases (const PhraseSpec& spec, PhraseKind kind, std::uint64_t fromSeed, std::span<const Phrase> against, std::int32_t count = 8,
        std::int32_t maxCandidates = 1000);
}
