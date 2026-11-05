#pragma once

// The Kit: everything one kit at one content version needs to play. P4's buildKit assembles it on the
// service thread from the two manifests (kit/manifest.h), the sample source and the four timelines;
// it never changes afterwards. Instances share it (KitCache), the audio thread gets it as a raw
// pointer (engine.h: Engine::publish) and only ever reads it, and it is freed off the audio thread.
//
// First the vocabulary: the ten lanes in their PERMANENT order, the three A/B switches, and the
// enums kit.json spells as strings (docs/plugin/manifest.md has every field). Then the Kit's flat
// tables. The layout is part of the contract, so buildKit and testing::makeSyntheticKit lay a
// manifest out identically:
// - instruments in the manifest's order;
// - zones: each instrument's zones in order, appended instrument by instrument
//   (Instrument::firstZone, zoneCount);
// - layers: each zone's layers in order, appended zone by zone in the zones table's order
//   (Zone::firstLayer, layerCount);
// - rr: each layer's round-robin file indices in order, appended layer by layer (Layer::firstRr, rrCount);
// - pans: each instrument's pan cycle, appended instrument by instrument (firstPan, panCount; 0: centre);
// - phrases in the manifest's order, their slices appended phrase by phrase (firstSlice, sliceCount);
// - choke groups numbered 1, 2... in order of first use among the instruments (0: none);
// - files and audio in the manifest's files order.
//
// Nothing here depends on the host's sample rate: times stay in the manifest's microseconds and
// 48 kHz frames, positions in milliticks. One Kit serves instances running at different rates.

