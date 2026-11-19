#pragma once

// Drum kits (ADR 0030): a kit's drums as a folder a drum rack or an SFZ player opens, planned from its two manifests
// alone. The folder, in the samples folder beside the Samples drawer's copies:
//
//     <KIT> <Title> Drum Kit[ c<N>]/
//         Pads/<key> <note> <sound> - <KIT>.wav               one per drum note: its top layer's first round robin
//         Samples/<key> <note> <installed name> - <KIT>.wav   every layer and round robin of the zone that plays the note
//         <folder>.sfz, or <folder> - <kick label>.sfz        the kit for an SFZ player, one per side of the kick switch
//         README.txt                                          the pad map, and the steps for Live, Logic and SFZ players
//
// - The pads are every key of every channel-10 keymap entry, both instruments of an A/B entry: the notes the drum MIDI
//   plays. Notes are named as the drum maps name them, middle C (60) being C3: 36 is C1.
// - Each SFZ plays every pad, layer and round robin as Wubansia's sampler picks them (engine/sampler/selection.h): the
//   nearest layer, the louder on a tie; round robins in turn; the pan cycle; at the channel's gain and pan inserts that
//   apply on that kick side; the hat choke as group/off_by.
// - planDrumKit () is pure and deterministic: text numbers come from integers, never floating point or a locale, and the
//   texts hold no date, version or path, so the same manifests give the same bytes on every platform and build.
// - What an SFZ drum pad can't carry is refused, naming the kit, the pad and its sound: end alignment, mono or a glide,
//   layered_with, a pre-roll, pow15, a sequence over 100 notes, restrike_continues off a looped pad, a level or pan
//   that depends on the vox switch, a drum A/B switch other than the kick's.

#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "util/result.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wubansia::sounds
{
    // 35 "B0", 36 "C1", 37 "C#1", 54 "F#2": the drum maps' names, middle C (60) being C3 as in Live and Logic.
    std::string drumNoteName (int key);

    // "FUT01 Alloy Signal Drum Kit", " c<N>" from content v2 on, in safe characters.
    std::string drumKitFolderName (const engine::KitMeta& kit);

    struct DrumKitPad
    {
        int key = 0; // a channel-10 keymap key, 35-59
        int side = -1; // -1 without a switch; else the kick switch's side, 0 A, 1 B
        int instrument = -1; // into the manifest's instruments
        int zone = -1; // into its zones: the one holding key
        std::string sound; // "Kick Garage", "Conga G3"
        std::int32_t levelMdb = 0; // gain_mdb + the channel's gain inserts that apply on this side
        std::int32_t panPm = 0; // the channel's pan insert, or 0

        friend bool operator== (const DrumKitPad&, const DrumKitPad&) = default;
    };

    struct DrumKitFile
    {
        std::string path; // in the folder, '/'-separated: "Pads/36 C1 Kick Garage - FUT01.wav"
        int source = -1; // a kit.json file index for a WAV; -1 for a text
        std::string text; // a text's bytes: LF, final newline

        friend bool operator== (const DrumKitFile&, const DrumKitFile&) = default;
    };

    struct DrumKitPlan
    {
        std::string folder; // drumKitFolderName ()
        std::vector<DrumKitPad> pads; // by key, then side
        std::vector<DrumKitFile> files; // Pads/, Samples/, the SFZ files (kick B's first), README.txt
        std::array<std::string, 2> sfz; // the SFZ's path per kick side ([0] A, [1] B); the same twice without a switch
        int wavs = 0, texts = 0;

        friend bool operator== (const DrumKitPlan&, const DrumKitPlan&) = default;
    };

    // A kit's drum kit from its two manifests alone; pure and deterministic. Refused, by kit, key and sound, for what an
    // SFZ drum pad can't carry: "FUT05's pad 36 C1 (Kick Rev) is end-aligned, which a drum kit can't carry".
    [[nodiscard]] util::Result<DrumKitPlan> planDrumKit (const engine::KitManifest& kit, const engine::MixConfig& mix);
}
