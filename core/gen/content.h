#pragma once

// The generator's content: a kit (content/kits/<kit>.json), the style it names
// (content/styles/<style>.json) and its feel template (content/feels/<feel>.json). These structs
// are what the loaders read and validate; the generator reads nothing else, so a direction is data,
// never code (no style names in core/gen's logic). The kit JSON has a schema,
// content/kits/schema.json; the style and feel JSON have none yet: the field comments on the
// structs below document them, and the loaders (content.cpp) enforce them.
//
// Everything is an integer: probabilities in percent (converted to p16 = pct * 65536 / 100 exactly),
// swing and scales in permille, times in ticks at 480 PPQ (120 per 16th), velocities 1-127.
// A grid is 2 bars of 16ths (32 steps); the JSON may give 16 values, meaning both bars alike.

#include "gen/theory.h"
#include "util/result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::gen::json
{
    class Node;
}

namespace wubansia::gen
{
    inline constexpr std::size_t gridSteps = 32; // 2 bars of 16ths
    inline constexpr std::size_t barSteps = 16;
    using Grid = std::array<std::int32_t, gridSteps>;
    using BarGrid = std::array<std::int32_t, barSteps>;

    // The section kinds of an arrangement (plan §2.3), in no particular order.
    inline constexpr std::array<std::string_view, 7> sectionKinds = { "intro", "groove", "build", "drop", "break", "drop2", "outro" };

    struct Range
    {
        std::int32_t lo = 0;
        std::int32_t hi = 0;
    };

    struct Weighted
    {
        std::int32_t value = 0;
        std::int32_t weight = 0;
    };

    // ------------------------------------------------------------------ kit

    struct KitVoiceNote
    {
        std::string voice;
        std::uint8_t note = 0; // GM drum note, 35-59 (plan §2.6)
    };

    struct KitLane
    {
        std::string id; // a lane of the style
        std::string track; // the MIDI track name
        std::uint8_t channel = 0; // 0-15
        std::vector<KitVoiceNote> notes; // drum lanes: one per voice of the style's lane
        Range range { 0, 127 }; // pitched lanes: the lowest and highest note it may play
        std::uint32_t rangeLine = 0; // the kit file's line of "range" (0: none given), for makeBundle's refusals
        std::int32_t root = -1; // bass: the tonic in the bass register (its lowest root)
    };

    struct Kit
    {
        std::string id; // "FUT01"
        std::string name; // "Alloy Signal"
        std::string direction;
        std::int32_t bpm = 0; // 128-140
        std::int32_t tonic = 0; // pitch class, C = 0
        bool minor = true;
        std::string keyName; // "F# minor", checked against tonic and mode
        std::string camelot; // "11A", checked too
        std::int32_t swing = 500; // the groove's base swing, permille (500 straight)
        std::string style;
        std::string feel;
        std::vector<KitLane> lanes;

        const KitLane* lane (std::string_view laneId) const noexcept;
    };

    // ------------------------------------------------------------------ style

    enum class LaneKind : std::uint8_t {
        drums, // voices on GM notes from probability grids, Euclidean or polymeter cells
        bass, // a line on chord degrees that may follow a drum lane and locks to it
        chords, // stabs or pads voiced from the harmony
        arp, // a cell of chord-tone indices played on a 16th grid, following the chords (arp.h)
        lead // the hook's carrier: the style's hook block plays on it (hook.h, lead.h)
    };

    struct SectionSpec
    {
        std::string kind;
        std::int32_t energy = 1; // 1-4, reserved: S1b2's syncopation bands per energy level (plan §4.1) read it; part 1 doesn't
        std::int32_t vel = 1000; // the section's score velocity scale, permille
        std::int32_t variant = 0; // the pattern variant every lane starts a section of this kind on, unless its form slot gives its own (FormSlot::variant)
    };

    // ---- the form grammar (arrange.h): every seed draws its own extended mix from it

    struct LaneWeight
    {
        std::string lane;
        std::int32_t weight = 0;
    };

    struct LaneGroup
    {
        std::vector<std::string> lanes;
        std::int32_t weight = 0;
    };

    // One slot of the form, in order: a section kind and the lengths it may take, in bars (multiples
    // of 8), by weight. A length of 0 leaves the slot out. A slot may start its section on its own
    // pattern variant (0-15), so a kind that comes back (the groove after drop 1) plays new patterns
    // on its grids instead of the first one's opening bars.
    struct FormSlot
    {
        std::string kind;
        std::vector<Weighted> bars;
        std::int32_t variant = -1; // -1: the kind's ("sections")
    };

    // A whole 32-bar mix-in or mix-out: the lanes of each of its 4 blocks.
    struct BlockChoice
    {
        std::int32_t weight = 0;
        std::vector<std::vector<std::string>> blocks;
    };