#include "ids.h"
#include "kit/sample_source.h"
#include "mix/mix_config.h"
#include "sched/timeline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::engine
{
    // The lanes, in their PERMANENT order: the parameter IDs follow it (<lane>_level, <lane>_mute),
    // and so do Controls::laneGain, Telemetry::lanePeak and every lane bitmask. Never reorder or
    // renumber; a new lane is appended.
    enum class Lane : std::uint8_t {
        kick,
        snare,
        hats,
        perc,
        bass,
        lead,
        chords,
        vox,
        accents,
        fx
    };
    inline constexpr int kLanes = 10;

    // Each lane's ID in the manifests and in parameter IDs. PERMANENT.
    inline constexpr std::array<std::string_view, kLanes> kLaneIds {
        "kick", "snare", "hats", "perc", "bass", "lead", "chords", "vox", "accents", "fx"
    };

    constexpr std::string_view laneId (Lane lane) noexcept { return kLaneIds[static_cast<std::size_t> (lane)]; }
    constexpr int laneIndex (std::string_view id) noexcept { return idIndex (kLaneIds, id); } // -1 if unknown
    constexpr std::uint32_t laneBit (Lane lane) noexcept { return 1u << static_cast<unsigned> (lane); }

    // The A/B switches, in Controls::ab's order (kick_ab, bass_ab, stab_ab).
    enum class AbSwitch : std::uint8_t {
        kick,
        bass,
        stab
    };
    inline constexpr int kAbSwitches = 3;
    inline constexpr std::array<std::string_view, kAbSwitches> kAbSwitchIds { "kick", "bass", "stab" };

    // How a voice plays its sample.
    enum class SampleMode : std::uint8_t {
        oneShot, // plays to its end; note-off is ignored
        gated, // releases at note-off on a raised cosine of releaseUs
        sustainLoop // wraps [loopStart, loopEnd) while held, then releases
    };
    inline constexpr std::array<std::string_view, 3> kSampleModeIds { "one_shot", "gated", "sustain_loop" };

    enum class Align : std::uint8_t {
        start, // the sample starts on the note (less its pre-roll)
        end // the sample's end lands on the note's end at the current tempo (risers, fills, swells)
    };
    inline constexpr std::array<std::string_view, 2> kAlignIds { "start", "end" };

    enum class VelocityLaw : std::uint8_t {
        layerOnly, // the layer carries the dynamics (drums, perc)
        pow15 // the layer, times (velocity / layer velocity) ^ 1.5 (pitched parts)
    };
    inline constexpr std::array<std::string_view, 2> kVelocityLawIds { "layer_only", "pow15" };

    enum class Interp : std::uint8_t {
        sinc, // the polyphase windowed sinc
        hermite // 4-point Hermite, as fut01/basslines.hermite
    };
    inline constexpr std::array<std::string_view, 2> kInterpIds { "sinc", "hermite" };

    // How a mono legato voice moves to a new note's pitch. The curves run in log frequency (semitones).
    enum class GlideCurve : std::uint8_t {
        none, // no glide (and us == 0)
        raisedCosine, // a raised-cosine slide over us, or over dipUs for a dip (FUT01: 70 ms, dip 30 ms; FUT04: 50 ms)
        exponential, // an exponential approach with time constant us (FUT02: 18 ms)
        linear // a linear slide over us
    };
    inline constexpr std::array<std::string_view, 4> kGlideCurveIds { "none", "raised_cosine", "exponential", "linear" };

    enum class ChokeRole : std::uint8_t {
        none, // in no choke group
        chokes, // its note-on fades the group's choked voices (the closed hat)
        choked, // faded by the group's chokers (the open hat)
        both
    };
    inline constexpr std::array<std::string_view, 4> kChokeRoleIds { "none", "chokes", "choked", "both" };

    enum class ChokeCurve : std::uint8_t {
        linear, // FUT01
        cos2 // FUT02-04
    };
    inline constexpr std::array<std::string_view, 2> kChokeCurveIds { "linear", "cos2" };

    // How a phrase's slice notes reach the throw sends (CC91 Space, CC94 Echo).
    enum class PhraseThrow : std::uint8_t {
        live, // the engine sends their throws live, as for any note (most stems)
        baked // the stem has its throw in the audio: no live throw for its slice notes
    };
    inline constexpr std::array<std::string_view, 2> kPhraseThrowIds { "live", "baked" };

    // An A/B switch's two labels for the editor. Both empty: the kit has no such switch.
    struct AbLabels
    {
        std::string a;
        std::string b;

        bool exists() const noexcept { return !a.empty(); }

        friend bool operator== (const AbLabels&, const AbLabels&) = default;
    };

    // Mono legato glide: a new note while one is held moves the held voice to its pitch instead of
    // starting a voice, keeping the first note's zone and layer. A dip is a legato note whose next legato
    // note returns to the pitch before it (FUT01's 30 ms pitch dip): the dip and that returning note
    // glide over dipUs (the timeline marks both with NoteFlag::dip). Every other legato note is a slide,
    // over us.
    struct Glide
    {
        GlideCurve curve = GlideCurve::none;
        std::int32_t us = 0; // a slide's length (raised_cosine, linear) or time constant (exponential)
        std::int32_t dipUs = 0; // raised_cosine only: a dip's length; 0 when the instrument has no dips

        friend bool operator== (const Glide&, const Glide&) = default;
    };

    struct Choke
    {
        std::uint8_t group = 0; // 0: none; groups numbered from 1 (see the layout above)
        ChokeRole role = ChokeRole::none;
        std::int32_t fadeUs = 0; // how fast this instrument's voices fade when choked
        ChokeCurve curve = ChokeCurve::linear;

        friend bool operator== (const Choke&, const Choke&) = default;
    };

    // What the manifest says about a file, beside its audio (the SHA-256 stays in the manifest).
    struct KitFile
    {
        std::string path;
        std::int64_t frames = 0; // at 48 kHz
        std::int32_t channels = 0; // 1 or 2
        std::int64_t loopStart = 0; // [loopStart, loopEnd) in frames; loopEnd == 0: no loop
        std::int64_t loopEnd = 0;
        std::int32_t prerollFrames = 0; // frames before the musical onset

        friend bool operator== (const KitFile&, const KitFile&) = default;
    };

    // One file's audio as a voice reads it. Mono: right == left.
    struct SampleView
    {
        const float* left = nullptr;
        const float* right = nullptr;
        std::int64_t frames = 0;
        std::int32_t channels = 0;
        std::int64_t loopStart = 0;
        std::int64_t loopEnd = 0; // 0: no loop
        std::int32_t prerollFrames = 0;
    };

    struct Layer
    {
        std::uint8_t velocity = 127; // round (layer x 127): the velocity this layer was made for
        std::uint16_t firstRr = 0; // into Kit::rr
        std::uint8_t rrCount = 0;

        friend bool operator== (const Layer&, const Layer&) = default;
    };

    // A zone plays keys lo..hi, repitched by (key - root) semitones from its sample.
    struct Zone
    {
        std::uint8_t root = 60;
        std::uint8_t lo = 0;
        std::uint8_t hi = 127;
        std::uint16_t firstLayer = 0; // into Kit::layers, softest first
        std::uint8_t layerCount = 0;

        friend bool operator== (const Zone&, const Zone&) = default;
    };

    struct Instrument
    {
        std::string id; // unique in the kit: [a-z0-9_]+
        std::string name; // for people: "Kick Garage"
        Lane lane = Lane::kick;
        std::uint8_t mixChannel = 0; // index into mix.channels; that channel's lane is this lane
        SampleMode mode = SampleMode::oneShot;
        Align align = Align::start;
        std::int32_t attackUs = 0; // a raised-cosine fade-in
        std::int32_t releaseUs = 0; // a raised-cosine fade-out after note-off (gated, sustain_loop)
        std::int32_t gainMdb = 0; // the voice's own gain, before its mix channel
        std::int64_t startFrames = 0; // playback starts this far into each of its files (the attack fades it in)
        VelocityLaw velocityLaw = VelocityLaw::layerOnly;
        Interp interp = Interp::sinc;
        bool mono = false; // one voice at a time, legato (with glide when glide.curve isn't none)
        Glide glide;
        bool restrikeContinues = false; // struck again on its own end tick, it continues instead of re-attacking
        Choke choke;
        bool duckKey = false; // its notes are onset-duck keys for its lane
        std::uint16_t firstPan = 0; // into Kit::pans
        std::uint8_t panCount = 0; // 0: centre
        std::int32_t rrBlock16ths = 64; // round robins and pans count hits within blocks this long
        std::int16_t layeredWith = -1; // an instrument that also plays every multisample note of this one
        std::uint16_t firstZone = 0; // into Kit::zones, by root
        std::uint8_t zoneCount = 0;

        friend bool operator== (const Instrument&, const Instrument&) = default;
    };

    // What a (MIDI channel, key) plays: instrument a, or b when the A/B switch ab is on B.
    struct KeyTarget
    {
        std::int16_t a = -1; // instrument index; -1: nothing plays
        std::int16_t b = -1; // -1: no alternative
        std::int8_t ab = -1; // an AbSwitch value when b >= 0, else -1

        friend bool operator== (const KeyTarget&, const KeyTarget&) = default;
    };

    // A phrase lane's recorded line: a stem (per A/B character) and the slices that tie it to notes. Its
    // rides (PhraseEntry::ride74, ride71) stay in the manifest: only the matcher reads them.
    struct Phrase
    {
        Lane lane = Lane::lead;
        std::uint8_t channel = 0; // 0-15: the MIDI channel whose notes it covers
        std::int8_t ab = -1; // an AbSwitch value choosing the stem, or -1
        std::uint8_t mixChannel = 0;
        std::int32_t gainMdb = 0; // the stem's own gain, before its mix channel
        PhraseThrow throwMode = PhraseThrow::live;
        std::array<std::uint16_t, 2> stem {}; // file index per character (A, B); B == A without an alternative
        std::int32_t cycle16ths = 32; // 32 or 64, aligned to the song grid
        std::int32_t tolerancePm = 20; // slices play while |host BPM / kit BPM - 1| <= tolerancePm / 1000
        std::int32_t fadeUs = 5000; // a slice with no follower fades over this, from its fade frame
        // A run that starts on a slice whose lead is 0 fades in over this, so plucks, arps and blips keep
        // their transients; a run that starts in a slice's pre-roll (lead > 0) crossfades over 3 ms.
        std::int32_t fadeInUs = 500;
        std::uint16_t firstSlice = 0; // into Kit::slices
        std::uint16_t sliceCount = 0;

        friend bool operator== (const Phrase&, const Phrase&) = default;
    };

    inline constexpr int kMaxHolds = 8;

    // One note of a phrase, in the stem's 48 kHz frames. holds are the notes after the slice's own that it
    // plays itself, by key, in order: retriggers of its key (FUT04's ratchets) and the targets its note
    // glides to inside it (FUT03's in-note glides, written as legato notes). Each starts after the slice's
    // note and before the next note the phrase's pattern matches on that channel; the matcher gives them no
    // sound of their own.
    struct PhraseSlice
    {
        std::int32_t step = 0; // the 16th in the cycle where its note starts
        std::uint8_t key = 0;
        // The onset (phrase.Grid.at (step)) less lead; in a looped stem, modulo the stem's frames (a slice 0
        // whose pre-roll falls before its cycle starts at frames + onset - lead and wraps).
        std::int64_t start = 0;
        // Exclusive. In a looped stem, read modulo its frames, the end may run past the stem's frames, up
        // to start + frames; the last slice ends where the next cycle's slice 0 starts.
        std::int64_t end = 0;
        std::int64_t fade = 0; // where it fades out when no slice follows it; up to start + frames too
        std::int32_t lead = 0; // frames of pre-roll before the onset
        std::array<std::uint8_t, kMaxHolds> holds {}; // keys, in order; entries from holdCount on are 0
        std::uint8_t holdCount = 0;

        friend bool operator== (const PhraseSlice&, const PhraseSlice&) = default;
    };

    // An instrument a kit adds on channels 11-16 (tools/sound/channels.py EXTRAS): its track name in
    // the kit's MIDI files and the lane it plays on.
    struct KitExtra
    {
        std::uint8_t channel = 10; // 0-15 (10-15: MIDI channels 11-16)
        std::string name; // the track's and the instrument's name, "Organ Bass"
        Lane lane = Lane::bass;

        friend bool operator== (const KitExtra&, const KitExtra&) = default;
    };

    struct KitInfo
    {
        std::string id; // "FUT01"
        std::string title; // "Alloy Signal"
        std::string direction; // "future garage / 2-step"
        std::string key; // "F# minor"
        std::string camelot; // "11A"
        std::int32_t bpm = 0; // 128-140
        std::int32_t swingPm = 500; // where the second 16th of each 8th lands, x 1000 (590: 0.59)
        std::int32_t contentVersion = 0;
        std::int32_t index = -1; // the kit's place in the plugin's kit list, set by the service; Telemetry::kitIndex
        std::array<std::string, kLanes> laneLabels; // "" where the kit has no such lane
        std::array<AbLabels, kAbSwitches> ab;
        std::uint32_t lanes = 0; // bit per Lane the kit has (laneLabels[i] not empty)
        std::uint32_t abSwitches = 0; // bit per AbSwitch the kit has
        std::vector<KitExtra> extras;

        friend bool operator== (const KitInfo&, const KitInfo&) = default;
    };

    struct Kit
    {
        KitInfo info;
        std::vector<KitFile> files;
        std::vector<SampleData> audio; // one per file, same order
        std::vector<Instrument> instruments;
        std::vector<Zone> zones;
        std::vector<Layer> layers;
        std::vector<std::uint16_t> rr; // file indices
        std::vector<std::int16_t> pans; // per mille, -1000..1000
        std::vector<Phrase> phrases;
        std::vector<PhraseSlice> slices;
        std::array<std::array<KeyTarget, 128>, 16> keys {}; // [channel 0-15][key]; built from the keymap
        // [channel 0-15]: bit per mix channel a CC on that MIDI channel reaches: the mix channels of
        // every instrument the keymap names on it (a and b, and what they're layered with) and of every
        // phrase on it.
        std::array<std::uint32_t, 16> ccMixChannels {};
        MixConfig mix;
        // [0] Loop vox, [1] Loop no-vox, [2] Song vox, [3] Song no-vox: engine.h's timelineIndex ().
        std::array<Timeline, 4> timelines;

        SampleView view (std::size_t file) const noexcept
        {
            SampleView v;
            if (file >= audio.size() || file >= files.size())
                return v;
            const auto& data = audio[file];
            const auto& meta = files[file];
            v.left = data.audio.data();
            v.right = data.channels == 2 ? data.audio.data() + data.frames : v.left;
            v.frames = data.frames;
            v.channels = data.channels;
            v.loopStart = meta.loopStart;
            v.loopEnd = meta.loopEnd;
            v.prerollFrames = meta.prerollFrames;
            return v;
        }

        friend bool operator== (const Kit&, const Kit&) = default;
    };
}
