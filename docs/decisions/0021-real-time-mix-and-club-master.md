# 0021. The real-time mix and club master

**Status:** Proposed, 2026-11-05 (draft, for the user's approval, with ADR 0017). It settles ADR 0014 §5 ("What the engine's master takes from this").

## Context
Each round 5 loop's sound is its mix and its club master: `sends.py` (Space, Echo, Resonator, the level duck), `master.py` (the club master), `phrase.pump_db` (the onset ducks) and `dsp48` (filters, oversampling, ADAA curves). All of it runs offline, "round the loop", in float64. The plugin has to play the same mix live, at the host's rate and block size, on the audio thread, from the kit's mix manifest (`<KIT>_mix.json`, P3; `docs/plugin/manifest.md` §3).

Some of the offline mix can't run in real time:
- master.py's limiter holds its gain from 60 ms before a peak to 65 ms after it, so it needs 65.5 ms of latency (3145 samples);
- its DC clean is zero-phase;
- every stage is centred or wraps round the loop.

ADR 0014 §5 left the real-time master's hold and DC clean open, and asked that a look-back hold re-measure aliasing before it is chosen.

## Decision
**1. The engine's mix is ported block by block and proven against the Python.** `engine/fx` holds the blocks, `engine/mix` the graph that plays a manifest. They are plain C++ (no JUCE), with recursive state in double and scalar loops.
- `tools/sound/export_engine_vectors.py` writes 41 reference vectors (251 KiB of float32, pinned by `vectors.sha256`). They come from dsp48, master.py, sends.py, phrase.pump_db, and rt_ref.py's real-time stages (P3).
- Inputs are a splitmix64 stream the C++ rebuilds bit for bit. The loop-based references run on silence-padded streams, so they start from rest as the engine does.
- Tolerance: 1e-6 for linear blocks and 1e-4 for nonlinear ones, as the largest absolute error on inputs peaking at -6 dBFS.
- Measured: every filter, the band splits, the SVF, the oversamplers, the ducks, the resonator, the glue's output and both limiters match their vectors exactly after float32 rounding. The soft clips and the drive are within 6e-8, the Space within 1.5e-8, the echo within 2e-9, and the glue's reduction within 1.2e-7 dB.

**2. A fixed latency per rate, whatever the kit.** The graph aligns every path inside a mix budget of 64 samples:
- channels, groups, returns and the two bus EQs are delayed to meet;
- duck keys and controllers wait as long as the signal at their insert;
- a level duck whose key channel is later than its own signal delays that signal.

The club master follows at a fixed latency. In all:
- 424 samples at 48 kHz (8.8 ms);
- 404 at 44.1 kHz (9.2 ms);
- 712 at 96 kHz (7.4 ms).

The latency is reported in prepare, stays the same with the club master off (its bypass waits as long), and the tail is 6 s.
- The four round 5 mixes need 54, 54, 50 and 54 samples (FUT01-FUT04).
- P3's exporter refuses a mix whose longest path needs more than 64. A mix that does anyway is marked over budget, and its late paths stay late.

**3. The insert set.** It is closed: gain, pan, width, peak and shelves, high- and low-pass, cc_filter, mono_below, clip, drive, duck, comp, parallel (manifest §3.2).
- A `when`-restricted insert crossfades over 10 ms against a bypass delayed as long as itself. It keeps running while bypassed, so it fades in warm.
- Lane levels, mutes and the Space, Echo and Pump macros move over 10 ms.
- A filter's band is a crossover (rest + f(band)). A duck's band is additive, x + (g - 1)·band(x), so the channel is untouched at rest.
- Block-based stages count their blocks from where aligned time 0 reaches them in the graph, as engine_ref's streams do: the glue's 16 samples and the level duck's 64. So do absolute-time ones: the echo's wow and flutter, the shimmer's taps.

**4. Lane clips and drives run on a 4x half-band cascade; the club master's clip keeps os_fir(4)'s taps, at a fixed internal rate.**
- The cascade is rt_ref's `halfband(47, 7.0)` for 1x↔2x and `halfband(15, 7.0)` for 2x↔4x, with 27 samples of latency.
- On a 5 kHz sine 6 dB over the knee, the loudest alias under 20 kHz reads -93.3 dB (the gate is -60 dB).
- Against master.soft_clip on drum material, the residual under 16 kHz is -69 dB. In stereo it costs 41 ns a sample, against 118 ns for the same clip on os_fir(4): about 2.9 times cheaper.
- The master's clip keeps os_fir(4)'s exact taps, at 176.4 or 192 kHz whatever the rate: 4x at 44.1 and 48 kHz, 2x at 88.2 and 96, 1x at 176.4 and 192, with 1 ms of latency (48 samples at 48 kHz).
- At a plain 4x of the rate, 96 kHz differed from 48 kHz. The clip's ADAA applies its excess half an oversampled sample late (2.6 µs at 48 kHz, 1.3 at 96 kHz), and os_fir(4) kept the clip's harmonics up to 44 kHz. The limiter took about 0.13 dB less: P12 read the club loops up to 0.19 LU louder, and their true peak up to 0.55 dB higher resampled to 48 kHz. On the fixed rate, a driven test signal reads within 0.005 dB of gain reduction and 0.003 LU at 96 and 192 kHz (`engine_tests "[master]"`).

**5. The limiter's hold looks back, over twice the hold, and reaches 24 samples past its lookahead.**
- Its minimum runs over need[k - 2h .. k + w + 24]: h is the 60 ms hold and w the 5 ms lookahead. Latency: 2·round(0.0025·fs) + 49 samples, 289 at 48 kHz, against master.py's 3145.
- **The 24 samples are the true-peak FIR's reach.** With the minimum ending at the lookahead, the gain was still ramping inside the interpolation kernel round each peak. Over 40 harsh signals per rate the 4x peaks overshot the ceiling: worst -1.0090 / -1.0075 / -1.0139 dBTP at 44.1 / 48 / 96 kHz. With the reach: -1.0198 / -1.0200 / -1.0200, the same as master.py's two-sided hold; the 0.0002 dB at 44.1 kHz is the 4x grid's own residual.
- **Twice the hold back** makes the window as long as the two-sided one (125 ms). A held low note's 4x peaks drift with the sample grid: 55 Hz is 872.7 samples a period, so its peaks beat over ~3.7 cycles. A 65.5 ms window (3.6 cycles) kept losing its lowest need, and the gain stepped every few cycles.
- Measured with master.py's own fixture (`test_no_aliasing_on_a_driven_tone`, qa's alias check, -60 dB limit), the real-time chain at master.py's solved glue and drive:

| Driven saw | exact master.py | look-back, 60 ms back | look-back, 120 ms back |
|---|---|---|---|
| 55 Hz, 3 dB | -68.3 | -60.6 | -67.7 |
| 55 Hz, 6 dB | -71.4 | **-54.8** | -76.4 |
| 110 Hz, 3 dB | -102.0 | **-57.2** | -93.6 |
| 110 Hz, 6 dB | -98.8 | **-58.4** | -92.1 |
| 220 Hz-7 kHz | -77.7 to -103.4 | unchanged | unchanged |

  `engine_tests` keeps the 55, 110 and 220 Hz cases at -60 dB, through the whole club master. A bare 55 Hz saw at 6 dB with no glue leaves -57 dB in master.py's two-sided limiter too, so the test uses master.py's glued fixture.
- Each peak still holds the gain down for 125 ms, as in master.py, now after the peak instead of round it. P3 measures the look-back master within 0.14 LU of the exact one on every loop (the worst, FUT04 novox, -0.140 LU), with its true peak at -1.020 dBTP on all eight.
- master.py's two-sided hold stays available for `groovecore render`'s verification renders: `Mixer::prepare (rate, block, outputs, MasterHold::twoSided)`, with its own fixed latency (3280 samples at 48 kHz).

**6. A causal DC blocker before the limiter.** It is a 2nd-order Butterworth high-pass at the manifest's `dc_hz` (5 Hz), after the clip, and it replaces master.py's zero-phase clean. It is ADR 0014 §5's option (a), with the limiter after it, so no catch limiter is needed.