    // Lanes that play only part of a section, the part drawn per section from bars (multiples of 8):
    enum class Entry : std::uint8_t {
        after, // they join after this many bars (0: from the start; the section's length or more: they sit it out)
        last, // they play only its last this many bars (its length or more: all of it)
        before // they leave this many bars before its end (0: they stay; its length or more: they sit it out)
    };

    struct LaneEntry
    {
        std::vector<std::string> lanes;
        Entry entry = Entry::after;
        std::vector<Weighted> bars;
    };

    // A lane out for `bars` bars inside a section (a drop's lane move), with chance p per section: one
    // lane drawn by weight, out from a multiple of `bars` (never the first) to `bars` later. A drum
    // lane never rests through the section's last block: it comes back inside the section (a lane
    // that can't rest there weighs 0).
    struct RestSpec
    {
        std::int32_t p = 0; // percent
        std::int32_t bars = 8; // 8 or 16
        std::vector<LaneWeight> lanes;
    };

    // A one-bar dropout: the bar before each of the section's own lines every `every` bars (16 or 32)
    // inside it, never its last bar, with chance p per line: a lane group drawn by weight sits it out,
    // its lanes that play there (a group none of whose lanes plays there, or with a lane that leaves
    // on the line, isn't drawn), so they all land back on the line. A section's pulls come at least
    // 32 bars apart (a line closer to the last pull isn't drawn).
    struct DropoutSpec
    {
        std::int32_t p = 0; // percent
        std::int32_t every = 16;
        std::vector<LaneGroup> lanes;
    };

    // How a section kind's lanes play across its blocks. The mix-in and mix-out take a whole 32-bar
    // choice; the other kinds play their lanes in every block but for the entries, the rest and the
    // dropouts.
    struct KindPlan
    {
        std::string kind;
        std::vector<BlockChoice> choices; // intro and outro only
        std::vector<std::string> lanes;
        std::vector<LaneEntry> entries; // a lane in at most one
        std::optional<RestSpec> rest;
        std::optional<DropoutSpec> dropout;
        bool change = false; // every section of the kind has a lane change inside it: a rest, else a dropout, if the draws gave none
    };

    // The style's "form" (ADR 0023):
    //   "form": { "bars": [lo, hi], "sixteen": percent,
    //     "sections": [ { "kind": "intro", "bars": [ { "bars": 32, "w": 1 } ] }, ... ],  (a slot's "variant": 0-15, optional)
    //     "layer": [ { "lane": id, "w": weight } ],                          (optional)
    //     "plans": { "intro": { "choices": [ { "w": weight, "blocks": [ [ids], [ids], [ids], [ids] ] } ] },
    //                "drop": { "lanes": [ids],
    //                          "entries": [ { "lanes": [ids], "after" | "last" | "before": [ { "bars", "w" } ] } ],
    //                          "rest": { "p", "bars", "lanes": [ { "lane", "w" } ] },
    //                          "dropout": { "p", "every", "lanes": [ { "lanes": [ids], "w" } ] },
    //                          "change": true }, ... } }
    // The loader refuses a grammar that could draw a form breaking a DJ rule:
    // - lengths off the 8-bar grid; anything but a 32-bar intro first and a 32-bar outro last;
    // - a build not followed by a drop or drop2 that always plays;
    // - a mix-in choice that isn't drums only in its first block, lanes only joining, never three
    //   blocks alike (and a mix-out the other way round), or whose edge block is empty or lacks the
    //   lane a bass follows (the kick a DJ mixes on);
    // - a lane the mix-in ends on that the section after it doesn't play from its start (a late
    //   entry, a "before" entry that can sit the section out, drop 2's layer in a drop);
    // - no mix-out choice that starts on no non-drum lane the section before it can end without;
    // - an entry, rest or dropout lane the kind doesn't play, or that can't play all through it where
    //   it must;
    // - a "change" kind with a length too short for its rest (inside the section, for a drum lane)
    //   and its dropouts;
    // - a layer drop 2 doesn't play all through;
    // - lines it can draw (each drop and break start on or off a 16-bar line) that no lengths keep
    //   inside [lo, hi].
    // makeBundle adds that every lane a plan can play has a grid for the kind.
    struct FormSpec
    {
        Range bars { 200, 240 }; // the whole form's length
        std::int32_t sixteen = 70; // percent: a drop, drop2 or break starts on a 16-bar line (bar 0 mod 16), else on 8 mod 16
        std::vector<FormSlot> slots; // from the intro to the outro
        std::vector<KindPlan> plans; // one per kind the slots name
        std::vector<LaneWeight> layer; // drop 2's layer, drawn per song: drops play without it, drop2 sections with it ([] for none)

