// The phrase library (phrase.h, ADR 0031). Declarations only so far: every function fails or returns nothing.

#include "gen/phrase.h"

namespace wubansia::gen
{
    std::optional<PhraseKind> phraseKindFromId (std::string_view) noexcept { return std::nullopt; }
    std::string_view phraseKindWord (PhraseKind) noexcept { return {}; }
    std::string_view phraseKindFileWord (PhraseKind) noexcept { return {}; }
    std::int32_t phraseFormCount (PhraseKind) noexcept { return 0; }

    util::Result<PhraseSpec> parsePhraseSpec (std::string_view, std::string_view source)
    {
        return util::Result<PhraseSpec>::failure (std::string (source) + ": not there yet");
    }

    std::filesystem::path phraseSpecPath (const std::filesystem::path& contentDir, std::string_view) { return contentDir; }
    std::filesystem::path phraseLibraryPath (const std::filesystem::path& contentDir, std::string_view) { return contentDir; }

    util::Result<PhraseSpec> loadPhraseSpec (const std::filesystem::path&, std::string_view kitId)
    {
        return util::Result<PhraseSpec>::failure (std::string (kitId) + ": not there yet");
    }

    rng::Key phraseKey (std::string_view, PhraseKind, std::uint64_t) noexcept { return rng::seedKey (0); }
    Phrase generatePhrase (const PhraseSpec&, PhraseKind kind, std::uint64_t seed)
    {
        Phrase p;
        p.kind = kind;
        p.seed = seed;
        return p;
    }

    bool PhraseChecks::pass() const noexcept { return false; }
    PhraseChecks checkPhrase (const PhraseSpec&, const Phrase&) { return {}; }
    std::int32_t phraseSimilarity (const Phrase&, const Phrase&) { return 1000; }
    std::vector<std::string> phraseTags (const PhraseSpec&, const Phrase&) { return {}; }

    std::string phraseNameAt (const PhraseSpec&, std::size_t) { return {}; }
    std::string phraseName (const PhraseSpec&, PhraseKind, std::uint64_t) { return {}; }
    std::string phraseId (PhraseKind, std::uint64_t) { return {}; }
    std::optional<std::pair<PhraseKind, std::uint64_t>> parsePhraseId (std::string_view) { return std::nullopt; }

    midi::SongSpec phraseSongSpec (const PhraseSpec&, const Phrase&, std::string_view) { return {}; }
    util::Result<std::vector<std::uint8_t>> phraseSmf (const PhraseSpec&, const Phrase&, std::string_view)
    {
        return util::Result<std::vector<std::uint8_t>>::failure ("not there yet");
    }

    std::vector<LibraryRow> selectLibrary (const PhraseSpec&, std::uint64_t) { return {}; }
    std::string libraryJson (const PhraseSpec&, std::string_view, const std::vector<LibraryRow>&) { return {}; }
    util::Result<PhraseLibrary> parsePhraseLibrary (std::string_view, std::string_view source)
    {
        return util::Result<PhraseLibrary>::failure (std::string (source) + ": not there yet");
    }

    MorePress morePhrases (const PhraseSpec&, PhraseKind, std::uint64_t fromSeed, std::span<const Phrase>, std::int32_t, std::int32_t)
    {
        MorePress m;
        m.nextSeed = fromSeed;
        return m;
    }
}
