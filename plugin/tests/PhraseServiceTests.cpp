// The Phrases drawer's thread (ADR 0031, services/PhraseService.h): the lists it makes from the embedded kits (the 64
// library phrases, the kit's own files, the kept and More phrases), More and Keep in a settings file of the test's own,
// the files it gives a drag, and the auditions it plays through the real processor on the synthetic kit: offered,
// stopped, replaced, a kit change, the clips deleted on its thread, and a host that runs no audio (the 300 ms
// withdraw, the idle give-up and a late wake). The audition cases join the TSan subset: WubansiaPluginTests
// "[phrases]" in a -fsanitize=thread build.
#include "content/ContentLocator.h"
#include "content/InstalledContent.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "helpers/test_helpers.h"
#include "services/KitCatalog.h"
#include "services/PhraseService.h"

#include "gen/phrase.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <set>
#include <string>
#include <thread>

using namespace wubansia;
using namespace std::chrono_literals;

namespace
{
    // A settings file of the test's own, deleted with it.
    struct TempSettings
    {
        TempSettings()
            : file (juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("WubansiaPhraseSettings " + juce::Uuid().toString(), ".xml", false)),
              settings (file)
        {
        }
        ~TempSettings() { file.deleteFile(); }
        juce::File file;
        UserSettings settings;
    };

    // The four embedded kits, no samples anywhere, and a phrase service over them on an engine that runs no block.
    struct Embedded
    {
        Embedded() : content (ContentLocator::Places {}, temp.settings), catalog (content.kits()), service (engine, [this]() -> const KitCatalog& { return catalog; })
        {
            service.setUserSettings (&temp.settings);
        }

        // Shows kit and waits for its list.
        std::shared_ptr<const PhraseList> listOf (const juce::String& kit)
        {
            const auto before = service.generation();
            service.show (kit);
            const auto shown = [&] {
                const auto l = service.list();
                return l != nullptr && l->kit == kit && service.generation() > before;
            };
            REQUIRE (test::waitFor (shown, 10s));
            return service.list();
        }

        TempSettings temp;
        InstalledContent content;
        KitCatalog catalog;
        engine::Engine engine;
        PhraseService service;
    };

    std::vector<PhraseInfo> ofOrigin (const PhraseList& list, PhraseOrigin origin)
    {
        std::vector<PhraseInfo> out;
        for (const auto& p : list.phrases)
            if (p.origin == origin)
                out.push_back (p);
        return out;
    }

    const PhraseInfo* findId (const PhraseList& list, const juce::String& id)
    {
        for (const auto& p : list.phrases)
            if (p.id == id)
                return &p;
        return nullptr;
    }

    std::string str (const juce::String& s) { return s.toStdString(); }

    // A processor on the synthetic kit, its kit loaded, the host stopped until a test plays it.
    struct Quiet
    {
        explicit Quiet (const char* id) : kit (id), plugin (withKit (id))
        {
            plugin->setPlayHead (&head);
            test::prepareWithKit (*plugin, 48000.0, 512);
        }

        // The host plays from ppq 0, and a block has said so.
        void play()
        {
            head.play (120.0, 0.0);
            blocks (1);
        }
        ~Quiet() { plugin->setPlayHead (nullptr); }

        static test::SyntheticContent::Options withKit (const char* id)
        {
            test::SyntheticContent::Options o;
            o.ids = { id };
            return o;
        }

        PluginProcessor& operator*() { return *plugin; }
        Controller& controller() { return plugin->controller(); }
        juce::String own() const { return kit + "_Phrase_Lead_2bars.mid"; } // the synthetic kit's own phrase file
        std::uint32_t sounding() { return plugin->telemetry().phraseSounding.load(); }
        void blocks (int n)
        {
            for (int i = 0; i < n; ++i)
            {
                test::process (*plugin, 512);
                head.advance (512, 48000.0);
            }
        }

        juce::String kit;
        test::SyntheticPlugin plugin;
        test::TestPlayHead head;
    };
}

TEST_CASE ("nothing starts until the drawer opens", "[phrases]")
{
    Quiet q ("PHR01");
    auto& c = q.controller();
    q.blocks (4);
    (void) c.phrasesGeneration(); // the editor's tick asks this, drawer shut or not
    (void) c.phraseAuditionState();
    CHECK_FALSE ((*q).phraseService().started());

    (void) c.phrases(); // the drawer opens
    CHECK ((*q).phraseService().started());
}