        const KindPlan* plan (std::string_view kind) const noexcept;
        // True if some plan of the kind (a block choice, or its lanes) can play the lane.
        bool mayPlay (std::string_view kind, std::string_view lane) const noexcept;
    };

    struct ChangeSpec
    {
        std::int32_t turn = 50; // percent: a turnaround in the last bar of a 4-bar line
        std::int32_t change = 70; // percent: a salient change at an 8-bar line
        std::int32_t early = 50; // percent: a change held at one 8-bar line comes 4 bars early at the next
        std::vector<std::string> roster; // the lanes a salient change may pick
    };

    struct DrumVoice
    {
        std::string id;
        std::int32_t length = 120; // ticks
    };

    struct EuclidCells
    {
        std::int32_t k = 0; // hits
        std::int32_t n = 16; // steps per cycle: 8, 16 or 32
        Range rotation; // drawn per variant
        std::int32_t p = 100; // percent per hit, drawn per variant
        Range vel; // drawn per variant and cell, so the cycle's accent shape repeats
        std::int32_t accent = 0; // added to the cycle's first hit
    };

    struct PolyCells
    {
        std::int32_t every = 3; // a hit every `every` 16ths...
        std::int32_t resetBars = 8; // ...counted from the last reset, every resetBars bars
        Range phase; // drawn per variant
        std::int32_t p = 100; // percent per hit, drawn per variant and position in the cycle
        Range vel; // drawn per variant and position in the cycle, so the accents repeat
        std::int32_t accent = 0; // added to the first hit after each reset
    };

    struct DrumVoiceGrid
    {
        std::string voice;
        bool hasCells = false; // p/vel given
        Grid p {}; // percent: drawn once per pattern variant; 100 is an anchor
        Grid vel {};
        std::int32_t spread = 0; // velocity +- per hit, drawn per bar
        Grid orn {}; // percent: drawn per bar where the pattern has no hit
        Range ornVel;
        std::int32_t skip = 0; // percent: a non-anchor pattern hit sits out a bar
        std::int32_t minPerBar = 0; // a pattern holds at least this many hits in each bar (redrawn up to the style's attempts)
        BarGrid turn {}; // percent: extra hits in a turnaround bar
        Range turnVel;
        BarGrid mute {}; // percent: non-anchor hits removed in a turnaround bar
        std::optional<EuclidCells> euclid;
        std::optional<PolyCells> poly;
    };

    struct DrumGrid
    {
        std::string name;
        std::vector<DrumVoiceGrid> voices; // in the lane's voice order; voices not listed are silent
        std::optional<Range> sync; // the pattern's LHL syncopation band (rhythm.h)
        std::vector<std::vector<std::int32_t>> never; // bar steps (0-15) a bar may not hold all of
        // Instead of voices, a ladder (a build's): the section split into stages.size() equal parts,
        // bar b of a section of B bars playing grid stages[b * size / B], each a grid of the lane with
        // voices of its own (quarters, then 8ths, then 16ths).
        std::vector<std::string> stages;
        // permille: every hit's velocity scaled from lo at the section's first bar to hi at its last
        // bar, in a straight line (a build's crescendo). On the grid a section kind uses.
        std::optional<Range> ramp;
    };

    struct ChokeRule
    {
        std::string voice; // e.g. the open hat...
        std::vector<std::string> by; // ...ends where one of these starts
    };

    struct DrumLane
    {
        std::vector<DrumVoice> voices;
        std::vector<ChokeRule> chokes;
        std::vector<std::pair<std::string, std::string>> exclusive; // (winner, loser) on the same step
        std::vector<DrumGrid> grids;
    };

    // Chord degrees a bass note may take, as weights: root, octave, fifth, seventh, third.
    enum class BassDegree : std::uint8_t {
        root,
        octave,
        fifth,
        seventh,
        third
    };
    inline constexpr std::size_t bassDegreeCount = 5;
    using DegreeWeights = std::array<std::int32_t, bassDegreeCount>;

    struct BassGrid
    {
        std::string name;
        Grid follow {}; // percent: a bass note on a kick of the follow lane at this step
        Grid p {}; // percent: a bass note where there is no kick (drawn per variant)
        Grid orn {}; // percent: drawn per bar
        Grid deg {}; // an index into BassLane::degreeSets
        Grid len {}; // ticks; 0 holds the note to the next one
        Grid vel {};
        std::int32_t spread = 0;
        Range ornVel;
        std::int32_t ornDeg = 0;
        std::int32_t ornBars = 1; // ornaments only in the last bar of every ornBars bars (8: phrase ends)
        bool hasSlide = false; // "slide" given: it replaces the lane's slide.p for this grid's notes
        Grid slide {}; // percent: the note on this cell slides into the next one, drawn per variant like the cell
        std::int32_t mutate = 0; // > 0: a change redraws only this many of the grid's variable cells (mutate.h)
    };

