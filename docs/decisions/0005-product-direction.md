# 0005. Product direction: a futuristic groove instrument, built one kit at a time

**Status:** Accepted, 2026-10-29.

## Context
On 2026-10-29 the user reset the product's goals:
1. Cool futuristic samples in four directions: sci-fi club; future garage / 2-step; hyper-clean future house; dark industrial / techno.
2. MIDI that isn't written like AI, with the generator rethought.
3. Effects.
4. An impressive, fun UI that is also pro and slick.

The next step is a one-kit vertical slice that tests all four goals small before scaling. Only macOS matters for now.

An audit of legacy `core.js` (516 exported files, 903,129 notes) shows why the current generator reads as machine-made:
- every kit part is 4 bars, and 82% of drum lanes repeat one bar four times
- all 1,018 chord stacks share onset, length and velocity
- one swing value per kit (0.52–0.62), the same on every lane and section (`legacy/web-pack/core.js:1364-1367`)
- drum velocities come from at most 9 levels (`core.js:1368`)
- full-song fills fall on bar 8 of 8 in 412 of 412 cases (`core.js:2159`)
- 12 of 43 kits share one breakdown progression, and 89% of chords in the minor kits are i, bVI, iv or v
- `buildSong` has no PRNG: one form per family, 7 forms for 43 kits

ROADMAP M2 planned a byte-exact port of this generator, and M1b a re-render of its sound library. Both would reproduce these tells before fixing them.

The S1 plan (`docs/plans/S1-futuristic-slice.md`) proposed decisions D1 to D5. The user listened to 17 prototype sounds and approved it with the recommended defaults ("lets build"). Their feedback on the prototypes: "mostly good, the kicks are a bit muddy, I hate muddy".

## Decision
**D1 to D5, as approved:**
- **D1, the slice kit:** FUT01 "Alloy Signal", sci-fi club × future garage / 2-step, 134 BPM, F# minor (11A). A core set of about 373 sounds comes first; full depth only after the S1h sign-off.
- **D2, the legacy generator and library:** the M2 port of `core.js` and the M1b HD re-render are dropped. `core.js` is frozen as a test baseline: a hash manifest, committed expected metrics, and listening condition L. QuickJS stays the escape hatch if the classic kits are ever wanted. The 43 classic kits stay in the web app and aren't in v1.
- **D3, new dependencies:** Vite, TypeScript, Playwright (+ WebKit), one exact Node LTS through `.nvmrc` and `actions/setup-node`, the fonts Space Grotesk, JetBrains Mono and Doto (OFL-1.1, the only ones that ship), and the Kokoro-82M and Whisper base.en weights at build time only. Nothing else. Versions and licenses are in the plan's §6.1 and ROADMAP §9.
- **D4, vocal masters:** only through WORLD or vocoder resynthesis at 48 kHz from features of the 24 kHz Kokoro take, with a synthetic top band (`extend_hf`) tagged `hf_synthetic`. This applies to vocals only. Plain resampling of TTS audio, varispeed chops included, is banned for masters. ADR 0007 writes it into the master spec.
- **D5, offline bounce:** only in non-realtime renders (`isNonRealtime()`), `processBlock` may wait, bounded at 30 s, on an atomic "ready" flag for the kit and timeline. Real-time playback never waits. This is the single exception to the audio-thread rules (ADR 0014, CLAUDE.md).

**What follows:**
1. **Product.** Wubansia is a futuristic groove instrument with four directions, named in plain words. Artist names never appear in product, sound, file or preset strings. The legacy artist-styled family titles (`legacy/web-pack/app.js:13-15`) are not carried over.
2. **Generator.** A new deterministic, integer-only generator (`core/gen`, "v2") replaces the port.
3. **Sounds.** Each new kit is rendered natively at 48 kHz / 24-bit from `tools/sound`, with its own kit DNA. The old library stays at 44.1 kHz / 16-bit for the web app.
4. **No mud (the user's rule).** Measured on the prototype kick: its 150–400 Hz band takes 771 ms to fall 30 dB, and after 80 ms it sits only 2.4 dB under the sub. Every kick, bass, tom and 2-step boom must now pass a QA gate (plan §5.2, ADR 0009): the 150–400 Hz band falls 30 dB within 120 ms, stays at least 20 dB under 30–120 Hz after 80 ms, and is at most −12 dB of the sound; 400 Hz–1 kHz falls 30 dB within 150 ms. Pads, stabs, textures and chops are high-passed at about 100 Hz. The gate also runs on a 2-step loop. The prototype kick fails it, and the kick is being redesigned to pass it.
5. **Effects are first class:** our own chain in `engine/` on JUCE `dsp` building blocks, including at least one effect driven by the timeline (S1's chord-tracking resonator).
6. **UI.** A WebView UI: lit hardware with pro restraint (stack in ADR 0017).
7. **Order.** M0 (done) → S1 → S2 → M3 → M4 → M5 → M6 → M7. S2 adds future house and techno, with acid as the first engine voice, and FUT01's full depth if the S1h go asks for it.
8. **Platform.** macOS universal only. Windows is deferred (ADR 0004 update of 2026-10-29).
9. **"New kit"** in the plugin means new MIDI from the generator over shipped sound sets. New sound sets (new DNA) are made offline.
10. **Workflow.** S1 runs as steps (S1.0–S1h), one branch and one PR per step. CLAUDE.md changes only through an exact diff the user approves line by line.

## Consequences
- ROADMAP is revised: S1 and S2 replace M1, M1b and M2, and M3–M5 and M7 are re-scoped. M3 keeps the RTSan run promised in ADR 0001 D5.
- "Golden" now means only the legacy baseline manifest and its expected metrics, which are never regenerated to pass a test. Generator snapshots are separate: they change only in their own commits and are frozen at the S1 sign-off (ADR 0010).
- The 43 classic kits can't play in the plugin. They can return later as StyleSpecs or through QuickJS.
- Content is per kit: about 82 MB of FLAC for FUT01's core set, and about 276 MB at full depth.
- The legacy web app is untouched apart from golden export, and keeps its artist-styled titles as a personal reference. If it becomes the public demo (ROADMAP M7), they get plain genre names first.

## Alternatives
- **Keep the plan** (port `core.js` in M2, re-render in M1b, add variation in M5). Rejected: M–L of work reproducing measured tells before fixing them.
- **Port `core.js` after S1 as a "Classic" mode.** Rejected for now: costly for a generator the user calls AI-written. QuickJS is cheaper if it's ever wanted.
- **A WebAudio prototype first, or WebAssembly first.** Rejected in ADR 0006.
- **A pure future-garage pilot.** Rejected: less DSP coverage and a weaker sci-fi identity.
- **Windows in parallel.** Deferred by the user.

## Sources
- `docs/plans/S1-futuristic-slice.md` (§2.2, §4.3, §5.2, §6.1, §8, §10) and its feasibility and creative reviews; the legacy-audit research, whose scripts are ported into `tools/legacy_audit/` in S1b.
- `docs/listening.md`, round 0 (the prototype listen and the kick measurements).
- `docs/ROADMAP.md` at `3745e15` (§2; §5 M1, M1b, M2, M5); CLAUDE.md, "Content rules" and "Determinism and golden files"; ADR 0001 D5; ADR 0004, update of 2026-10-29.
- `legacy/web-pack/core.js:1364-1368, 1456, 2159`; `legacy/web-pack/app.js:13-15`.
