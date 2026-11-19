// The phrase library (gen/phrase.h, ADR 0031): tests 1-15 of the phrases plan, with the brief's 13 and 13a.
// Unless a test names its kits, it runs on the four kits' specs and libraries in content/phrases/.

#include "gen/json.h"
#include "gen/phrase.h"
#include "gen/theory.h"
#include "midi/for_synths.h"
#include "midi/reader.h"
#include "rng/rng.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
    namespace fs = std::filesystem;

    const fs::path sourceDir = WUBANSIA_SOURCE_DIR;
    const fs::path contentDir = sourceDir / "content";
    const fs::path fixtureDir = sourceDir / "core" / "tests" / "fixtures" / "phrases";
    constexpr std::array<std::string_view, 4> kits { "FUT01", "FUT02", "FUT03", "FUT04" };

    std::string readText (const fs::path& p)
    {
        const auto bytes = util::readBinaryFile (p);
        REQUIRE (bytes.ok());
        return std::string (bytes.value.begin(), bytes.value.end());
    }

    std::string sha (std::string_view s) { return util::sha256Hex (std::span (reinterpret_cast<const std::uint8_t*> (s.data()), s.size())); }

    PhraseSpec specOf (std::string_view kit)
    {
        auto spec = loadPhraseSpec (contentDir, kit);
        INFO (spec.error);
        REQUIRE (spec.ok());
        return spec.value;
    }

    PhraseLibrary libraryOf (std::string_view kit)
    {
        const fs::path p = phraseLibraryPath (contentDir, kit);
        auto lib = parsePhraseLibrary (readText (p), p.string());
        INFO (lib.error);
        REQUIRE (lib.ok());
        return lib.value;
    }

    PhraseKind kindAt (std::size_t i) { return static_cast<PhraseKind> (i); }

    std::int32_t quotaSum (const PhraseSpec& spec)
    {
        std::int32_t n = 0;
        for (const auto& k : spec.kinds)
            n += k.cutAll ? 0 : k.quota;
        return n;
    }

    std::vector<std::uint8_t> fileOf (const PhraseSpec& spec, const Phrase& p, std::string_view name)
    {
        auto smf = phraseSmf (spec, p, name);
        INFO (smf.error);
        REQUIRE (smf.ok());
        return smf.value;
    }

    // The library's phrases, regenerated from their rows.
    std::vector<std::pair<LibraryRow, Phrase>> libraryPhrases (const PhraseSpec& spec, const PhraseLibrary& lib)
    {
        std::vector<std::pair<LibraryRow, Phrase>> out;
        for (const LibraryRow& r : lib.rows)
            out.emplace_back (r, generatePhrase (spec, r.kind, r.seed));
        return out;
    }

    // ------------------------------------------------------------------ tools/sound/denylist.py, for ASCII text
    // (as plugin/tests/DragOutTests.cpp ports it)

    bool isLower (char c) { return c >= 'a' && c <= 'z'; }
    bool isUpper (char c) { return c >= 'A' && c <= 'Z'; }
    bool isDigit (char c) { return c >= '0' && c <= '9'; }
    bool isAlpha (char c) { return isLower (c) || isUpper (c); }

    std::vector<std::string> splitWords (const std::string& text)
    {
        std::string camel;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (i > 0)
            {
                const char p = text[i - 1], c = text[i];
                if ((isLower (p) && isUpper (c)) || (isUpper (p) && isUpper (c) && i + 1 < text.size() && isLower (text[i + 1])))
                    camel += ' ';
            }
            camel += text[i];
        }
        std::string spaced;
        for (std::size_t i = 0; i < camel.size(); ++i)
        {
            if (i > 0 && ((isAlpha (camel[i - 1]) && isDigit (camel[i])) || (isDigit (camel[i - 1]) && isAlpha (camel[i]))))
                spaced += ' ';
            spaced += camel[i];
        }
        std::vector<std::string> words;
        std::string word;
        for (char c : spaced)
        {
            if (isUpper (c))
                c = static_cast<char> (c - 'A' + 'a');
            if (c == '\'')
                continue;
            if (isLower (c) || isDigit (c))
                word += c;
            else if (!word.empty())
            {
                words.push_back (word);
                word.clear();
            }
        }
        if (!word.empty())
            words.push_back (word);
        return words;
    }

    std::set<std::string> grams (const std::string& text)
    {
        const auto w = splitWords (text);
        std::set<std::string> out;
        for (std::size_t i = 0; i < w.size(); ++i)
        {
            std::string spaced, joined;
            for (std::size_t j = i; j < std::min (w.size(), i + 4); ++j)
            {
                spaced += (j > i ? " " : "") + w[j];
                joined += w[j];
                out.insert (spaced);
                out.insert (joined);
            }
        }
        return out;
    }

    std::set<std::string> loadDenylist (const char* name)
    {
        std::ifstream in (sourceDir / "tools" / "sound" / "denylists" / (std::string (name) + ".sha256"));
        REQUIRE (in.good());
        std::set<std::string> digests;
        std::string line;
        while (std::getline (in, line))
        {
            line = line.substr (0, line.find ('#'));
            while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\t'))
                line.pop_back();
            while (!line.empty() && line.front() == ' ')
                line.erase (0, 1);
            if (!line.empty())
                digests.insert (line);
        }
        REQUIRE (digests.size() > 10);
        return digests;
    }

    std::vector<std::string> denylistHits (const std::string& text, const std::set<std::string>& digests)
    {
        std::vector<std::string> hits;
        for (const auto& g : grams (text))
            if (digests.count (sha ("wubansia-denylist-v1:" + g)) != 0)
                hits.push_back (g);
        return hits;
    }
}

