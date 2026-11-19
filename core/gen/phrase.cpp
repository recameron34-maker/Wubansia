// The phrase library (phrase.h, ADR 0031): the spec and its loader. The generator and the library are still stubs.

#include "gen/phrase.h"

#include "gen/json.h"
#include "util/file_io.h"

#include <algorithm>
#include <charconv>
#include <system_error>

namespace wubansia::gen
{
    std::optional<PhraseKind> phraseKindFromId (std::string_view id) noexcept
    {
        for (std::size_t i = 0; i < phraseKindCount; ++i)
            if (phraseKindIds[i] == id)
                return static_cast<PhraseKind> (i);
        return std::nullopt;
    }

    std::string_view phraseKindWord (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::string_view, phraseKindCount> words { "Hook", "Call & answer", "Period", "Sparse", "Arp", "Stabs", "Counter-line", "Bounce" };
        return words[static_cast<std::size_t> (kind)];
    }

    std::string_view phraseKindFileWord (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::string_view, phraseKindCount> words { "Hook", "Answer", "Period", "Sparse", "Arp", "Stabs", "Counter", "Bounce" };
        return words[static_cast<std::size_t> (kind)];
    }

    std::int32_t phraseFormCount (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::int32_t, phraseKindCount> forms { 4, 3, 2, 2, 2, 2, 2, 3 };
        return forms[static_cast<std::size_t> (kind)];
    }

    std::string phraseId (PhraseKind kind, std::uint64_t seed) { return std::string (phraseKindIds[static_cast<std::size_t> (kind)]) + "-" + std::to_string (seed); }

    std::optional<std::pair<PhraseKind, std::uint64_t>> parsePhraseId (std::string_view id)
    {
        const auto dash = id.rfind ('-');
        if (dash == std::string_view::npos || dash + 1 >= id.size())
            return std::nullopt;
        const auto kind = phraseKindFromId (id.substr (0, dash));
        if (!kind)
            return std::nullopt;
        const std::string_view digits = id.substr (dash + 1);
        if (digits.size() > 1 && digits.front() == '0')
            return std::nullopt;
        std::uint64_t seed = 0;
        const auto [end, ec] = std::from_chars (digits.data(), digits.data() + digits.size(), seed);
        if (ec != std::errc() || end != digits.data() + digits.size())
            return std::nullopt;
        return std::pair { *kind, seed };
    }

    // ------------------------------------------------------------------ the spec

    namespace
    {
        using json::Node;

        std::string lowered (std::string_view s)
        {
            std::string out (s);
            for (char& c : out)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char> (c - 'A' + 'a');
            return out;
        }

        // Weights: count of them, each 0-100, not all 0.
        template <std::size_t N>
        std::array<std::int32_t, N> weightsOf (const Node& n)
        {
            std::array<std::int32_t, N> out {};
            const auto w = n.ints (N, N, 0, 100);
            std::int32_t total = 0;
            for (std::size_t i = 0; i < w.size() && i < N; ++i)
            {
                out[i] = w[i];
                total += w[i];
            }
            if (w.size() == N && total == 0)
                n.fail ("every weight is 0: none could be picked");
            return out;
        }