TEST_CASE ("each kit lists its 64 library phrases and its own", "[phrases]")
{
    Embedded e;
    CHECK_FALSE (e.service.started());
    const std::map<std::string, std::size_t> ownCount { { "FUT01", 6 }, { "FUT02", 5 }, { "FUT03", 5 }, { "FUT04", 5 } };
    for (const auto& entry : e.catalog.entries())
    {
        INFO (entry.id);
        REQUIRE (entry.phraseSpec.has_value());
        const auto list = e.listOf (entry.id);
        CHECK (e.service.started());

        // The library, row for row, in its order: id, name, kind, seed, tags; 8 bars on the lead, with its roll.
        const auto library = ofOrigin (*list, PhraseOrigin::library);
        REQUIRE (library.size() == 64);
        REQUIRE (entry.phraseRows.size() == 64);
        const auto leadLabel = entry.card.laneLabels[(std::size_t) engine::Lane::lead];
        for (std::size_t i = 0; i < library.size(); ++i)
        {
            const auto& row = entry.phraseRows[i];
            const auto& p = library[i];
            CHECK (str (p.id) == row.id);
            CHECK (str (p.name) == row.name);
            CHECK (str (p.kind) == std::string (gen::phraseKindIds[(std::size_t) row.kind]));
            CHECK (p.seed == row.seed);
            CHECK (p.bars == 8);
            CHECK (p.tags.size() == (int) row.tags.size());
            CHECK ((int) p.roll.size() == row.notes);
            CHECK_FALSE (p.kept);
            CHECK_FALSE (p.fresh);
            for (const auto& n : p.roll)
                CHECK ((n.step < 128 && n.len16 >= 1 && n.key >= row.low && n.key <= row.high));
            const bool glass = entry.id == "FUT02" && row.kind == gen::PhraseKind::period;
            if (glass)
            {
                CHECK (p.sound == "Glass Ping"); // FUT02's periods, on the accents
                CHECK (p.lanes == engine::laneBit (engine::Lane::accents));
            }
            else
            {
                CHECK (p.sound == leadLabel);
                CHECK (p.lanes == engine::laneBit (engine::Lane::lead));
            }
        }

        // Then the kit's own phrase files and its lead line, as kitPhraseFiles lists them.
        const auto own = ofOrigin (*list, PhraseOrigin::kit);
        const auto files = dragout::kitPhraseFiles (entry.id, entry.midiFiles);
        REQUIRE (own.size() == ownCount.at (entry.id));
        REQUIRE ((int) own.size() == files.size());
        for (std::size_t i = 0; i < own.size(); ++i)
        {
            CHECK (own[i].id == files[(int) i]);
            CHECK (own[i].kind == "kit");
            CHECK ((own[i].bars == 2 || own[i].bars == 4));
            CHECK_FALSE (own[i].roll.empty());
            CHECK (own[i].lanes != 0u);
            CHECK (own[i].sound.isNotEmpty());
        }
        CHECK (list->phrases.size() == 64 + own.size()); // nothing kept, no More yet
    }

    // FUT01's Warp Riff plays on the bass lane, and its lead line on the lead.
    const auto fut01 = e.listOf ("FUT01");
    const auto* warp = findId (*fut01, "FUT01_Phrase_WarpRiff_2bars.mid");
    REQUIRE (warp != nullptr);
    CHECK (warp->lanes == engine::laneBit (engine::Lane::bass));
    const auto* lead = findId (*fut01, "FUT01_Lead_ChromePluck_2bars.mid");
    REQUIRE (lead != nullptr);
    CHECK (lead->lanes == engine::laneBit (engine::Lane::lead));
    CHECK (lead->name == "Lead Chrome Pluck");
}