TEST_CASE ("phrase 1: the same kit, kind and seed give the same notes and the same file", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        for (std::size_t k = 0; k < phraseKindCount; ++k)
        {
            CAPTURE (phraseKindIds[k]);
            for (std::uint64_t seed = 0; seed < 16; ++seed)
            {
                const Phrase a = generatePhrase (spec, kindAt (k), seed);
                const Phrase b = generatePhrase (spec, kindAt (k), seed);
                CHECK (!a.notes.empty());
                CHECK (a.notes == b.notes);
                CHECK (a.form == b.form);
                CHECK (fileOf (spec, a, "Name") == fileOf (spec, b, "Name"));
            }
            CHECK (generatePhrase (spec, kindAt (k), 0).notes != generatePhrase (spec, kindAt (k), 1).notes);
        }
    }
}

TEST_CASE ("phrase 2: every library row regenerates to its hash", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        const PhraseLibrary lib = libraryOf (kit);
        CHECK (lib.kitId == kit);
        CHECK (static_cast<std::int32_t> (lib.rows.size()) == quotaSum (spec));
        CHECK (lib.rows.size() == 64);
        for (const auto& [row, p] : libraryPhrases (spec, lib))
        {
            CAPTURE (row.id);
            const auto bytes = fileOf (spec, p, row.name);
            CHECK (util::sha256Hex (bytes) == row.sha256);
            CHECK (row.id == phraseId (row.kind, row.seed));
            CHECK (row.name == phraseName (spec, row.kind, row.seed));
            const PhraseChecks c = checkPhrase (spec, p);
            CHECK (p.form == row.form);
            CHECK (c.notes == row.notes);
            CHECK (c.low == row.low);
            CHECK (c.high == row.high);
            CHECK (phraseTags (spec, p) == row.tags);
        }
    }
}

TEST_CASE ("phrase 3: the library file is a fresh choice", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        const std::string specText = readText (phraseSpecPath (contentDir, kit));
        const std::string libText = readText (phraseLibraryPath (contentDir, kit));
        CHECK (libraryJson (spec, specText, selectLibrary (spec)) == libText);
        CHECK (libraryOf (kit).specSha256 == sha (specText));
    }
}