        // A kind's bar-1 rhythms: onsets 0-15 ascending, one length (1-16) per onset, the accents among the onsets.
        std::vector<HookCell> cellsOf (const Node& n)
        {
            std::vector<HookCell> out;
            if (n.isArray() && n.size() == 0)
                n.fail ("at least one cell is needed");
            std::int32_t total = 0;
            for (std::size_t i = 0; i < n.size(); ++i)
            {
                const Node c = n.at (i);
                c.only ({ "on", "len", "acc", "w" });
                HookCell cell;
                cell.on = c["on"].ints (1, 16, 0, 15);
                for (std::size_t k = 1; k < cell.on.size(); ++k)
                    if (cell.on[k] <= cell.on[k - 1])
                    {
                        c["on"].fail ("the onsets are 16ths in ascending order, each once");
                        break;
                    }
                const Node len = c["len"];
                if (len.isArray() && len.size() != cell.on.size())
                    len.fail ("one length per onset: " + std::to_string (cell.on.size()) + " onsets, " + std::to_string (len.size()) + " lengths");
                else
                    cell.len = len.ints (cell.on.size(), cell.on.size(), 1, 16);
                cell.acc = c["acc"].ints (0, 16, 0, 15);
                for (const auto a : cell.acc)
                    if (std::find (cell.on.begin(), cell.on.end(), a) == cell.on.end())
                    {
                        c["acc"].fail ("the accent on 16th " + std::to_string (a) + " isn't one of the onsets");
                        break;
                    }
                cell.weight = c["w"].int32 (0, 100);
                total += cell.weight;
                out.push_back (std::move (cell));
            }
            if (n.size() > 0 && total == 0)
                n.fail ("every weight is 0: no cell could be picked");
            return out;
        }

        PhraseKindSpec kindOf (const Node& k, PhraseKind kind, Range range)
        {
            k.only ({ "quota", "forms", "channel", "track", "cells", "pool", "start", "end", "moves", "leap", "center", "span", "vel", "ladders" });
            PhraseKindSpec ks;
            ks.quota = k["quota"].int32 (1, 64);
            const Node forms = k["forms"];
            const auto count = static_cast<std::size_t> (phraseFormCount (kind));
            if (forms.isArray() && forms.size() != count)
                forms.fail ("a " + std::string (phraseKindIds[static_cast<std::size_t> (kind)]) + " has " + std::to_string (count) + " form weights, not "
                            + std::to_string (forms.size()));
            else
            {
                ks.forms = forms.ints (count, count, 0, 100);
                std::int32_t total = 0;
                for (const auto w : ks.forms)
                    total += w;
                if (ks.forms.size() == count && total == 0)
                    forms.fail ("every weight is 0: no form could be picked");
            }
            ks.channel = static_cast<std::uint8_t> (k.int32Or ("channel", 2, 1, 16));
            if (const Node t = k.optional ("track"); t.exists())
                ks.track = t.string (1, 32);
            if (const Node c = k.optional ("cells"); c.exists())
                ks.cells = cellsOf (c);
            if (const Node pool = k.optional ("pool"); pool.exists())
                ks.pool = pool.ints (1, 7, 0, 6);
            if (const Node w = k.optional ("start"); w.exists())
                ks.start = weightsOf<7> (w);
            if (const Node w = k.optional ("end"); w.exists())
                ks.end = weightsOf<7> (w);
            if (const Node w = k.optional ("moves"); w.exists())
                ks.moves = weightsOf<8> (w);
            ks.leap = k.int32Or ("leap", 0, 1, 12);
            ks.center = k.int32Or ("center", 0, range.lo, range.hi);
            ks.span = k.int32Or ("span", 0, 1, 24);
            ks.velShift = k.int32Or ("vel", 0, -40, 40);
            const Node ladders = kind == PhraseKind::arp ? k["ladders"] : k.optional ("ladders");
            if (kind != PhraseKind::arp && ladders.exists())
                ladders.fail ("ladders are the arp's only");
            else if (ladders.exists())
            {
                if (ladders.isArray() && ladders.size() == 0)
                    ladders.fail ("at least one ladder is needed");
                for (std::size_t i = 0; i < ladders.size(); ++i)
                    ks.ladders.push_back (ladders.at (i).ints (1, 16, 0, 8));
            }
            return ks;
        }