TEST_CASE ("More adds 8 phrases unlike the list and saves where it stopped", "[phrases]")
{
    Embedded e;
    const auto before = e.listOf ("FUT01");
    const auto* entry = e.catalog.find ("FUT01");
    REQUIRE ((entry != nullptr && entry->phraseSpec.has_value()));
    const auto& spec = *entry->phraseSpec;
    CHECK (e.temp.settings.phrasesNextSeed ("FUT01") == gen::firstMoreSeed);

    CHECK_FALSE (e.service.more ("FUT01", "nonsense"));
    const auto generation = e.service.generation();
    REQUIRE (e.service.more ("FUT01", "arp"));
    CHECK_FALSE (e.service.more ("FUT01", "arp")); // one press at a time
    REQUIRE (test::waitFor ([&] { return ! e.service.moreInFlight() && e.service.generation() > generation; }, 10s));
    const auto after = e.service.list();
    REQUIRE (after->phrases.size() == before->phrases.size() + 8);
    CHECK (e.service.lastMore().made == 8);
    CHECK (e.service.lastMore().kind == "arp");

    // At the end of the list: arps of More's seeds, new, each passing every check and unlike everything listed.
    std::vector<gen::Phrase> listed;
    for (const auto& p : before->phrases)
        if (p.origin == PhraseOrigin::library)
            listed.push_back (gen::generatePhrase (spec, *gen::phraseKindFromId (str (p.kind)), p.seed));
    std::uint64_t highest = 0;
    for (std::size_t i = before->phrases.size(); i < after->phrases.size(); ++i)
    {
        const auto& p = after->phrases[i];
        INFO (p.id);
        CHECK (p.origin == PhraseOrigin::more);
        CHECK (p.kind == "arp");
        CHECK (p.fresh);
        CHECK (p.seed >= gen::firstMoreSeed);
        CHECK (str (p.id) == gen::phraseId (gen::PhraseKind::arp, p.seed));
        CHECK (str (p.name) == gen::phraseName (spec, gen::PhraseKind::arp, p.seed));
        const auto phrase = gen::generatePhrase (spec, gen::PhraseKind::arp, p.seed);
        CHECK (gen::checkPhrase (spec, phrase).pass());
        for (const auto& other : listed)
            CHECK (gen::phraseSimilarity (phrase, other) < spec.nearDuplicate);
        listed.push_back (phrase);
        highest = std::max (highest, p.seed);
    }
    CHECK (e.temp.settings.phrasesNextSeed ("FUT01") > highest); // where the next press starts, kept per user

    // Under All: one of each kind.
    const auto second = e.service.generation();
    REQUIRE (e.service.more ("FUT01", ""));
    REQUIRE (test::waitFor ([&] { return ! e.service.moreInFlight() && e.service.generation() > second; }, 10s));
    const auto all = e.service.list();
    REQUIRE (all->phrases.size() == after->phrases.size() + 8);
    std::set<juce::String> kinds;
    for (std::size_t i = after->phrases.size(); i < all->phrases.size(); ++i)
        kinds.insert (all->phrases[i].kind);
    CHECK (kinds.size() == 8);

    // Played or dragged, a New badge goes.
    const auto seen = all->phrases.back().id;
    const auto third = e.service.generation();
    e.service.markSeen ("FUT01", seen);
    REQUIRE (test::waitFor ([&] { return e.service.generation() > third; }, 5s));
    CHECK_FALSE (findId (*e.service.list(), seen)->fresh);
}

TEST_CASE ("a kept phrase is listed by a new service (settings.xml)", "[phrases]")
{
    TempSettings temp;
    juce::String kept, keptName;
    {
        Embedded e;
        e.service.setUserSettings (&temp.settings);
        (void) e.listOf ("FUT01");
        const auto generation = e.service.generation();
        REQUIRE (e.service.more ("FUT01", "bounce"));
        REQUIRE (test::waitFor ([&] { return ! e.service.moreInFlight() && e.service.generation() > generation; }, 10s));
        const auto& more = e.service.list()->phrases.back();
        kept = more.id;
        keptName = more.name;
        const auto g2 = e.service.generation();
        e.service.setKept ("FUT01", kept, true);
        e.service.setKept ("FUT01", "hook-0", true);
        REQUIRE (test::waitFor ([&] { return e.service.generation() > g2 && findId (*e.service.list(), kept)->kept; }, 5s));
        CHECK (findId (*e.service.list(), "hook-0")->kept);
    }
    CHECK (temp.settings.keptPhrases ("FUT01") == juce::StringArray { kept, "hook-0" });

    // A new process: the More phrase it kept comes back, regenerated, with its name; the library's star stays.
    Embedded e;
    e.service.setUserSettings (&temp.settings);
    const auto list = e.listOf ("FUT01");
    const auto* again = findId (*list, kept);
    REQUIRE (again != nullptr);
    CHECK (again->kept);
    CHECK (again->origin == PhraseOrigin::more);
    CHECK (again->name == keptName);
    CHECK_FALSE (again->fresh); // New is this session's More only
    CHECK (findId (*list, "hook-0")->kept);
    CHECK (list->phrases.size() == 64 + 6 + 1);

    // The star off: gone from the file, and the More phrase from the list.
    const auto g = e.service.generation();
    e.service.setKept ("FUT01", kept, false);
    REQUIRE (test::waitFor ([&] { return e.service.generation() > g; }, 5s));
    CHECK (findId (*e.service.list(), kept) == nullptr);
    CHECK (temp.settings.keptPhrases ("FUT01") == juce::StringArray { "hook-0" });
}