TEST_CASE ("phrase 4: every library phrase passes every check", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        const PhraseSpec spec = specOf (kit);
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
        {
            CAPTURE (kit, row.id);
            const PhraseChecks c = checkPhrase (spec, p);
            CHECK (c.inKey);
            CHECK (c.inRange);
            CHECK (c.everyBar);
            CHECK (c.motif);
            CHECK (c.variation);
            CHECK (c.ending);
            CHECK (c.rhythm);
            CHECK (c.clean);
            CHECK (c.synth);
            CHECK (c.melody);
            CHECK (c.strongChords);
            CHECK (c.pass());
        }
    }
}

TEST_CASE ("phrase 5: on 400 seeds a kind, in key, in range, clean and synth-ready", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        const PhraseSpec spec = specOf (kit);
        for (std::size_t k = 0; k < phraseKindCount; ++k)
        {
            CAPTURE (kit, phraseKindIds[k]);
            std::int32_t inKey = 0, inRange = 0, clean = 0, synth = 0, pass = 0;
            for (std::uint64_t seed = 0; seed < 400; ++seed)
            {
                const PhraseChecks c = checkPhrase (spec, generatePhrase (spec, kindAt (k), seed));
                inKey += c.inKey ? 1 : 0;
                inRange += c.inRange ? 1 : 0;
                clean += c.clean ? 1 : 0;
                synth += c.synth ? 1 : 0;
                pass += c.pass() ? 1 : 0;
            }
            CHECK (inKey == 400);
            CHECK (inRange == 400);
            CHECK (clean == 400);
            CHECK (synth == 400);
            CHECK (pass >= 160);
        }
    }
}

TEST_CASE ("phrase 6: strong beats on chord tones", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        std::int64_t sum = 0, count = 0;
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
        {
            CAPTURE (row.id);
            const PhraseChecks c = checkPhrase (spec, p);
            REQUIRE (c.strong > 0);
            const std::int64_t pm = std::int64_t { c.strongTones } * 1000 / c.strong;
            CHECK (pm >= 800);
            sum += pm;
            ++count;
        }
        REQUIRE (count > 0);
        CHECK (sum / count >= 950);
    }
}

TEST_CASE ("phrase 7: a motif comes back, varied", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        const PhraseSpec spec = specOf (kit);
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
        {
            CAPTURE (kit, row.id);
            const PhraseChecks c = checkPhrase (spec, p);
            CHECK ((c.units == 2 || c.units == 3));
        }
    }
}

TEST_CASE ("phrase 8: a real ending", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        const PhraseSpec spec = specOf (kit);
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
        {
            CAPTURE (kit, row.id);
            CHECK (checkPhrase (spec, p).ending);
        }
    }
}

TEST_CASE ("phrase 9: no near-duplicates", "[gen][phrase]")
{
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        std::vector<Phrase> lib;
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
            lib.push_back (p);
        for (std::size_t i = 0; i < lib.size(); ++i)
            for (std::size_t j = i + 1; j < lib.size(); ++j)
                CHECK (phraseSimilarity (lib[i], lib[j]) < spec.nearDuplicate);
        for (std::size_t k = 0; k < phraseKindCount; ++k)
        {
            CAPTURE (phraseKindIds[k]);
            std::vector<Phrase> against = lib;
            std::uint64_t seed = firstMoreSeed;
            std::size_t more = 0;
            for (int press = 0; press < 3; ++press)
            {
                const MorePress m = morePhrases (spec, kindAt (k), seed, against);
                CHECK (m.phrases.size() == 8);
                CHECK (m.nextSeed > seed);
                seed = m.nextSeed;
                for (const Phrase& p : m.phrases)
                {
                    CHECK (p.seed >= firstMoreSeed);
                    CHECK (checkPhrase (spec, p).pass());
                    for (const Phrase& q : against)
                        CHECK (phraseSimilarity (p, q) < spec.nearDuplicate);
                    against.push_back (p);
                    ++more;
                }
            }
            CHECK (more == 24);
        }
    }
}