    struct BassLane
    {
        std::string follow; // the drum lane it follows and locks to; "" for none
        std::vector<DegreeWeights> degreeSets;
        std::int32_t holdGap = 30; // a held note ends this many ticks before the next
        std::int32_t holdMax = 1680; // and lasts at most this long
        std::int32_t approach = 0; // percent per chord change
        std::vector<Weighted> approachBy; // ticks before the change
        std::int32_t approachBelow = 50; // percent from a scale step below (else above)
        Range approachVel;
        std::int32_t slide = 0; // percent per pair of notes
        std::int32_t slideOverlap = 30; // ticks a slid note overlaps the next
        std::int32_t slideMaxJump = 12; // semitones
        std::int32_t restrike = 480; // a held note crossing a chord change by more than this is struck again
        std::int32_t duck = 0; // ticks: > 0, every note ends this far before each onset of the follow lane in its section, and before the section's end (duck.h)
        std::vector<BassGrid> grids;
    };

    struct ChordGrid
    {
        std::string name;
        bool pad = false; // a pad: one voicing held per chord
        std::int32_t padVel = 70;
        std::int32_t padGap = 40; // ticks before the chord ends
        bool padHold = false; // a key struck again straight after itself is held over (hold.h): a drone
        Grid p {}; // stabs: percent per variant
        Grid orn {}; // percent per bar
        std::int32_t ornBars = 1; // ornaments only in the last bar of every ornBars bars (8: phrase ends)
        Grid len {}; // ticks
        Grid vel {};
        std::int32_t spread = 0;
        std::int32_t minPerBar = 0; // stabs: at least this many pattern hits in each bar
        bool anticipate = true; // a stab on every pushed change
    };

    struct ChordLane
    {
        Range top { 4, 8 }; // the top voice's velocity over the stack's
        Range voice { -2, 1 }; // the other voices' velocity variation
        bool fresh = false; // voice leading starts afresh at every 8-bar phrase, so the voicings repeat phrase by phrase
        std::vector<ChordGrid> grids;
    };

    // An arp's cell: indices into the chord's tone ladder, played in turn on the grid's steps.
    struct ArpCell
    {
        std::vector<std::int32_t> idx; // 0 the chord root, 1 the next ladder tone up...
        std::int32_t weight = 0;
    };

    struct ArpGrid
    {
        std::string name;
        std::vector<std::int32_t> tones; // the ladder: scale steps above the chord root (0 root, 2 third, 4 fifth, 6 seventh, 7 octave, 8 ninth), ascending; index i is tones[i % n] + 7 * (i / n)
        Grid p {}; // percent per 16th, drawn per variant (100: always)
        Grid vel {};
        Grid len {}; // ticks
        std::int32_t spread = 0; // velocity +- per hit, drawn per bar
        std::vector<ArpCell> cells; // one is drawn per variant
        std::vector<std::int32_t> restart; // bar steps (0-15) where the cell starts again
        Range shift { 0, 0 }; // added to every index of the cell, drawn per variant (the cell starts on another chord tone)
        std::int32_t octave = 0; // semitones (a multiple of 12) added to the lane's register
    };

    struct ArpLane
    {
        std::int32_t center = 69; // each chord's root is placed nearest this note (the lower on a tie)
        std::vector<ArpGrid> grids;
    };

    // A lead lane's grid (lead.h): only the scales scoreLead puts on the hook's notes in a section kind.
    //   "grids": { "plant": { "vel": 920, "len": 850 }, "main": { "vel": 1000, "len": 1000 } }
    struct LeadGrid
    {
        std::string name;
        std::int32_t vel = 1000; // permille: the hook's score velocities scaled by this
        std::int32_t len = 1000; // permille: its lengths scaled by this
    };

    struct LeadLane
    {
        std::vector<LeadGrid> grids;
    };

    // A controller lane (cc.h): per section kind, a ramp over the section with optional arches on it.
    struct CcRow
    {
        std::string kind;
        Range from; // the value at the section's start, drawn per section
        Range to; // the value at its end: a straight ramp between
        std::vector<std::int32_t> waveBars; // arches of one of these lengths in bars (4, 8, 16, 32), drawn per section; empty: none
        Range depth { 0, 0 }; // an arch's height at its middle, drawn per section (negative: a dip)
    };

    struct CcSpec
    {
        std::int32_t number = 74; // the controller, 0-119
        std::int32_t step = 120; // ticks between points: 60, 120, 240 or 480
        std::vector<CcRow> rows; // one per section kind the lane plays in