        std::vector<std::string> wordsOf (const Node& n)
        {
            if (n.isArray() && n.size() != 24)
            {
                n.fail ("24 words, not " + std::to_string (n.size()));
                return {};
            }
            auto words = n.strings (24, 24);
            for (std::size_t i = 0; i < words.size(); ++i)
            {
                const auto& w = words[i];
                bool ok = !w.empty() && w.size() <= 16;
                for (const char c : w)
                    ok = ok && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
                if (!ok)
                {
                    n.at (i).fail ("a word is 1-16 letters, A-Z and a-z, not \"" + w + "\"");
                    break;
                }
                if (std::find (words.begin(), words.begin() + static_cast<std::ptrdiff_t> (i), w) != words.begin() + static_cast<std::ptrdiff_t> (i))
                {
                    n.at (i).fail ("\"" + w + "\" twice");
                    break;
                }
            }
            return words;
        }
    }

    util::Result<PhraseSpec> parsePhraseSpec (std::string_view text, std::string_view source)
    {
        using R = util::Result<PhraseSpec>;
        const auto parsed = json::parse (text);
        if (!parsed.ok())
            return R::failure (std::string (source) + ": " + parsed.error);
        json::Errors errors { std::string (source) };
        const Node root = json::root (parsed.value, errors);
        root.only ({ "format", "kit", "tonic", "bpm", "about", "range", "swing", "chords", "material", "ending", "nearDuplicate", "kinds", "names", "rename", "cut" });
        PhraseSpec spec;
        (void) root["format"].int32 (1, 1);
        spec.kitId = root["kit"].string (1, 16);
        spec.tonic = root["tonic"].int32 (0, 11);
        spec.bpm = root["bpm"].int32 (128, 140);
        spec.about = root["about"].string (1, 400);
        {
            const Node range = root["range"];
            const auto r = range.ints (2, 2, 0, 127);
            if (r.size() == 2)
            {
                spec.range = { r[0], r[1] };
                if (r[0] > r[1])
                    range.fail ("the range is empty: " + std::to_string (r[0]) + " is above " + std::to_string (r[1]));
            }
        }
        spec.swing = root["swing"].int32 (0, 120);
        {
            const Node chords = root["chords"];
            if (chords.isArray() && (chords.size() < 1 || chords.size() > 4))
                chords.fail ("1-4 chords, one a bar, not " + std::to_string (chords.size()));
            else
                for (std::size_t i = 0; i < chords.size(); ++i)
                {
                    const Node c = chords.at (i);
                    c.only ({ "name", "degree", "keys" });
                    AppChord ch;
                    ch.name = c["name"].string (1, 16);
                    ch.degree = c["degree"].int32 (0, 6);
                    ch.keys = c["keys"].ints (1, 8, 0, 127);
                    spec.chords.push_back (std::move (ch));
                }
        }
        {
            const Node m = root["material"];
            m.only ({ "register", "contours", "cells", "mutate", "answer", "pitch", "vel", "guard", "period", "pedal" });
            if (m.exists())
                readHookMaterial (m, spec.material);
        }
        spec.ending = weightsOf<7> (root["ending"]);
        spec.nearDuplicate = root["nearDuplicate"].int32 (1, 1000);
        {
            const Node kinds = root["kinds"];
            kinds.only ({ "hook", "answer", "period", "sparse", "arp", "stabs", "counter", "bounce" });
            for (std::size_t i = 0; i < phraseKindCount; ++i)
                spec.kinds[i] = kindOf (kinds[phraseKindIds[i]], static_cast<PhraseKind> (i), spec.range);
        }
        {
            const Node names = root["names"];
            names.only ({ "first", "second" });
            spec.words[0] = wordsOf (names["first"]);
            spec.words[1] = wordsOf (names["second"]);
            for (std::size_t i = 0; i < spec.words[1].size() && !errors.any(); ++i)
                if (std::find (spec.words[0].begin(), spec.words[0].end(), spec.words[1][i]) != spec.words[0].end())
                    names["second"].at (i).fail ("\"" + spec.words[1][i] + "\" is in both lists");
        }
        for (const auto& [id, name] : root["rename"].members())
        {
            const auto parsedId = parsePhraseId (id);
            if (!parsedId || parsedId->second >= firstMoreSeed)
            {
                name.fail ("no library phrase can be \"" + id + "\": a library id is <kind>-<seed>, the seed under " + std::to_string (firstMoreSeed));
                continue;
            }
            spec.rename.emplace_back (id, name.string (1, 40));
        }
        for (const auto& [kindId, c] : root["cut"].members())
        {
            const auto kind = phraseKindFromId (kindId);
            if (!kind)
            {
                c.fail ("unknown kind \"" + kindId + "\"");
                continue;
            }
            PhraseKindSpec& ks = spec.kinds[static_cast<std::size_t> (*kind)];
            if (c.isString())
            {
                if (c.string() != "all")
                    c.fail ("a cut is a list of seeds, or \"all\" for the whole kind, not \"" + c.string() + "\"");
                ks.cutAll = true;
                continue;
            }
            for (std::size_t i = 0; i < c.size(); ++i)
            {
                const auto seed = static_cast<std::uint64_t> (c.at (i).integer (0, static_cast<std::int64_t> (firstMoreSeed) - 1));
                if (std::find (ks.cut.begin(), ks.cut.end(), seed) != ks.cut.end())
                {
                    c.fail (std::to_string (seed) + " is listed twice");
                    break;
                }
                ks.cut.push_back (seed);
            }
        }
        if (errors.any())
            return R::failure (errors.first());
        return R::success (std::move (spec));
    }