TEST_CASE ("phrase 10: the files are synth-ready", "[gen][phrase]")
{
    const std::map<std::string_view, std::pair<std::int8_t, std::int64_t>> keyAndSwing {
        { "FUT01", { 3, 43 } }, { "FUT02", { -1, 10 } }, { "FUT03", { -2, 29 } }, { "FUT04", { 1, 19 } } };
    for (const auto kit : kits)
    {
        const PhraseSpec spec = specOf (kit);
        const auto [sharps, swing] = keyAndSwing.at (kit);
        CHECK (spec.swing == swing);
        for (const auto& [row, p] : libraryPhrases (spec, libraryOf (kit)))
        {
            CAPTURE (kit, row.id);
            const auto bytes = fileOf (spec, p, row.name);
            const auto file = midi::readSmf (bytes);
            REQUIRE (file.ok());
            CHECK (file.value.format == 1);
            CHECK (file.value.ppq == 960);
            REQUIRE (file.value.tracks.size() == 2);
            CHECK (midi::trackName (file.value.tracks[0]) == std::string (kit) + " " + row.name);
            const auto tempos = midi::tempoMap (file.value);
            REQUIRE (tempos.size() == 1);
            CHECK (tempos[0].tick == 0);
            CHECK (tempos[0].microsPerQuarter == static_cast<std::uint32_t> ((60000000 * 2 + spec.bpm) / (2 * spec.bpm)));
            const auto sigs = midi::timeSignaturesOf (file.value);
            REQUIRE (sigs.size() == 1);
            CHECK ((sigs[0].tick == 0 && sigs[0].numerator == 4 && sigs[0].denominatorPow2 == 2));
            const auto keys = midi::keySignaturesOf (file.value);
            REQUIRE (keys.size() == 1);
            CHECK ((keys[0].tick == 0 && keys[0].sharps == sharps && keys[0].minor));
            const bool glass = kit == "FUT02" && row.kind == PhraseKind::period;
            CHECK (midi::trackName (file.value.tracks[1]) == (glass ? "Glass Ping" : "Lead"));
            const auto notes = midi::notesOf (file.value.tracks[1]);
            REQUIRE (!notes.empty());
            for (const auto& n : notes)
            {
                CHECK (n.channel == (glass ? 12 : 1));
                const std::int64_t at = n.tick % 480;
                CHECK ((at == 0 || at == 240 + swing));
                CHECK (n.tick + n.length <= 30720);
            }
            CHECK (file.value.tracks[1].endTick == 30720);
            const std::array<midi::SynthPart, 2> parts { midi::SynthPart::asIs, midi::SynthPart::pitched };
            const auto synth = midi::forSynths (bytes, parts);
            REQUIRE (synth.ok());
            const auto synthFile = midi::readSmf (synth.value);
            REQUIRE (synthFile.ok());
            const auto after = midi::notesOf (synthFile.value.tracks[1]);
            REQUIRE (after.size() == notes.size());
            for (std::size_t i = 0; i < notes.size(); ++i)
                CHECK ((after[i].tick == notes[i].tick && after[i].length == notes[i].length && after[i].key == notes[i].key));
        }
    }
}

TEST_CASE ("phrase 11: the phrase keys are permanent", "[gen][phrase]")
{
    CHECK (rng::tag ("phrase") == 0x86AA24BB6D57B1E8ull);
    const rng::Key k = phraseKey ("FUT01", PhraseKind::hook, 7);
    CHECK (k.h == 0xECF1ABCA4132DAF6ull);
    CHECK (k.with (rng::tag ("material")).h == 0x9C16409F2012D092ull);
}