        const CcRow* row (std::string_view kind) const noexcept;
    };

    struct LaneSpec
    {
        std::string id;
        LaneKind kind = LaneKind::drums;
        std::vector<std::pair<std::string, std::string>> use; // section kind -> grid name
        DrumLane drums;
        BassLane bass;
        ChordLane chords;
        ArpLane arp;
        LeadLane lead;
        std::vector<CcSpec> cc; // controller lanes written with the lane's notes (cc.h)

        // The grid name for a section kind, or "" if the lane has none there.
        std::string_view gridFor (std::string_view kind) const noexcept;
    };

    struct VoicingSpec
    {
        std::string name;
        std::vector<std::int32_t> degrees; // scale steps above the chord root: 0 root, 2 third, 4 fifth, 6 seventh, 8 ninth...
        std::vector<std::int32_t> shape; // or: semitones above the root, moved in parallel (planing)
        std::int32_t choose = 0; // > 0: use only this many of the tones, picked by cost (single-note riffs)
        bool inversions = false; // any tone may be lowest (else the first listed)
        Range span { 0, 48 }; // semitones from the lowest to the highest note
        Range top { 60, 84 }; // the top voice's arc
        std::int32_t center = 64; // the register the stack centres on
        std::int32_t quality = 0; // 0: any chord; 1: minor chords only (others take the palette's next voicing)
    };

    struct ProgressionSlot
    {
        std::string name; // "main", "break"...
        std::array<std::int32_t, 7> start {}; // weights for the first chord's degree
        std::string tonic = "present"; // present | emergent | absent
        std::int32_t phrygian = 0; // percent: the slot takes Phrygian colour
    };

    struct ShapeMemory
    {
        std::string name;
        std::vector<std::int32_t> shape; // chord-root degrees, read cyclically
    };

    // The low-interval limits every chord voicing keeps (voicingCandidates), so stacks stay clear in
    // the low mids: wide spacing at the bottom, and no semitone or minor-ninth rub low in the stack.
    struct MudSpec
    {
        std::int32_t below = 52; // below this note...
        std::int32_t gap = 5; // ...neighbouring voices are at least this far apart
        std::int32_t minorSecond = 0; // no two voices a semitone apart with the lower one below this note
        std::int32_t minorNinth = 0; // no two voices a minor ninth apart with the lower one below this note
    };

    struct HarmonySpec
    {
        std::vector<Weighted> modes; // value: Mode
        std::array<std::vector<std::int32_t>, modeCount> colour; // a progression in the mode must use one of these degrees
        std::vector<Weighted> loops; // bars: 2 or 4
        std::vector<std::vector<std::int32_t>> rhythms; // 1-3 units summing to 4 ([4]: a one-chord pedal)
        std::vector<std::int32_t> rhythmWeights;
        Range pushPermille { 400, 600 };
        std::vector<Weighted> pushBy; // ticks: 120 or 240
        bool pushAlike = false; // every phrase lays its pushes out alike, the section's first included (harmony.cpp)
        std::array<std::array<std::int32_t, 7>, modeCount> degrees {}; // per mode: weights for each degree
        std::array<std::array<std::int32_t, 7>, 7> move {}; // weight of degree a -> b
        std::vector<ShapeMemory> memory; // the anti-cliche memory
        std::int32_t penaltyWhole = 1000; // permille off a progression that is a memory shape
        std::int32_t penaltyThree = 900; // ...that holds 3 chords of one in a row
        std::int32_t penaltyTwo = 400; // ...that holds 2
        std::vector<ProgressionSlot> slots;
        std::vector<std::pair<std::string, std::string>> use; // section kind -> slot
        std::vector<std::pair<std::string, std::vector<std::string>>> palette; // section kind -> voicing names
        std::vector<VoicingSpec> voicings;
        MudSpec mud;
        std::int32_t revoice = 30; // percent: a recurring chord takes its second-best voicing
        std::vector<std::string> arcs; // rise | fall | arch | flat

        const ProgressionSlot* slot (std::string_view name) const noexcept;
        const VoicingSpec* voicing (std::string_view name) const noexcept;
        std::string_view slotFor (std::string_view kind) const noexcept;
        const std::vector<std::string>* paletteFor (std::string_view kind) const noexcept;
    };