TEST_CASE ("a kept phrase that was cut from the library still lists under Kept", "[phrases]")
{
    // FUT01 as embedded, but its spec cuts hook seed 0 and its library is chosen again: hook-0 leaves the library.
    TempSettings temp;
    InstalledContent embedded (ContentLocator::Places {}, temp.settings);
    const auto& fut01 = embedded.kits().front();
    REQUIRE (fut01.id == "FUT01");
    auto specText = std::string (fut01.phraseSpec);
    const auto cutAt = specText.find ("\"cut\": {}");
    REQUIRE (cutAt != std::string::npos);
    specText.replace (cutAt, 9, "\"cut\": {\"hook\": [0]}");
    const auto spec = gen::parsePhraseSpec (specText, "FUT01_phrases.json (cut)");
    REQUIRE (spec.ok());
    const auto libraryText = gen::libraryJson (spec.value, specText, gen::selectLibrary (spec.value));
    std::vector<KitSource> sources (embedded.kits().begin(), embedded.kits().end());
    sources.front().phraseSpec = specText;
    sources.front().phraseLibrary = libraryText;
    const KitCatalog catalog (sources);
    REQUIRE (catalog.find ("FUT01")->phraseRows.size() == 64);
    for (const auto& row : catalog.find ("FUT01")->phraseRows)
        CHECK (row.id != "hook-0");

    REQUIRE (temp.settings.setPhraseKept ("FUT01", "hook-0", true));
    engine::Engine engine;
    PhraseService service (engine, [&]() -> const KitCatalog& { return catalog; });
    service.setUserSettings (&temp.settings);
    service.show ("FUT01");
    REQUIRE (test::waitFor ([&] { return service.list() != nullptr; }, 10s));
    const auto list = service.list();
    const auto* kept = findId (*list, "hook-0");
    REQUIRE (kept != nullptr);
    CHECK (kept->kept);
    CHECK (str (kept->name) == gen::phraseName (spec.value, gen::PhraseKind::hook, 0));
    CHECK (kept->roll.size() == gen::generatePhrase (spec.value, gen::PhraseKind::hook, 0).notes.size());
    CHECK (list->phrases.size() == 64 + 6 + 1);
    CHECK (service.file ("FUT01", "hook-0").ok()); // and it drags
}

TEST_CASE ("file () gives the row's bytes", "[phrases]")
{
    Embedded e;
    for (const auto& entry : e.catalog.entries())
    {
        INFO (entry.id);
        for (std::size_t i = 0; i < entry.phraseRows.size(); i += 9)
        {
            const auto& row = entry.phraseRows[i];
            const auto file = e.service.file (entry.id, row.id);
            REQUIRE (file.ok());
            CHECK (util::sha256Hex (file.value.bytes) == row.sha256);
            CHECK (file.value.name == row.name);
            CHECK (file.value.kindWord == std::string (gen::phraseKindFileWord (row.kind)));
        }
        CHECK_FALSE (e.service.file (entry.id, "hook-99999").ok()); // not listed, not kept, not More's
    }
    CHECK_FALSE (e.service.file ("FUT01", "nonsense").ok());
    CHECK_FALSE (e.service.started()); // a drag starts no thread
}