TEST_CASE ("phrase 12: the loader refuses a broken spec", "[gen][phrase]")
{
    const std::string base = readText (fixtureDir / "base.json");
    {
        const auto ok = parsePhraseSpec (base, "base.json");
        INFO (ok.error);
        REQUIRE (ok.ok());
    }
    const std::string refusalsText = readText (fixtureDir / "refusals.json");
    const auto parsed = json::parse (refusalsText);
    REQUIRE (parsed.ok());
    const json::Value* cases = parsed.value.find ("cases");
    REQUIRE (cases != nullptr);
    REQUIRE (cases->items.size() >= 20);
    for (const json::Value& c : cases->items)
    {
        const auto field = [&c] (std::string_view k) { return c.find (k) != nullptr ? c.find (k)->string : std::string(); };
        CAPTURE (field ("why"));
        const std::string from = field ("from");
        const auto at = base.find (from);
        REQUIRE (at != std::string::npos);
        REQUIRE (base.find (from, at + 1) == std::string::npos);
        std::string text = base;
        text.replace (at, from.size(), field ("to"));
        const auto r = parsePhraseSpec (text, "fut01/FUT01_phrases.json");
        CAPTURE (r.error);
        REQUIRE (!r.ok());
        CHECK (r.error.find ("fut01/FUT01_phrases.json: ") == 0);
        CHECK (r.error.find (field ("path")) != std::string::npos);
        CHECK (r.error.find ("line ") != std::string::npos);
        CHECK (r.error.find (field ("says")) != std::string::npos);
    }

    SECTION ("loadPhraseSpec: the kit is the folder's, its tonic and bpm the kit file's")
    {
        const fs::path root = fs::path (WUBANSIA_TEST_TMP_DIR) / "phrase-loader";
        fs::remove_all (root);
        fs::create_directories (root / "kits");
        fs::create_directories (root / "phrases" / "fut01");
        fs::copy_file (contentDir / "kits" / "fut01.json", root / "kits" / "fut01.json");
        const auto write = [&root] (const std::string& text) {
            std::ofstream (root / "phrases" / "fut01" / "FUT01_phrases.json", std::ios::binary) << text;
        };
        const auto refused = [&] (const std::string& from, const std::string& to, const std::string& says) {
            std::string text = base;
            text.replace (text.find (from), from.size(), to);
            write (text);
            const auto r = loadPhraseSpec (root, "FUT01");
            CAPTURE (says, r.error);
            CHECK (!r.ok());
            CHECK (r.error.find ("FUT01_phrases.json") != std::string::npos);
            CHECK (r.error.find (says) != std::string::npos);
        };
        write (base);
        CHECK (loadPhraseSpec (root, "FUT01").ok());
        refused ("\"kit\": \"FUT01\"", "\"kit\": \"FUT02\"", "FUT02");
        refused ("\"tonic\": 6", "\"tonic\": 7", "tonic");
        refused ("\"bpm\": 134", "\"bpm\": 135", "bpm");
        fs::remove_all (root);
    }
}

TEST_CASE ("phrase 13: a cut stays out and the quota fills", "[gen][phrase]")
{
    const PhraseSpec spec = specOf ("FUT01");
    std::string text = readText (phraseSpecPath (contentDir, "FUT01"));
    std::string cut = readText (fixtureDir / "fut01-cut.json");
    while (!cut.empty() && (cut.back() == '\n' || cut.back() == '\r'))
        cut.pop_back();
    const auto at = text.find ("\"cut\": {}");
    REQUIRE (at != std::string::npos);
    text.replace (at, 9, cut);
    const auto cutSpec = parsePhraseSpec (text, "fut01-cut");
    INFO (cutSpec.error);
    REQUIRE (cutSpec.ok());

    const auto uncut = selectLibrary (spec);
    const auto rows = selectLibrary (cutSpec.value);
    std::map<std::string, LibraryRow> byId;
    for (const auto& r : rows)
        byId[r.id] = r;
    std::set<std::uint64_t> uncutHooks;
    for (const auto& r : uncut)
    {
        if (r.kind == PhraseKind::hook)
            uncutHooks.insert (r.seed);
        if (r.id == "hook-0" || r.id == "hook-1" || r.kind == PhraseKind::stabs)
            continue;
        CAPTURE (r.id);
        REQUIRE (byId.count (r.id) == 1);
        CHECK (byId[r.id].name == r.name);
        CHECK (byId[r.id].sha256 == r.sha256);
    }
    CHECK (byId.count ("hook-0") == 0);
    CHECK (byId.count ("hook-1") == 0);
    std::int32_t hooks = 0, fresh = 0, stabs = 0;
    for (const auto& r : rows)
    {
        hooks += r.kind == PhraseKind::hook ? 1 : 0;
        fresh += r.kind == PhraseKind::hook && uncutHooks.count (r.seed) == 0 ? 1 : 0;
        stabs += r.kind == PhraseKind::stabs ? 1 : 0;
    }
    CHECK (hooks == 12);
    CHECK (fresh == 2);
    CHECK (stabs == 0);
    CHECK (rows.size() == uncut.size() - 8);
}