    // ---- the hook (hook.h, docs/plans/S1b2b-plan.md): the style's "hook" block. planHook draws one
    // hook per seed from it (rhythm first, pitched key-relative), places it per 8-bar line over the
    // drawn form and realizes it on its carrier, a lead lane. Integers only, like the rest; the
    // loader refuses everything the plan's "The loader's refusals" lists, with the file, the JSON
    // path and the line:
    //   "hook": { "lane": the carrier, the style's one lead lane (every lead lane is it),
    //     "register": { "center", "span", "floor" (optional) },        (in the carrier's kit range, which
    //                                                                    starts at harmony.mud.minorSecond or above)
    //     "contours": [ { "shape": "late" | "early" | "twin", "w" } ],
    //     "cells": [ { "on": [16ths], "len": [16ths], "acc": [16ths], "w" } ],
    //     "mutate": { "drop", "add", "extra": [16ths] }, "answer": { "same", "move", "add", "drop", "headDiffers": 0 | 1 },
    //     "pitch": { "pool", "start": [7 weights], "end": [7], "moves": [8], "leap", "strong": "accents" | "beats",
    //                "peak": [lo, hi], "early": [lo, hi], "tones", "clash" },
    //     "vel": { "down" (optional), "beat" (optional), "accent", "other", "peak", "shape" }, "guard": { "second" },
    //     "period": { "idiom": "organ", "halves": [ { "on", "w" } ], "octave" (optional), "cadence": [7 weights], "register" (optional) }
    //            or { "idiom": "glass", "halves", "octave", "register" } or { "idiom": "stab", "cells", "octave", "register" },
    //     "pedal": { "degrees", "vel", "in": ["full", "answer", "fragment", "new"], "max" },   (optional: a pedal style)
    //     "withhold": [kinds],
    //     "plan": { kind: { "enter" (a groove's), "first", "next", "last", "holdFull" (drop 1's), "cover" (a drop's) }, ...,
    //               "after": { kind: { ... } or, a build's, { "continue": 1 } } } }
    // The plan has a menu for each kind the carrier's plans play it in (before the first drop, and
    // "after" it for a groove, break or build) and for no other; drop 2's first menu is "new"; a
    // drop's cover is 500 permille or more and reachable with a line to spare at every length the form
    // draws; and some groove or build before the first drop always plants the hook (plan §5.1).

    enum class HookContour : std::uint8_t {
        late, // one peak, the highest note alone, on a bar-2 onset in the "peak" window
        early, // the peak in bar 1, in the "early" window
        twin // the top struck on two onsets in a row
    };

    // Drop 2's new phrase, in the kit's written idiom.
    enum class HookIdiom : std::uint8_t {
        organ, // a b a' c, a 4-bar period: c keeps a's first half and falls by scale steps to 5, 2 or b3
        glass, // a b a' c on one rhythm in all four bars, c striking the top again
        stab // a second 2-bar riff from the period's own cells, on the tonic, its last note the 5th
    };

    // A line's operator in the line plan's menus ("op"): out rests (the hand-off); repeat plays the line
    // before note for note; full, fragment (the half-bar call), skeleton (the accents), answer (the
    // call, then the response) and augment (half speed) are textures of the hook; vary, cadence and
    // breath end the line before's texture (its last note on varyDegree, a fall to the 5th held to the
    // bar line, its last bar resting); newPhrase ("new") plays drop 2's new phrase; sequence plays the
    // line before an octave up.
    enum class HookOp : std::uint8_t {
        out,
        repeat,
        full,
        fragment,
        skeleton,
        answer,
        augment,
        vary,
        cadence,
        breath,
        newPhrase,
        sequence
    };

    // A bar-1 rhythm: "cells" (the hook's) and "period.cells" (the stab idiom's).
    struct HookCell
    {
        std::vector<std::int32_t> on; // onsets, 16ths 0-15, ascending
        std::vector<std::int32_t> len; // their lengths in 16ths, 1-16, one per onset
        std::vector<std::int32_t> acc; // the accented onsets, each one of `on`
        std::int32_t weight = 0;
    };

    struct HookOpWeight
    {
        HookOp op = HookOp::out;
        std::int32_t weight = 0;
    };

    // One section kind's line plan: "plan": { kind: ... } and, for the grooves, breaks and builds after
    // the first drop, "plan": { "after": { kind: ... } }.
    struct HookMenu
    {
        std::string kind;
        bool after = false; // a "plan.after" menu: the sections of the kind from the first drop on
        std::vector<Weighted> enter; // grooves: leading OUT lines (value: lines) by weight
        std::vector<HookOpWeight> first; // the carrier's first active line at or after `enter`
        std::vector<HookOpWeight> next; // the other lines ([]: repeat)
        std::vector<HookOpWeight> last; // the section's last line ([]: next)
        bool holdFull = false; // drops ("holdFull": 1): the drop holds a FULL line
        std::int32_t cover = 0; // drops: permille of the lines that play the hook, at least (0: none)
        bool carry = false; // "continue": 1 (an after build): every line keeps the section before's last state
    };