TEST_CASE ("an audition offers a clip, Stop stops it, the latest wins", "[phrases]")
{
    STATIC_CHECK (PhraseService::kUntakenLimit == 300ms);
    STATIC_CHECK (PhraseService::kDrainInterval == 50ms);
    Quiet q ("PHR02");
    auto& c = q.controller();
    const auto& service = (*q).phraseService();
    q.play(); // the host plays: no Preview

    CHECK (c.auditionPhrase ("PHR02_nothing.mid") == 0u);
    const auto first = c.auditionPhrase (q.own());
    REQUIRE (first > 0u);
    CHECK (c.phraseAuditionState().latest == first);
    CHECK (c.phraseAuditionState().id == q.own());
    CHECK (c.phraseAuditionState().waiting);
    CHECK_FALSE (c.preview());
    REQUIRE (test::waitFor ([&] { q.blocks (1); return q.sounding() == first; }));

    // The latest wins: the first goes back to the service, the second plays.
    const auto second = c.auditionPhrase (q.own());
    REQUIRE (second > first);
    REQUIRE (test::waitFor ([&] { q.blocks (1); return q.sounding() == second; }));
    REQUIRE (test::waitFor ([&] { q.blocks (1); return c.phraseAuditionState().sounding == second; }, 10s));
    CHECK_FALSE (c.phraseAuditionState().waiting);
    CHECK (c.phraseAuditionState().startsAtBar >= 1);

    // Stop: it goes at the next block.
    c.stopPhraseAudition();
    CHECK (c.phraseAuditionState().sounding == 0u);
    q.blocks (2);
    CHECK (q.sounding() == 0u);
    CHECK (test::waitFor ([&] { return service.clipsMade() == 2 && service.clipsDeleted() == 2; }));
}

TEST_CASE ("the clips the engine lets go are deleted on the service's thread", "[phrases]")
{
    Quiet q ("PHR03");
    auto& c = q.controller();
    const auto& service = (*q).phraseService();
    q.play();
    for (int i = 0; i < 12; ++i)
    {
        REQUIRE (c.auditionPhrase (q.own()) > 0u);
        q.blocks (1 + i % 3);
        if (i % 4 == 3)
            c.stopPhraseAudition();
    }
    c.stopPhraseAudition();
    REQUIRE (test::waitFor ([&] { q.blocks (1); return service.clipsMade() > 0 && service.clipsMade() == service.clipsDeleted(); }, 10s));
    CHECK (q.sounding() == 0u);
}

TEST_CASE ("auditions and stops on the message thread while the audio thread runs: every clip is made and deleted once", "[phrases]")
{
    Quiet q ("PHR04");
    auto& c = q.controller();
    const auto& service = (*q).phraseService();
    q.play();
    std::atomic<bool> done { false };
    std::thread audio ([&] {
        while (! done)
            q.blocks (1);
    });
    for (int i = 0; i < 200; ++i)
    {
        if (i % 7 == 6)
            c.stopPhraseAudition();
        else
            (void) c.auditionPhrase (q.own());
        (void) c.phraseAuditionState();
        std::this_thread::sleep_for (1ms);
    }
    c.stopPhraseAudition();
    REQUIRE (test::waitFor ([&] { return service.clipsMade() == service.clipsDeleted(); }, 10s));
    done = true;
    audio.join();
}

TEST_CASE ("a kit change stops the audition", "[phrases]")
{
    test::SyntheticContent::Options two;
    two.ids = { "PHR05", "PHR06" };
    test::SyntheticPlugin plugin (two);
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, 48000.0, 512);
    head.play (120.0, 0.0);
    test::process (*plugin, 512);
    auto& c = plugin->controller();
    const auto ticket = c.auditionPhrase ("PHR05_Phrase_Lead_2bars.mid");
    REQUIRE (ticket > 0u);
    REQUIRE (test::waitFor ([&] { test::process (*plugin, 512); return plugin->telemetry().phraseSounding.load() == ticket; }));
    c.selectKit ("PHR06");
    CHECK (c.phraseAuditionState().sounding == 0u);
    test::process (*plugin, 512);
    CHECK (plugin->telemetry().phraseSounding.load() == 0u);
    plugin->setPlayHead (nullptr);
}