    std::filesystem::path phraseSpecPath (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        return contentDir / "phrases" / lowered (kitId) / (std::string (kitId) + "_phrases.json");
    }

    std::filesystem::path phraseLibraryPath (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        return contentDir / "phrases" / lowered (kitId) / (std::string (kitId) + "_phrase_library.json");
    }

    util::Result<PhraseSpec> loadPhraseSpec (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        using R = util::Result<PhraseSpec>;
        const auto shown = [] (const std::filesystem::path& p) {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        };
        const auto specPath = phraseSpecPath (contentDir, kitId);
        const auto bytes = util::readBinaryFile (specPath);
        if (!bytes.ok())
            return R::failure ("no phrase spec for \"" + std::string (kitId) + "\": " + bytes.error);
        const std::string text (bytes.value.begin(), bytes.value.end());
        auto spec = parsePhraseSpec (text, shown (specPath));
        if (!spec.ok())
            return spec;
        const auto kitPath = contentDir / "kits" / (lowered (kitId) + ".json");
        const auto kitBytes = util::readBinaryFile (kitPath);
        if (!kitBytes.ok())
            return R::failure ("no kit \"" + std::string (kitId) + "\": " + kitBytes.error);
        const auto kit = parseKit (std::string_view (reinterpret_cast<const char*> (kitBytes.value.data()), kitBytes.value.size()), shown (kitPath));
        if (!kit.ok())
            return R::failure (kit.error);
        const auto lineOf = [&text] (std::string_view key) {
            const auto v = json::parse (text);
            const json::Value* x = v.ok() ? v.value.find (key) : nullptr;
            return x != nullptr ? x->line : 0u;
        };
        const auto refuse = [&] (std::string_view key, const std::string& message) {
            return R::failure (shown (specPath) + ": " + std::string (key) + ": line " + std::to_string (lineOf (key)) + ": " + message);
        };
        if (spec.value.kitId != kitId)
            return refuse ("kit", "the spec is for \"" + spec.value.kitId + "\", not this folder's " + std::string (kitId));
        if (spec.value.tonic != kit.value.tonic)
            return refuse ("tonic", "the tonic is " + std::to_string (spec.value.tonic) + ", the kit's (" + shown (kitPath) + ") " + std::to_string (kit.value.tonic));
        if (spec.value.bpm != kit.value.bpm)
            return refuse ("bpm", "the bpm is " + std::to_string (spec.value.bpm) + ", the kit's (" + shown (kitPath) + ") " + std::to_string (kit.value.bpm));
        return spec;
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