    // "register": where the riff sits in its kit lane's range.
    struct HookRegister
    {
        std::int32_t center = 74; // the tonic's octave puts the 3rd above the start nearest this key
        std::int32_t span = 15; // the riff spans at most this many semitones
        std::int32_t floor = -1; // the top voice's lowest key (FUT02: C5); -1: the kit range's low end
        std::uint32_t line = 0; // the style file's line of "register", for makeBundle's refusals (it checks the block against the kit range)
    };

    // "mutate": per seed, at most one non-accent onset of the cell drops out and one extra comes in.
    struct HookMutate
    {
        std::int32_t drop = 0; // percent
        std::int32_t add = 0; // percent
        std::vector<std::int32_t> extra; // the 16ths an extra onset may take
    };

    // "answer": bar 2 keeps bar 1's 16ths 0-7 and makes at most one edit among 8-15, by weight.
    struct HookAnswer
    {
        std::int32_t same = 0;
        std::int32_t move = 0; // an onset moved by a 16th
        std::int32_t add = 0;
        std::int32_t drop = 0;
        bool headDiffers = true; // bar 2's first half differs from bar 1's in a pitch
    };

    // "pitch": the degree walk.
    struct HookPitch
    {
        std::vector<std::int32_t> pool; // scale degrees from the tonic, 0-6
        std::array<std::int32_t, 7> start {}; // weights of the start degree
        std::array<std::int32_t, 7> end {}; // weights of the end degree
        std::array<std::int32_t, 8> moves {}; // weights of moves of 0-7 scale steps
        std::int32_t leap = 9; // the largest move in semitones, the riff's wrap and the period's joins included
        bool beats = false; // "strong": "beats" (a strong onset is a beat only), else "accents" (a beat or an accent)
        Range peak { 18, 28 }; // the late contour's window, 16ths of the 2-bar phrase
        Range early { 4, 12 }; // the early contour's window
        std::vector<std::int32_t> tones; // scale steps above a chord's root a strong onset takes (1 3 5 7 9: 0 2 4 6 8)
        std::vector<std::int32_t> clash; // scale steps above a chord's root a strong note can't sit a semitone over (1 3 5 7)
    };

    // "vel": the fixed accent shape, drawn once per seed.
    struct HookVelocity
    {
        std::int32_t down = -1; // the downbeat's accent; -1: beat's
        std::int32_t beat = -1; // beat 3's accent; -1: accent's
        std::int32_t accent = 0; // the other accents
        std::int32_t other = 0; // the rest
        std::int32_t peak = 0; // added to the peak
        std::int32_t shape = 0; // one offset per onset in +-shape
    };

    // "period": drop 2's new phrase.
    struct HookPeriod
    {
        HookIdiom idiom = HookIdiom::organ;
        std::vector<std::pair<std::vector<std::int32_t>, std::int32_t>> halves; // organ: bar a's second-half onsets (8-15) by weight
        std::int32_t octave = 0; // its octave against the riff's, -1 to 1
        std::array<std::int32_t, 7> cadence {}; // organ: weights of the fall's end degree (the tonic's is 0)
        std::optional<Range> range; // its own register ("register", FUT02's glass), inside the kit range; none: the riff's
        std::uint32_t rangeLine = 0; // the style file's line of "register", for makeBundle's refusals
        std::vector<HookCell> cells; // stab: its cells
    };

    // "pedal" (FUT02's ChromeArp): a key-fixed pedal under the top voice, after each top note on the
    // free 16ths before the next one, never on 16th 15 nor before the bar's first top note.
    struct HookPedal
    {
        std::vector<std::int32_t> degrees; // scale degrees from the tonic, -7 to 7, in the order they play (FUT02: 0, -3)
        std::vector<std::int32_t> vel; // one per degree, 1-127
        std::vector<HookOp> in; // the textures it plays under: full, answer, fragment, newPhrase
        std::int32_t max = 2; // pedal notes after one top note, 1-2
    };

    struct HookSpec
    {
        std::string lane; // the carrier, a lead lane
        HookRegister reg;
        std::vector<std::pair<HookContour, std::int32_t>> contours; // by weight
        std::vector<HookCell> cells; // bar 1's rhythm, one by weight
        HookMutate mutate;
        HookAnswer answer;
        HookPitch pitch;
        HookVelocity vel;
        std::int32_t guard = 0; // "guard.second": a semitone against a chord-lane note only with the lower note at this key or above (a minor 9th never)
        HookPeriod period;
        std::optional<HookPedal> pedal;
        std::vector<std::string> withhold; // kinds with no FULL or NEW before the first drop
        std::vector<HookMenu> menus;