TEST_CASE ("an offer no block takes in 300 ms is withdrawn, and the drawer's state says the host is silent", "[phrases]")
{
    Quiet q ("PHR07");
    auto& c = q.controller();
    const auto asked = std::chrono::steady_clock::now();
    const auto ticket = c.auditionPhrase (q.own());
    REQUIRE (ticket > 0u);
    CHECK (c.preview()); // nothing played: Preview starts for it
    CHECK (c.phraseAuditionState().startedTransport);

    // No block runs (Logic, a stopped track that isn't selected): the offer goes, the hint says why, Preview goes off.
    REQUIRE (test::waitFor ([&] { return c.phraseAuditionState().hostSilent; }, 3000ms));
    CHECK (std::chrono::steady_clock::now() - asked >= PhraseService::kUntakenLimit);
    const auto state = c.phraseAuditionState();
    CHECK (state.latest == ticket);
    CHECK_FALSE (state.waiting);
    CHECK (state.sounding == 0u);
    CHECK_FALSE (state.startedTransport);
    CHECK_FALSE (c.preview());

    // Blocks come back: nothing stale plays.
    q.blocks (8);
    CHECK (q.sounding() == 0u);
    CHECK (c.phraseAuditionState().hostSilent); // until the next audition
    CHECK (c.auditionPhrase (q.own()) > ticket);
    CHECK_FALSE (c.phraseAuditionState().hostSilent);
}

TEST_CASE ("a held clip is given up when blocks stop", "[phrases]")
{
    Quiet q ("PHR08");
    auto& c = q.controller();
    const auto& service = (*q).phraseService();
    const auto ticket = c.auditionPhrase (q.own());
    REQUIRE (ticket > 0u);
    CHECK (c.preview());
    REQUIRE (test::waitFor ([&] { q.blocks (1); return q.sounding() == ticket; }));
    q.blocks (2); // then no block

    REQUIRE (test::waitFor ([&] { return c.phraseAuditionState().hostSilent; }, 3000ms));
    CHECK (service.clipsGivenUp() == 1);
    CHECK (c.phraseAuditionState().sounding == 0u);
    CHECK_FALSE (c.preview()); // the Preview the audition started is off

    // Blocks come back: the clip goes at the first, unplayed.
    q.blocks (1);
    CHECK (q.sounding() == 0u);
    CHECK (test::waitFor ([&] { return service.clipsMade() == service.clipsDeleted(); }));
}

TEST_CASE ("a late wake past both limits gives up the held clip and withdraws the offer", "[phrases]")
{
    struct Hold
    {
        std::atomic<bool> armed { false }, held { false }, released { false };
        void operator()()
        {
            if (! armed.exchange (false))
                return;
            held = true;
            const auto until = std::chrono::steady_clock::now() + 5s;
            while (! released && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for (1ms);
        }
    };
    const auto wake = std::make_shared<Hold>();
    Quiet q ("PHR09");
    (*q).setPhraseWakeProbeForTesting ([wake] { (*wake)(); });
    struct Release
    {
        std::shared_ptr<Hold> wake;
        ~Release() { wake->released = true; }
    } release { wake };

    auto& c = q.controller();
    const auto& service = (*q).phraseService();
    q.play();
    const auto stale = c.auditionPhrase (q.own());
    REQUIRE (test::waitFor ([&] { q.blocks (1); return q.sounding() == stale; }));
    q.blocks (1); // then no block

    // A newer phrase is offered; the thread's next wake comes late, after both limits have passed.
    wake->armed = true;
    const auto fresh = c.auditionPhrase (q.own());
    REQUIRE (fresh > stale);
    REQUIRE (test::waitFor ([&] { return wake->held.load(); }, 3000ms));
    std::this_thread::sleep_for (400ms);
    CHECK (service.clipsGivenUp() == 0);
    wake->released = true;

    REQUIRE (test::waitFor ([&] { return service.clipsGivenUp() == 1 && c.phraseAuditionState().hostSilent; }, 3000ms));
    const auto state = c.phraseAuditionState();
    CHECK (state.latest == fresh);
    CHECK (state.sounding == 0u);
    CHECK_FALSE (state.waiting);

    // The host comes back: neither phrase plays.
    q.blocks (4);
    CHECK (q.sounding() == 0u);
    CHECK (test::waitFor ([&] { return service.clipsMade() == service.clipsDeleted(); }));
}

TEST_CASE ("a bypassed block drops the offer", "[phrases]")
{
    Quiet q ("PHR10");
    auto& c = q.controller();
    q.play();
    const auto ticket = c.auditionPhrase (q.own());
    REQUIRE (ticket > 0u);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    REQUIRE (test::waitFor ([&] { return (*q).phraseOffered(); }));
    (*q).processBlockBypassed (buffer, midi);
    q.blocks (2);
    CHECK (q.sounding() == 0u); // the offer went back unplayed
}
