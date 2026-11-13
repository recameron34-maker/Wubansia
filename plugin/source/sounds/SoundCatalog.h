#pragma once

// The Samples drawer's catalog (ADR 0024): every sound of a kit, from its kit.json alone, so it needs no
// samples. Pure and deterministic: the same manifest gives the same sounds, in the same order, on every
// platform. KitCatalog keeps each kit's (Entry::sounds); the Controller's sounds () hands them out.
//
// The rules:
// 1. Instruments, in manifest order. An instrument's file set is every round-robin file of its zones and
//    layers. An instrument whose set is an earlier sound's is that sound: the earlier keeps its id, name and
//    place, takes the higher category of the two (Accents > FX > Vox > Synths > Bass > Drums), and, when the
//    newcomer is pitched and the earlier isn't, the newcomer's zones choose the default file. So FUT02's two
//    Glass Pings, FUT04's three accent twins and FUT01's Glitch Fill Lift Beat 4 are one sound each.
// 2. The category is the lane's: accents Accents, fx FX, vox Vox, lead and chords Synths, bass Bass, and
//    kick, snare, hats and perc Drums.
// 3. Then each phrase stem (stem a, then b), in phrase order, is a Loop, unless a sound already has exactly
//    that one file.
// 4. files: the zones in order, each zone's layers soft to hard, each layer's round robins; a file seen before
//    is skipped.
// 5. The default file is the top layer's first round robin of one zone: for a pitched instrument (a zone with
//    lo < hi) the first zone rooted on the kit's tonic, else its middle zone (zones[size / 2]); for a one-key
//    instrument its first zone. So it is the kit's key note in the lowest register that has one, at its loudest:
//    usually a note the kit's loops play, but not always (FUT03's Vox Synth and Keys EP give G3, below the G4 up
//    the kit plays them; FUT02's Chrome Arp gives D4, below its arp's A4-C6). The File box reaches every file.
// 6. name: the instrument's, a leading "Accent " dropped ("Dub Chord Cm9"); a loop's, its file name without
//    folder and ".wav". id: "<KIT>/<instrument id>", or "<KIT>/<path>" for a loop.
// 7. bars: the default file's length in whole bars at the kit's tempo (48000 x 240 / bpm frames a bar) when
//    that is at least 1 and within 48 frames (1 ms) of whole bars, else 0. leadInMs: its pre-roll in ms,
//    rounded. endAligned: the instrument's align is end. loopBars: a loop's cycle_16ths / 16.

#include "Controller.h"

#include "kit/manifest.h"

#include <juce_core/juce_core.h>

#include <string_view>
#include <vector>

namespace wubansia::sounds
{
    // Every sound of the kit, by the rules above.
    std::vector<SoundInfo> soundsOf (const engine::KitManifest& kit);

    SoundCategory categoryOf (engine::Lane lane) noexcept;

    // "Accents", "FX", "Vox", "Synths", "Bass", "Drums" or "Loops".
    juce::String categoryName (SoundCategory category);

    // The pitch class (C = 0 ... B = 11) of a key's first word: "F# minor" 6, "D minor" 2, "Bb major" 10;
    // -1 when it isn't a note name.
    int tonicOf (std::string_view key) noexcept;
}