TEST_CASE ("phrase 13a: names come from kind and seed", "[gen][phrase]")
{
    const PhraseSpec spec = specOf ("FUT01");
    for (std::size_t i = 0; i < 576; ++i)
        CHECK (phraseNameAt (spec, i) == spec.words[0][i % 24] + " " + spec.words[1][(7 * i + i / 24) % 24]);
    CHECK (phraseName (spec, PhraseKind::hook, 0) == phraseNameAt (spec, 0));
    CHECK (phraseName (spec, PhraseKind::hook, 47) == phraseNameAt (spec, 47));
    CHECK (phraseName (spec, PhraseKind::hook, 48) == phraseNameAt (spec, 0) + " 2");
    CHECK (phraseName (spec, PhraseKind::hook, 96) == phraseNameAt (spec, 0) + " 3");
    CHECK (phraseName (spec, PhraseKind::hook, firstMoreSeed) == phraseNameAt (spec, 48));
    CHECK (phraseName (spec, PhraseKind::hook, firstMoreSeed + 24) == phraseNameAt (spec, 48) + " 2");
    CHECK (phraseName (spec, PhraseKind::answer, 5) == phraseNameAt (spec, 72 + 5));
    CHECK (phraseName (spec, PhraseKind::bounce, firstMoreSeed + 3) == phraseNameAt (spec, 7 * 72 + 48 + 3));
    CHECK (phraseId (PhraseKind::counter, 12) == "counter-12");
    const auto id = parsePhraseId ("counter-12");
    REQUIRE (id.has_value());
    CHECK ((id->first == PhraseKind::counter && id->second == 12));
    CHECK (!parsePhraseId ("chord-1").has_value());
}

TEST_CASE ("phrase 14: names are original and unique", "[gen][phrase]")
{
    const auto artists = loadDenylist ("artists");
    const auto slurs = loadDenylist ("slurs");
    const auto profanity = loadDenylist ("profanity");
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        std::set<std::string> all;
        for (std::size_t i = 0; i < 576; ++i)
            all.insert (phraseNameAt (spec, i));
        CHECK (all.size() == 576);
        for (const auto& [id, name] : spec.rename)
            all.insert (name);
        for (const auto& name : all)
        {
            CAPTURE (name);
            CHECK (denylistHits (name, artists).empty());
            CHECK (denylistHits (name, slurs).empty());
            CHECK (denylistHits (name, profanity).empty());
        }
        std::set<std::string> names;
        for (const auto& r : libraryOf (kit).rows)
            CHECK (names.insert (r.name).second);
    }
}

#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(WUBANSIA_NO_TIMING)
TEST_CASE ("phrase 15: fast enough for a worker thread", "[gen][phrase][timing]")
{
    using clock = std::chrono::steady_clock;
    for (const auto kit : kits)
    {
        CAPTURE (kit);
        const PhraseSpec spec = specOf (kit);
        const PhraseLibrary lib = libraryOf (kit);
        auto t0 = clock::now();
        std::vector<Phrase> phrases;
        for (const auto& r : lib.rows)
            phrases.push_back (generatePhrase (spec, r.kind, r.seed));
        CHECK (std::chrono::duration_cast<std::chrono::milliseconds> (clock::now() - t0).count() < 100);
        for (std::size_t k = 0; k < phraseKindCount; ++k)
        {
            CAPTURE (phraseKindIds[k]);
            t0 = clock::now();
            const MorePress m = morePhrases (spec, kindAt (k), firstMoreSeed, phrases);
            CHECK (m.phrases.size() == 8);
            CHECK (std::chrono::duration_cast<std::chrono::milliseconds> (clock::now() - t0).count() < 100);
        }
    }
}
#endif
