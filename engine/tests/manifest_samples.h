#pragma once

// A small, complete pair of manifests for the parser and builder tests (TST01): every field docs/plugin/manifest.md
// lists, every insert type, both duck modes, both kinds of `when`, a phrase with A/B stems and holds, an extra on
// channel 11, mono and stereo files. Written by hand (not in the canonical layout: the parsers don't care), so a test
// can break one thing at a time. The synthetic kit SYN01 (engine/testing) is the other pair the tests parse.

#include "kit/sample_source.h"
#include "util/result.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::engine::test
{
    // Files: 0 kick A; 1-4 kick B (2 layers x 2 round robins); 5 clap; 6-7 the hats (mono); 8-9 bass A's two zones
    // (mono, looped); 10 bass B (looped); 11 the pad; 12 the lead; 13 the vox (20 ms pre-roll); 14 the riser;
    // 15-16 the lead phrase's stems A and B; 17 the sub layered under bass B (mono).
    // Instruments: 0 kick_a, 1 kick_b, 2 clap, 3 hat_closed, 4 hat_open, 5 bass_a, 6 bass_b, 7 sub_layer, 8 pad,
    // 9 lead, 10 vox, 11 riser.
    inline constexpr std::string_view kKitJson = R"json({
  "kit": {
    "id": "TST01", "title": "Test Signal", "direction": "test / synthetic", "bpm": 130, "key": "F# minor",
    "camelot": "11A", "swing_pm": 560, "content_version": 3,
    "lanes": { "kick": "Kick", "snare": "Clap", "hats": "Hats", "perc": "", "bass": "Bass", "lead": "Lead",
               "chords": "Pad", "vox": "Vox", "accents": "", "fx": "Riser" },
    "ab": { "kick": { "a": "Kick A", "b": "Kick B" }, "bass": { "a": "Bass A", "b": "Bass B" }, "stab": { "a": "", "b": "" } }
  },
  "files": [
    { "path": "Drums/Kick A.wav", "frames": 4800, "channels": 2, "sha256": "0000000000000000000000000000000000000000000000000000000000000000", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Kick B v1 rr1.wav", "frames": 4800, "channels": 2, "sha256": "1111111111111111111111111111111111111111111111111111111111111111", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Kick B v1 rr2.wav", "frames": 4801, "channels": 2, "sha256": "2222222222222222222222222222222222222222222222222222222222222222", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Kick B v2 rr1.wav", "frames": 4802, "channels": 2, "sha256": "3333333333333333333333333333333333333333333333333333333333333333", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Kick B v2 rr2.wav", "frames": 4803, "channels": 2, "sha256": "4444444444444444444444444444444444444444444444444444444444444444", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Clap.wav", "frames": 6000, "channels": 2, "sha256": "5555555555555555555555555555555555555555555555555555555555555555", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Hat Closed.wav", "frames": 2400, "channels": 1, "sha256": "6666666666666666666666666666666666666666666666666666666666666666", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Drums/Hat Open.wav", "frames": 9600, "channels": 1, "sha256": "7777777777777777777777777777777777777777777777777777777777777777", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Bass/Bass A F#1.wav", "frames": 48000, "channels": 1, "sha256": "8888888888888888888888888888888888888888888888888888888888888888", "loop_start": 1000, "loop_end": 47000, "preroll_frames": 0 },
    { "path": "Bass/Bass A C#2.wav", "frames": 47000, "channels": 1, "sha256": "9999999999999999999999999999999999999999999999999999999999999999", "loop_start": 1200, "loop_end": 46000, "preroll_frames": 0 },
    { "path": "Bass/Bass B F#1.wav", "frames": 48000, "channels": 2, "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "loop_start": 2000, "loop_end": 48000, "preroll_frames": 0 },
    { "path": "Music/Pad.wav", "frames": 96000, "channels": 2, "sha256": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Music/Lead C4.wav", "frames": 24000, "channels": 2, "sha256": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Vox/Vox Ahh.wav", "frames": 24000, "channels": 2, "sha256": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "loop_start": 0, "loop_end": 0, "preroll_frames": 960 },
    { "path": "FX/Riser.wav", "frames": 96000, "channels": 2, "sha256": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Loops/Lead Phrase A.wav", "frames": 192000, "channels": 2, "sha256": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Loops/Lead Phrase B.wav", "frames": 190000, "channels": 2, "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 },
    { "path": "Bass/Sub Layer.wav", "frames": 48000, "channels": 1, "sha256": "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210", "loop_start": 0, "loop_end": 0, "preroll_frames": 0 }
  ],
  "instruments": [
    { "id": "kick_a", "name": "Kick A", "lane": "kick", "mix_channel": "kick", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": 0, "start_frames": 0, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": true, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 36, "lo": 36, "hi": 36, "layers": [ { "vel": 127, "rr": [0] } ] } ] },
    { "id": "kick_b", "name": "Kick B", "lane": "kick", "mix_channel": "kick", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": -500, "start_frames": 0, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": true, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 36, "lo": 36, "hi": 36, "layers": [ { "vel": 80, "rr": [1, 2] }, { "vel": 127, "rr": [3, 4] } ] } ] },
    { "id": "clap", "name": "Clap", "lane": "snare", "mix_channel": "snare", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": 0, "start_frames": 0, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": true, "pan_cycle_pm": [-200, 200],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 39, "lo": 39, "hi": 39, "layers": [ { "vel": 127, "rr": [5] } ] } ] },
    { "id": "hat_closed", "name": "Hat Closed", "lane": "hats", "mix_channel": "hats", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": 0, "start_frames": 0, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "hats", "role": "chokes", "fade_us": 4000, "curve": "cos2" }, "duck_key": false,
      "pan_cycle_pm": [-300, 0, 300], "rr_block_16ths": 16, "layered_with": "",
      "zones": [ { "root": 42, "lo": 42, "hi": 42, "layers": [ { "vel": 127, "rr": [6] } ] } ] },
    { "id": "hat_open", "name": "Hat Open", "lane": "hats", "mix_channel": "hats", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": 0, "start_frames": 0, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "hats", "role": "choked", "fade_us": 4000, "curve": "cos2" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 46, "lo": 46, "hi": 46, "layers": [ { "vel": 127, "rr": [7] } ] } ] },
    { "id": "bass_a", "name": "Bass A", "lane": "bass", "mix_channel": "bass", "mode": "sustain_loop", "align": "start",
      "attack_us": 1000, "release_us": 20000, "gain_mdb": 0, "start_frames": 0, "velocity_law": "pow15", "interp": "hermite",
      "mono": true, "glide": { "curve": "raised_cosine", "us": 70000, "dip_us": 30000 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 30, "lo": 0, "hi": 36, "layers": [ { "vel": 100, "rr": [8] } ] },
                 { "root": 37, "lo": 37, "hi": 127, "layers": [ { "vel": 100, "rr": [9] } ] } ] },
    { "id": "bass_b", "name": "Bass B", "lane": "bass", "mix_channel": "bass", "mode": "sustain_loop", "align": "start",
      "attack_us": 0, "release_us": 15000, "gain_mdb": 0, "start_frames": 0, "velocity_law": "pow15", "interp": "sinc",
      "mono": true, "glide": { "curve": "exponential", "us": 18000, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "sub_layer",
      "zones": [ { "root": 30, "lo": 0, "hi": 127, "layers": [ { "vel": 100, "rr": [10] } ] } ] },
    { "id": "sub_layer", "name": "Sub Layer", "lane": "bass", "mix_channel": "sub", "mode": "gated", "align": "start",
      "attack_us": 0, "release_us": 10000, "gain_mdb": -3000, "start_frames": 0, "velocity_law": "pow15", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 30, "lo": 0, "hi": 127, "layers": [ { "vel": 127, "rr": [17] } ] } ] },
    { "id": "pad", "name": "Pad", "lane": "chords", "mix_channel": "chords", "mode": "gated", "align": "start",
      "attack_us": 5000, "release_us": 200000, "gain_mdb": 0, "start_frames": 0, "velocity_law": "pow15", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": true,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 54, "lo": 0, "hi": 127, "layers": [ { "vel": 110, "rr": [11] } ] } ] },
    { "id": "lead", "name": "Lead", "lane": "lead", "mix_channel": "lead", "mode": "gated", "align": "start",
      "attack_us": 0, "release_us": 30000, "gain_mdb": 0, "start_frames": 0, "velocity_law": "pow15", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 60, "lo": 0, "hi": 127, "layers": [ { "vel": 100, "rr": [12] } ] } ] },
    { "id": "vox", "name": "Vox Ahh", "lane": "vox", "mix_channel": "vox", "mode": "one_shot", "align": "start",
      "attack_us": 0, "release_us": 0, "gain_mdb": 0, "start_frames": 0, "velocity_law": "pow15", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 60, "lo": 0, "hi": 127, "layers": [ { "vel": 100, "rr": [13] } ] } ] },
    { "id": "riser", "name": "Riser", "lane": "fx", "mix_channel": "fx", "mode": "one_shot", "align": "end",
      "attack_us": 2000, "release_us": 0, "gain_mdb": -3000, "start_frames": 1000, "velocity_law": "layer_only", "interp": "sinc",
      "mono": false, "glide": { "curve": "none", "us": 0, "dip_us": 0 }, "restrike_continues": false,
      "choke": { "group": "", "role": "none", "fade_us": 0, "curve": "linear" }, "duck_key": false, "pan_cycle_pm": [],
      "rr_block_16ths": 64, "layered_with": "",
      "zones": [ { "root": 48, "lo": 48, "hi": 48, "layers": [ { "vel": 127, "rr": [14] } ] } ] }
  ],
  "keymap": [
    { "ch": 10, "keys": [36, 36], "a": "kick_a", "b": "kick_b", "ab": "kick" },
    { "ch": 10, "keys": [39, 39], "a": "clap", "b": "", "ab": "" },
    { "ch": 10, "keys": [42, 42], "a": "hat_closed", "b": "", "ab": "" },
    { "ch": 10, "keys": [46, 46], "a": "hat_open", "b": "", "ab": "" },
    { "ch": 1, "keys": [0, 127], "a": "bass_a", "b": "bass_b", "ab": "bass" },
    { "ch": 2, "keys": [0, 127], "a": "lead", "b": "", "ab": "" },
    { "ch": 5, "keys": [0, 127], "a": "vox", "b": "", "ab": "" },
    { "ch": 7, "keys": [48, 48], "a": "riser", "b": "", "ab": "" },
    { "ch": 11, "keys": [0, 127], "a": "pad", "b": "", "ab": "" }
  ],
  "extras": [
    { "ch": 11, "name": "Pad", "lane": "chords" }
  ],
  "phrases": [
    { "lane": "lead", "ch": 2, "ab": "bass", "mix_channel": "lead", "gain_mdb": 0, "throw": "live", "stems": { "a": 15, "b": 16 },
      "cycle_16ths": 32, "tolerance_pm": 20, "fade_us": 5000, "fade_in_us": 750,
      "ride": { "cc74": [], "cc71": [] },
      "slices": [
        { "step": 0, "key": 60, "start": 0, "end": 22000, "fade": 21000, "lead": 0, "holds": [] },
        { "step": 4, "key": 62, "start": 21500, "end": 44000, "fade": 43000, "lead": 500, "holds": [62] },
        { "step": 8, "key": 64, "start": 44000, "end": 66000, "fade": 66000, "lead": 0, "holds": [] },
        { "step": 12, "key": 65, "start": 66000, "end": 90000, "fade": 88000, "lead": 0, "holds": [67, 69] }
      ] }
  ]
}
)json";

    // Channels: 0 kick, 1 snare, 2 hats, 3 bass, 4 sub, 5 lead, 6 chords, 7 vox, 8 fx.
    inline constexpr std::string_view kMixJson = R"json({
  "kit": "TST01",
  "content_version": 3,
  "channels": [
    { "id": "kick", "lane": "kick", "group": "drums", "sends_pm": [0, 0, 0], "inserts": [
        { "type": "gain", "gain_mdb": -1500 },
        { "type": "clip", "ceiling_mdb": -3000, "knee_mdb": 1500 } ] },
    { "id": "snare", "lane": "snare", "group": "drums", "sends_pm": [120, 0, 0], "inserts": [
        { "type": "pan", "pan_pm": 100 },
        { "type": "peak", "freq_hz": 200, "gain_mdb": -2000, "q_pm": 1400 },
        { "type": "duck", "mode": "onset", "key_lanes": ["kick"], "min_velocity": 1, "depth_mdb": -2000, "ramp_us": 0,
          "hold_us": 5000, "release_us": 40000, "pre_us": 3000, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": false } ] },
    { "id": "hats", "lane": "hats", "group": "drums", "sends_pm": [0, 200, 0], "inserts": [
        { "type": "highpass", "freq_hz": 300, "order": 2, "q_pm": 0, "stages": 1, "band": { "lo_hz": 0, "hi_hz": 0 } },
        { "type": "width", "width_pm": 1300, "freq_hz": 0, "order": 2 } ] },
    { "id": "bass", "lane": "bass", "group": "music", "sends_pm": [0, 0, 0], "inserts": [
        { "type": "cc_filter", "response": "lowpass", "cc74_lo_hz": 650, "cc74_hi_hz": 2210, "cc74_default": 127,
          "cc71_lo_pm": 707, "cc71_hi_pm": 1600, "cc71_default": 0, "band": { "lo_hz": 120, "hi_hz": 0 } },
        { "type": "duck", "mode": "onset", "key_lanes": ["kick", "snare"], "min_velocity": 1, "depth_mdb": 6000,
          "ramp_us": 2000, "hold_us": 10000, "release_us": 120000, "pre_us": 3000, "band": { "lo_hz": 0, "hi_hz": 120 },
          "pump": true },
        { "type": "mono_below", "freq_hz": 150, "order": 8 } ] },
    { "id": "sub", "lane": "bass", "group": "music", "sends_pm": [0, 0, 0], "inserts": [
        { "type": "lowpass", "freq_hz": 120, "order": 4, "q_pm": 0, "stages": 2, "band": { "lo_hz": 0, "hi_hz": 0 } },
        { "type": "duck", "mode": "onset", "key_lanes": ["kick"], "min_velocity": 20, "depth_mdb": 9000, "ramp_us": 1000,
          "hold_us": 20000, "release_us": 150000, "pre_us": 3000, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": true } ] },
    { "id": "lead", "lane": "lead", "group": "music", "sends_pm": [150, 250, 100], "inserts": [
        { "type": "high_shelf", "freq_hz": 8000, "gain_mdb": 2000, "q_pm": 707 },
        { "type": "drive", "drive_pm": 2000, "bias_pm": 100, "output_mdb": -1000,
          "when": { "variant": "novox", "ab": "none", "is": "a" } } ] },
    { "id": "chords", "lane": "chords", "group": "music", "sends_pm": [300, 0, 50], "inserts": [
        { "type": "low_shelf", "freq_hz": 200, "gain_mdb": -3000, "q_pm": 707 },
        { "type": "comp", "threshold_mdb": -18000, "ratio_pm": 2000, "knee_mdb": 6000, "attack_us": 30000,
          "release_fast_us": 150000, "release_slow_us": 1500000, "makeup_mdb": 2000 },
        { "type": "parallel", "level_mdb": -6000, "inserts": [
            { "type": "drive", "drive_pm": 4000, "bias_pm": 0, "output_mdb": -6000 },
            { "type": "highpass", "freq_hz": 200, "order": 2, "q_pm": 900, "stages": 1, "band": { "lo_hz": 0, "hi_hz": 8000 } } ] } ] },
    { "id": "vox", "lane": "vox", "group": "none", "sends_pm": [400, 300, 0], "inserts": [
        { "type": "gain", "gain_mdb": 1000, "when": { "variant": "any", "ab": "bass", "is": "b" } },
        { "type": "lowpass", "freq_hz": 12000, "order": 2, "q_pm": 900, "stages": 1, "band": { "lo_hz": 0, "hi_hz": 0 } } ] },
    { "id": "fx", "lane": "fx", "group": "music", "sends_pm": [500, 0, 0], "inserts": [
        { "type": "duck", "mode": "level", "key_channel": "vox", "depth_mdb": 4000, "range_mdb": 20000, "top_mdb": -6000,
          "attack_us": 5000, "release_us": 150000, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": false,
          "when": { "variant": "vox", "ab": "kick", "is": "a" } } ] }
  ],
  "groups": {
    "drums": { "inserts": [
      { "type": "comp", "threshold_mdb": -12000, "ratio_pm": 4000, "knee_mdb": 3000, "attack_us": 10000,
        "release_fast_us": 100000, "release_slow_us": 1000000, "makeup_mdb": 0 } ] },
    "music": { "inserts": [ { "type": "peak", "freq_hz": 3000, "gain_mdb": 1000, "q_pm": 1000 } ] }
  },
  "returns": {
    "space": { "delays_frames": [1031, 1327, 1523, 1801, 2063, 2341, 2617, 2903], "signs": [1, -1, 1, -1, 1, 1, -1, -1],
      "t60_ms": 2200, "t60_low_ms": 1500, "low_hz": 250, "damp_hz": 7000, "predelay_us": 20000, "send_hp_hz": 200,
      "send_order": 4, "shimmer_pm": 120, "shimmer_lo_hz": 1500, "shimmer_hi_hz": 9000, "window_frames": 2048, "mono_hz": 150,
      "return_mdb": -6000, "inserts": [
        { "type": "duck", "mode": "level", "key_channel": "vox", "depth_mdb": 6000, "range_mdb": 24000, "top_mdb": -3000,
          "attack_us": 10000, "release_us": 300000, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": false } ] },
    "echo": { "steps": 3, "feedback_pm": 450, "lp_hz": 6000, "hp_hz": 300, "drive_pm": 1500, "wow_ppm": 400, "wow_mhz": 500,
      "flutter_ppm": 100, "flutter_mhz": 6000, "phases_mrad": [0, 3142], "return_mdb": -9000, "inserts": [] },
    "resonator": { "tones_mhz": [46250, 92500, 138750], "tau_ms": 400, "send_hp_hz": 200, "return_mdb": -12000, "inserts": [] }
  },
  "bus_eq": {
    "vox": [ { "type": "peak", "freq_hz": 2500, "gain_mdb": -1000, "q_pm": 800 } ],
    "novox": []
  },
  "master": {
    "lowcut_hz": 25, "lowcut_order": 4, "mono_below_hz": 150,
    "glue": { "ratio_pm": 2000, "knee_mdb": 6000, "attack_us": 30000, "release_fast_us": 150000, "release_slow_us": 1500000 },
    "vox": { "glue_threshold_mdb": -14000, "drive_mdb": 2000 },
    "novox": { "glue_threshold_mdb": -15000, "drive_mdb": 2500 },
    "clip": { "knee_mdb": 1500, "ceiling_mdb": -520 },
    "limiter": { "ceiling_mdb": -1020, "lookahead_us": 5000, "hold_us": 60000, "release_us": 80000 },
    "dc_hz": 5
  },
  "reference": {
    "vox": { "pre_master_lufs_mlu": -14200, "club_lufs_mlu": -9730,
             "channel_true_peak_mdb": [-1000, -3000, -9000, -4000, -6000, -7000, -8000, -5000, -12000] },
    "novox": { "pre_master_lufs_mlu": -14500, "club_lufs_mlu": -9800,
               "channel_true_peak_mdb": [-1000, -3000, -9000, -4000, -6000, -7000, -8000, -120000, -12000] }
  }
}
)json";

    // The manifests' audio, made in code: file i is deterministic and different from every other.
    inline SampleData sampleFor (std::int64_t frames, std::int32_t channels, std::int32_t seed)
    {
        SampleData d;
        d.frames = frames;
        d.channels = channels;
        d.audio.resize (static_cast<std::size_t> (frames * channels));
        for (std::size_t i = 0; i < d.audio.size(); ++i)
            d.audio[i] = static_cast<float> (static_cast<std::int32_t> ((i * 7919u + static_cast<std::size_t> (seed) * 104729u) % 2001u) - 1000) / 1024.0f;
        return d;
    }

    // A sample source in memory: path -> what read () gives. It records the paths it was asked for, and can run a
    // hook before each read (to raise a cancel flag, say).
    class MemorySource : public SampleSource
    {
    public:
        std::map<std::string, util::Result<SampleData>, std::less<>> files;
        std::vector<std::string> reads;
        std::function<void (std::size_t readIndex)> beforeRead;

        util::Result<SampleData> read (std::string_view relativePath) override
        {
            if (beforeRead)
                beforeRead (reads.size());
            reads.emplace_back (relativePath);
            const auto found = files.find (relativePath);
            if (found == files.end())
                return util::Result<SampleData>::failure ("missing: " + std::string (relativePath) + ": not in the test source");
            return found->second;
        }
    };
}