        // The menu a section of the kind plays: a drop's and drop 2's own; a groove's, break's or
        // build's own until the first drop and its after-menu from then on. nullptr where there is none.
        const HookMenu* menu (std::string_view kind, bool afterFirstDrop) const noexcept;
    };

    struct Style
    {
        std::string id;
        std::string about;
        std::vector<SectionSpec> sections;
        FormSpec form; // the extended mix's grammar: each seed draws its form from it (arrange.h)
        ChangeSpec changes;
        std::int32_t attempts = 16; // per pattern draw, then the best so far
        HarmonySpec harmony;
        std::vector<LaneSpec> lanes; // in track order
        std::optional<HookSpec> hook; // the "hook" block: none, and the style generates as it did before it

        const SectionSpec* section (std::string_view kind) const noexcept;
        std::optional<std::size_t> laneIndex (std::string_view laneId) const noexcept;
    };

    // ------------------------------------------------------------------ feel

    struct AfterRule
    {
        std::string voice; // e.g. the clap...
        std::string after; // ...plays this many ticks after this voice on the same step
        Range by;
    };

    // One lane's feel. Fields a row leaves out fall back to the lane's defaults.
    struct FeelLane
    {
        std::optional<Range> swing; // permille relative to the kit's base swing, drawn per song
        std::optional<Range> sd; // 1/f timing, tenths of a tick
        std::optional<std::int32_t> clamp; // ticks either way, for notes that aren't anchors
        std::optional<bool> anchor; // anchors (100% cells) stay within the feel's anchorClamp
        std::optional<Grid> offset; // ticks per step
        std::optional<Grid> vel; // velocity per step
        std::optional<Range> vsd; // 1/f velocity, tenths
        std::optional<Range> dip; // lo: velocity off a note before an accent; hi: the accent's score velocity
        std::optional<std::string> lock; // the lane whose ticks onsets on its steps take (the low-end lock)
        std::optional<std::vector<AfterRule>> after;
        std::optional<Range> spread; // chords: ticks from the lowest voice's onset to the highest's
        std::optional<std::int32_t> artic; // permille: lengths vary by up to this much
        std::optional<std::int32_t> tight; // a note with a score velocity at or above this plays as written (only the lock moves it); the ghosts below take the feel
    };

    struct FeelRow
    {
        std::string kind;
        std::vector<std::pair<std::string, FeelLane>> lanes;
    };

    struct Feel
    {
        std::string id;
        std::string about;
        std::int32_t amount = 1000; // permille: the Feel knob's default
        std::int32_t anchorClamp = 2; // ticks (plan §4.1)
        std::vector<std::pair<std::string, FeelLane>> lanes;
        std::vector<FeelRow> rows; // one per section kind that differs

        // The lane's feel in a section kind: the row's fields over the lane's defaults.
        FeelLane resolve (std::string_view laneId, std::string_view kind) const;
    };

    // ------------------------------------------------------------------ loading

    struct Bundle
    {
        Kit kit;
        Style style;
        Feel feel;
    };

    // Parse one file's text; source names it in messages ("content/kits/fut01.json").
    [[nodiscard]] util::Result<Kit> parseKit (std::string_view text, std::string_view source);
    [[nodiscard]] util::Result<Style> parseStyle (std::string_view text, std::string_view source);
    [[nodiscard]] util::Result<Feel> parseFeel (std::string_view text, std::string_view source);

    // Checks that the three fit together: every style lane has a kit lane and the other way round,
    // every drum voice has a note in 35-59, every lane active in the form has a grid for that
    // section kind, the feel names only lanes and voices the style has, and so on.
    [[nodiscard]] util::Result<Bundle> makeBundle (Kit kit, Style style, Feel feel);

    // The material keys of a hook block ("register", "contours", "cells", "mutate", "answer", "pitch", "vel", "guard",
    // "period", "pedal") into hook, refusing as the style loader does; the caller checks the keys it allows. The
    // phrase specs' "material" (gen/phrase.h) reads this way.
    void readHookMaterial (const json::Node& h, HookSpec& hook);

    // Reads <content>/kits/<kitId lowercased>.json and the style and feel it names.
    [[nodiscard]] util::Result<Bundle> loadBundle (const std::filesystem::path& contentDir, std::string_view kitId);

    // percent (0-100) to p16 (0-65536), exactly: pct * 65536 / 100 rounded half up.
    constexpr std::int32_t p16FromPercent (std::int32_t pct) noexcept
    {
        if (pct <= 0)
            return 0;
        if (pct >= 100)
            return 65536;
        return static_cast<std::int32_t> ((static_cast<std::int64_t> (pct) * 65536 * 2 + 100) / 200);
    }
}