**7. The solved values are fixed.**
- The glue runs at the variant's solved threshold. A variant change moves it at once and the envelopes glide.
- The drive is per variant, moved over 10 ms.
- Clip ceilings are absolute.
- The level duck's top is the key's loudest block in the reference loop.

**8. Real-time rules.**
- `Mixer::prepare` allocates every line for the manifest's largest values at the rate: the echo holds 16 16ths at 40 BPM, the FDN 1 s lines, the shimmer window 16384 frames, 512 insert slots. That is 10.9 MB per instance at 48 kHz and 34 MB at 192 kHz.
- Adopting a mix builds the plan in place, with no allocation.
- The mixer runs with flush-to-zero, so silent tails never go subnormal.
- The FIRs sum in four partial sums joined in a fixed order: the same bytes on every run and IEEE platform, about four times faster than one running sum. They pair their symmetric taps (checked at compile time), which halves their multiplies.
- The output is identical for any host block size.

## Consequences
- The plugin plays the round 5 mixes live:
  - its whole chain matches engine_ref's streaming render (P3) of all eight loops, FUT01-FUT04 with and without the vocal, with residuals of -137.5 to -139.9 dB on Linux (the gate is -80 dB), no sample more than 3.6e-7 apart;
  - on FUT01's vox loop every channel matches to -141..-157 dB and the pre-master bus to -139.6 dB.
- The match needs the timeline (P7) to send each lane's keys early by its onset ducks' `pre_us` (manifest §3.2), as engine_ref does. Without it FUT02's vox loop reads -40.6 dB.
- The total latency is 8.8 ms at 48 kHz, so live MIDI in plays that much late. Hosts compensate playback, and the latency doesn't change with the kit, the variant or the club master switch.
- The look-back master changes peaks and transients slightly against the approved club loops. P3 measures it per kit (within 0.14 LU integrated, up to 0.196 LU short-term), and P12's fidelity runs gate it.
- CPU at 48 kHz on this session's shared x86 machine, in the thread's CPU time, with the rest unmeasured, notably the Mac:
  - one round 5 kit's whole mix and club master: 0.7 to 1.35 µs a sample, 3.3 to 6.5 % of a core. FUT03 is the lightest; FUT01, with nine lane clips, the heaviest. Blocks of 128 and 512 samples cost the same.
  - the club master: 1.1 %;
  - a lane clip: 0.27 %;
  - the Space: 0.46 %;
  - the limiter: 0.35 %;
  - a busy five-channel test mix with every effect: 3.8 %.

  The same figures come from `engine_tests "[cpu]"` on any machine, and a kit's from `"[wholechain]"`.

## Alternatives
- **master.py's two-sided hold in real time.** It is exact, but it adds 65.5 ms of latency to every note, and 69 ms in all.
- **A 0.02 dB lower limiter ceiling instead of the 24-sample reach.** It passes the overshoot tests, but it doesn't remove the cause and it costs loudness.
- **A 60 ms look-back window.** It fails master.py's alias test on driven 55 and 110 Hz tones (above).
- **os_fir(4) for every clip and drive.** About 2.9 times the CPU, for a difference of -69 dB under 16 kHz.
- **A latency that follows the kit's mix.** Hosts handle latency changes badly, and the plugin reports its latency before any kit is loaded.
- **A fixed 48 kHz engine with an output resampler.** It would remap every event at 44.1 kHz (ADR 0017, rejected there).
