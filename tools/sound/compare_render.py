"""compare_render: the fidelity report (P12). groovecore render's renders of each kit, held kit by kit and lane by lane to
the approved round 5 loops: the measurement that stands in for listening.

    python3 tools/sound/compare_render.py --renders build/renders [--kit FUT01] [--groovecore build/release/cli/groovecore]
        [--content DIR] [--out build/v0.1-fidelity] [--fresh]

With --groovecore it first renders what the comparisons need and the renders folder lacks (plan(); --fresh renders them
all again), from the checkout's manifests and MIDI and the content folder (--content, else $WUBANSIA_CONTENT_DIR). The
references are the round 5 render folders, build/<kit>-r5 ($WUBANSIA_R5_BUILD or the repo's build/: engine_ref.r5_dir).
It writes <out>/report.md and report.json and exits 1 when a gate fails (2 when something it needs is missing).

The gates (the design's hop 3), each on one loop pass, the renders in aligned time (groovecore render drops the
engine's latency):
- mix: pass 2 of the club render against the Club loop, and pass 2 of the pre-master render (--club off) against the
  Wet loop, as engine_ref.gates() holds the Python reference: LUFS-I within 0.3 LU, short-term loudness 0.5 LU, every
  1/3 octave from 40 Hz to 16 kHz 0.75 dB, true peak 0.5 dB, the full-velocity kicks' crest 0.5 dB;
- dry: pass 2 of the dry renders (--space 0 --echo 0 --club off) with bass A and B against the dry loops, the same
  measures and tolerances (report only where the recipe's dry loop isn't the wet mix less its returns: it says why);
- lanes: what the voices play into each mix channel before its inserts (groovecore render --taps) against the recipe's
  own lane there (mix_trace's capture, engine_ref's input), with each A/B switch's other side on its own lane. A phrase
  lane (a channel a kit.json phrase plays into) nulls to -40 dB or lower one pass at a time, each of its cycles at its
  best whole-sample offset within 3 samples (a cycle at the exact tempo isn't a whole number of samples, so the
  engine's cycles start on different roundings): the first pass from 5 ms on (a start chases its slices in over 5 ms),
  the second outside the 2 ms after each cycle's wrap, where the voice re-syncs over 1 ms by design (the whole pass's
  null is reported too). A sampled lane keeps every 1/3 octave within 40 dB of its loudest within 1 dB on pass 2; its
  null is reported. A failing lane names its likely cause and owner;
- songs: 8 bars of every Song section (--songs: all, both variants with bass A and B; some; none): finite, not silent
  where the drums play, and under the club master's ceiling (true peak at most -0.9 dBTP); their loudness against the
  Club loop is reported.
Reported without a gate: master.py's two-sided limiter hold (--master exact) against the look-back master, and against
the Club loop; the club mix 1 % and 3 % off the kit's tempo, and its phrase lanes against themselves at the kit's
tempo; the phrase lanes on their multisamples (--phrases off: what plays past 2 % and from MIDI in) against the
recipe's lanes, and FUT04's squelch ticks there; the engine's CPU per render, and a stress at 128-sample blocks with
the voices full. Known gaps the lead has accepted (KNOWN) stay in the report, marked with their owner.
"""
import argparse
import json
import math
import os
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402

SR = 48000
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
VARIANTS = ('vox', 'novox')
GATES = {'lufs_i': 0.3, 'lufs_s': 0.5, 'bands': 0.75, 'true_peak': 0.5, 'kick_crest': 0.5}
LANE_NULL_DB = -40.0          # a phrase lane against the recipe's, one pass at a time
LANE_BAND_DB = 1.0            # a sampled lane: each 1/3 octave within LANE_BAND_RANGE_DB of its loudest
LANE_BAND_RANGE_DB = 40.0
CHASE_S = 0.005               # a start chases a phrase's slices in over 5 ms (the sampler's chase fade)
RESYNC_S = 0.002              # after pass 1 a phrase voice re-syncs at each cycle's wrap over 1 ms (P6): this much after
#                               each wrap is left out of the gated pass 2 null (the whole pass's is reported)
MAX_SHIFT = 3                 # samples either way a pass may sit off the recipe's tiled loop
SILENT_DB = -100.0            # a lane quieter than this (RMS, dBFS) is silent
SECTION_CEILING_DB = -0.9     # a Song section's true peak, at most (the limiter's ceiling is -1.02 dBTP)
SECTION_BARS = 8
PASSES = '2'                  # loop renders: pass 1 (from rest, chased) and pass 2 (steady state) are all the checks read

# The round 5 dry loops (bass A and B; FUT04 has no no-vox pair), by kit. The Low loops (the kick and bass lanes alone)
# aren't compared: they are round 4's mixes of those lanes, which round 5 clips and mixes differently.
DRY = {
    'FUT01': {('vox', 'a'): 'GarageVox', ('vox', 'b'): 'GarageVoxWarm',
              ('novox', 'a'): 'GarageNoVox', ('novox', 'b'): 'GarageNoVoxWarm'},
    'FUT02': {('vox', 'a'): 'ContextVoxSynth', ('vox', 'b'): 'ContextVoxSynthWarm',
              ('novox', 'a'): 'ContextNoVox', ('novox', 'b'): 'ContextNoVoxWarm'},
    'FUT03': {('vox', 'a'): 'ContextLoopVox', ('vox', 'b'): 'ContextLoopVoxWarm',
              ('novox', 'a'): 'ContextLoopNoVox', ('novox', 'b'): 'ContextLoopNoVoxWarm'},
    'FUT04': {('vox', 'a'): 'ContextLoopVox', ('vox', 'b'): 'ContextLoopVoxWarm'},
}

# Gaps the lead has accepted for now, with their owner: they stay in the report, marked, and don't fail the run.
FUT04_GRID = ("exact-tempo grid (338823.53 samples a loop) vs the recipe's rounded 338824-sample loop; the kick lands 1 "
              "sample off the bar-anchored stem on some beats (accepted: inaudible as timing; the Club and Wet crests pass)")
KNOWN = {
    ('FUT01', 'lane', 'novox bass B: acc_warp'):
        "the warp stab's release runs 15-30 ms past its 16th, where the recipe gates it: P2 manifest (integration)",
    ('FUT04', 'dry', 'vox bass A vs ContextLoopVox'): FUT04_GRID,
    ('FUT04', 'dry', 'vox bass B vs ContextLoopVoxWarm'): FUT04_GRID,
}

# Who a failing gate most likely belongs to (the plan's packages), by what fails.
OWNERS = {
    'phrase': 'P6 phrase lanes (the voice) or P2 phrase tables (the slices)',
    'sampled': 'P5 sampler (selection, gains, interpolation) or P2 kit manifest (zones, layers, gains, round robins)',
    'timing': 'P7 timeline and scheduler (swing snap, pre-roll, positions)',
    'extra': 'P7 timeline (a note the recipe does not play) or P2 kit manifest (its keymap)',
    'missing': 'P7 timeline (a note that never plays) or P2 kit manifest (its keymap or files)',
    'mix': 'P8 mixer (inserts, returns, the club master) or P3 mix manifest (its values)',
    'master': 'P8 club master (glue, drive, clip, limiter) or P3 mix manifest (the solved values)',
}


# ------------------------------------------------------------------ the renders
@dataclass(frozen=True)
class Render:
    """One groovecore render: its file stem under <renders>/<kit>/, its options (after --kit and --out), whether it
    writes dry taps, and which (--taps-of: lanes or mix channels; empty: all)."""
    name: str
    args: tuple
    taps: bool = False
    taps_of: tuple = ()


def plan_loops(variants=VARIANTS):
    """The loop renders the comparisons need, per variant: the club mix with bass A and B, each with its dry taps; the
    pre-master mix (bass B, as the Wet loop); the dry mix (no returns, no master) with bass A and B. 3 passes each."""
    out = []
    for v in variants:
        out.append(Render(f'loop_{v}_bassb', ('--variant', v, '--bass', 'b', '--passes', PASSES), taps=True))
        out.append(Render(f'loop_{v}_bassa', ('--variant', v, '--bass', 'a', '--passes', PASSES), taps=True,
                          taps_of=('bass', 'kick')))
        out.append(Render(f'loop_{v}_bassb_premaster', ('--variant', v, '--bass', 'b', '--passes', PASSES, '--club', 'off')))
        for b in 'ab':
            out.append(Render(f'loop_{v}_bass{b}_dry', ('--variant', v, '--bass', b, '--passes', PASSES, '--space', '0',
                                                        '--echo', '0', '--club', 'off')))
    return out


RATES = (44100, 96000)        # host rates besides 48 kHz: the engine runs at the host's rate (ADR 0017)


def rate_render(rate, variant='vox'):
    """The club mix at another host rate, bass B (measured after resampling to 48 kHz)."""
    return Render(f'loop_{variant}_bassb_{rate}', ('--variant', variant, '--bass', 'b', '--passes', PASSES, '--rate', str(rate)))


def side_render(switch, side, variant='vox'):
    """The club mix with one A/B switch on its other side (kick A, stab B), with taps: that lane against the recipe's
    lane on that side."""
    return Render(f'loop_{variant}_{switch}{side}', ('--variant', variant, f'--{switch}', side, '--passes', PASSES), taps=True,
                  taps_of=(SWITCH_LANE[switch],))


DEFAULT_SIDE = {'kick': 'b', 'bass': 'b', 'stab': 'a'}
SWITCH_LANE = {'kick': 'kick', 'bass': 'bass', 'stab': 'chords'}


def section_render(variant, bass, number):
    return Render(f'song_{variant}_bass{bass}_s{number:02d}',
                  ('--mode', 'song', '--variant', variant, '--bass', bass, '--section', str(number), '--bars', str(SECTION_BARS)))


TEMPO_PCT = (-3, -1, 1, 3)    # off the kit's tempo: inside the phrase lanes' 2 % (slices anchored on onsets) and past it


def exact_render(variant):
    """The club mix on master.py's two-sided limiter hold (--master exact), bass B: the verification master."""
    return Render(f'loop_{variant}_bassb_exact', ('--variant', variant, '--bass', 'b', '--passes', PASSES, '--master', 'exact'))


def tempo_render(kit_bpm, pct, variant='vox', taps_of=()):
    """The club mix at the kit's tempo moved by pct percent, bass B, with taps (of taps_of: the phrase channels)."""
    bpm = kit_bpm * (1 + pct / 100.0)
    return Render(f'loop_{variant}_bassb_tempo{pct:+d}', ('--variant', variant, '--bass', 'b', '--passes', PASSES, '--tempo',
                                                          f'{bpm:.6f}'), taps=True, taps_of=tuple(taps_of))


def bench_render(variant='vox'):
    """The CPU stress: the club mix at 128-sample blocks with notes held on the kit's polyphonic sustained instruments
    through MIDI in, so the voices fill up (the design's 64 voices and every effect)."""
    return Render(f'loop_{variant}_bassb_bench', ('--variant', variant, '--bass', 'b', '--passes', PASSES, '--block', '128',
                                                  '--hold', '64'))


def multisample_render(variant='vox', taps_of=()):
    """The loop with the phrase lanes on their multisamples (--phrases off), bass B, with taps: what the lanes play off
    the kit's tempo or from MIDI in (FUT04's squelch ticks are measured on it)."""
    return Render(f'loop_{variant}_bassb_multisample', ('--variant', variant, '--bass', 'b', '--passes', PASSES, '--phrases',
                                                        'off'), taps=True, taps_of=tuple(taps_of))


def wav_path(renders, kit, name):
    return Path(renders) / kit.lower() / f'{name}.wav'


def taps_dir(renders, kit, name):
    return Path(renders) / kit.lower() / f'{name}_taps'


def run_render(groovecore, kit, render, renders, root, content=None):
    """Runs groovecore render for one Render (under nice), from the checkout root. Returns (exit code, stderr)."""
    out = wav_path(renders, kit, render.name)
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = ['nice', str(groovecore), 'render', '--kit', kit, *render.args, '--out', str(out),
           '--manifests', str(Path(root) / 'content' / 'plugin'), '--midi', str(Path(root) / 'content' / 'midi')]
    if render.taps:
        cmd += ['--taps', str(taps_dir(renders, kit, render.name))]
        if render.taps_of:
            cmd += ['--taps-of', ','.join(render.taps_of)]
    if content:
        cmd += ['--content', str(content)]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stderr


SONG_SETS = {'all': tuple((v, b) for v in VARIANTS for b in 'ab'),   # the design's: both variants, bass A and B
             'some': (('vox', 'b'), ('novox', 'a')),                    # each variant once, each bass once
             'none': ()}


def ensure(groovecore, kit, renders, root, content=None, fresh=False, log=print, songs='all'):
    """Renders every loop the comparisons need and 8 bars of every Song section (by default in both variants with bass
    A and B; songs='some' or 'none' for fewer) that the renders folder lacks, or all of them with fresh. Returns the
    problems met ([str])."""
    problems = []

    def one(r):
        if not fresh and wav_path(renders, kit, r.name).exists() and meta_of(wav_path(renders, kit, r.name)) is not None \
                and (not r.taps or taps_dir(renders, kit, r.name).exists()):
            return 0, ''
        log(f'  render {kit} {r.name}')
        return run_render(groovecore, kit, r, renders, root, content)

    from tools.sound import plugin_mix as PM
    _, bpm = PM.loop_info(kit)
    lanes = phrase_lanes(kit, root)
    extra = [multisample_render(taps_of=lanes), bench_render()] + [exact_render(v) for v in VARIANTS]
    extra += [tempo_render(bpm, t, taps_of=lanes) for t in TEMPO_PCT]
    extra += [side_render(sw, 'a' if DEFAULT_SIDE[sw] == 'b' else 'b') for sw in switches(kit, root) if sw != 'bass']
    extra += [rate_render(r) for r in RATES]
    for r in plan_loops() + extra:
        code, err = one(r)
        if code != 0:
            problems.append(f'{kit} {r.name}: groovecore render exit {code}: {err.strip()}')
    for v, b in SONG_SETS[songs]:
        for n in range(1, 64):
            code, err = one(section_render(v, b, n))
            if code == 2 and 'no section' in err:
                break
            if code != 0:
                problems.append(f'{kit} song {v} bass {b} section {n}: exit {code}: {err.strip()}')
                break
    return problems


def meta_of(wav):
    """The render's sidecar (FILE.json beside FILE.wav), or None."""
    p = Path(wav).with_suffix('.json')
    try:
        return json.loads(p.read_text())
    except (OSError, ValueError):
        return None


def load(wav, resample=False):
    """(audio (n, 2) float64, sidecar) of a render, or None when it isn't there. A render at another rate is refused,
    or with resample, brought to 48 kHz for measuring (scipy's polyphase resampler)."""
    import soundfile as sf
    wav = Path(wav)
    if not wav.exists():
        return None
    x, sr = sf.read(str(wav), dtype='float64', always_2d=True)
    if sr != SR:
        if not resample:
            raise ValueError(f'{wav}: {sr} Hz; the comparisons are at {SR}')
        from math import gcd
        from scipy import signal
        g = gcd(SR, sr)
        x = signal.resample_poly(x, SR // g, sr // g, axis=0)
    return D.st(x), meta_of(wav)


# ------------------------------------------------------------------ the measures
def db(x):
    return float(10 * np.log10(x)) if x > 0 else -math.inf


def rms_db(x):
    x = np.asarray(x, dtype=float)
    return db(float(np.mean(x ** 2))) if x.size else -math.inf


def null_db(got, want):
    """The residual's energy against the reference's, dB (P2's measure): -inf when equal, +inf against silence."""
    e = float(np.sum((np.asarray(got) - np.asarray(want)) ** 2))
    w = float(np.sum(np.asarray(want) ** 2))
    if e == 0.0:
        return -math.inf
    return db(e / w) if w > 0 else math.inf


def pass_slice(x, L, k, shift=0, start=0):
    """Pass k of a render (samples k L + shift + start .. (k + 1) L + shift), zero-padded where the render ends."""
    a, b = k * L + shift + start, (k + 1) * L + shift
    out = np.zeros((b - a,) + x.shape[1:])
    lo, hi = max(a, 0), min(b, len(x))
    if lo < hi:
        out[lo - a:hi - a] = x[lo:hi]
    return out


def best_null(x, want, L, k, start=0, max_shift=MAX_SHIFT):
    """The null of pass k of x against one loop of the reference (want, L samples) from `start` on, at the whole-sample
    shift within max_shift that nulls best: (null dB, shift)."""
    best = (math.inf, 0)
    ref = want[start:]
    for s in range(-max_shift, max_shift + 1):
        n = null_db(pass_slice(x, L, k, s, start), ref)
        if n < best[0]:
            best = (n, s)
    return best


def cycle_null(x, want, L, k, cycle_16ths, start=0, resync=False, max_shift=MAX_SHIFT):
    """A phrase lane's null on pass k, each of the pass's cycles (cycle_16ths of its 64) at its own best whole-sample
    shift: a cycle at the exact tempo isn't a whole number of samples, so the engine's cycles start on different
    roundings of their positions. The residuals summed over the cycles against the reference's energy, from `start` on,
    and with resync, without the RESYNC_S after each cycle's wrap. Returns (null dB, [shift per cycle])."""
    n = max(1, 64 // cycle_16ths) if cycle_16ths else 1
    edges = [int(round(j * L / n)) for j in range(n + 1)]
    keep = resync_mask(L, cycle_16ths) if resync else np.ones(L, dtype=bool)
    keep[:start] = False
    shifted = {s: pass_slice(x, L, k, s) for s in range(-max_shift, max_shift + 1)}
    num = den = 0.0
    shifts = []
    for a, b in zip(edges, edges[1:]):
        m = keep[a:b]
        ref = want[a:b][m]
        e, s = min((float(np.sum((shifted[s][a:b][m] - ref) ** 2)), s) for s in shifted)
        num += e
        den += float(np.sum(ref ** 2))
        shifts.append(s)
    if num == 0.0:
        return -math.inf, shifts
    return (db(num / den) if den > 0 else math.inf), shifts


def third_octave_db(x, lo=40.0, hi=16000.0):
    from tools.sound import engine_ref as E
    return E.third_octave_db(x, lo, hi)


def band_diff(got, want, range_db=LANE_BAND_RANGE_DB):
    """Each 1/3 octave's level difference (got - want, dB) where the reference is within range_db of its loudest band:
    (worst (centre, dB) or None, bands judged)."""
    a, b = third_octave_db(got), third_octave_db(want)
    top = max(b.values())
    judged = [(fc, a[fc] - b[fc]) for fc in b if b[fc] >= top - range_db]
    if not judged:
        return None, 0
    return max(judged, key=lambda t: abs(t[1])), len(judged)


def measure(x, kicks):
    from tools.sound import engine_ref as E
    return E.measure(x, kicks)


def compare(got, want, kicks, gates=None):
    """engine_ref.compare's readings at the hop 3 tolerances: {'diff', 'pass', 'got', 'want'}."""
    gates = gates or GATES
    a, b = measure(got, kicks), measure(want, kicks)
    worst_band = max(((fc, a['bands'][fc] - b['bands'][fc]) for fc in a['bands']), key=lambda t: abs(t[1]))
    crest = np.abs(np.array(a['kick_crest']) - np.array(b['kick_crest'])) if len(a['kick_crest']) else np.zeros(1)
    diff = {'lufs_i': a['lufs_i'] - b['lufs_i'],
            'lufs_s': float(np.max(np.abs(a['lufs_s'] - b['lufs_s']))),
            'bands': worst_band,
            'true_peak': a['true_peak'] - b['true_peak'],
            'kick_crest': float(np.max(crest))}
    ok = {'lufs_i': abs(diff['lufs_i']) <= gates['lufs_i'], 'lufs_s': diff['lufs_s'] <= gates['lufs_s'],
          'bands': abs(worst_band[1]) <= gates['bands'], 'true_peak': abs(diff['true_peak']) <= gates['true_peak'],
          'kick_crest': diff['kick_crest'] <= gates['kick_crest']}
    return {'diff': diff, 'pass': ok, 'got': {'lufs_i': a['lufs_i'], 'true_peak': a['true_peak']},
            'want': {'lufs_i': b['lufs_i'], 'true_peak': b['true_peak']}}


# ------------------------------------------------------------------ the references
def references(kit, variant):
    """The recipe's capture for a variant (mix_trace.run, cached): its dry lanes per channel and A/B sides."""
    from tools.sound import mix_trace as T
    return T.run(kit, variant)[1]


def lane_reference(cap, channel, bass):
    """The recipe's lane before its inserts on a mix channel for a bass side: the A side where the trace has one."""
    if bass == 'a' and (channel, 'a') in cap.sides:
        return D.st(cap.sides[(channel, 'a')])
    x = cap.dry.get(channel)
    return None if x is None else D.st(x)


def kit_json(kit, root=None):
    root = Path(root) if root else Path(__file__).resolve().parents[2]
    return json.loads((root / 'content' / 'plugin' / kit.lower() / f'{kit}_kit.json').read_text())


def phrase_channels(kit, root=None):
    """The mix channels kit.json's phrases play into."""
    return sorted({p['mix_channel'] for p in kit_json(kit, root)['phrases']})


def phrase_lanes(kit, root=None):
    """The lanes kit.json's phrases play on."""
    return sorted({p['lane'] for p in kit_json(kit, root)['phrases']})


def phrase_cycles(kit, root=None):
    """{mix channel: its phrases' cycle in 16ths} (one cycle per channel: P4's parser enforces it per MIDI channel)."""
    return {p['mix_channel']: p['cycle_16ths'] for p in kit_json(kit, root)['phrases']}


def switches(kit, root=None):
    """The kit's A/B switches (kit.json's kit.ab with labels): a subset of kick, bass, stab."""
    return [k for k, v in kit_json(kit, root)['kit'].get('ab', {}).items() if v.get('a') or v.get('b')]


def resync_mask(L, cycle_16ths, at=RESYNC_S):
    """The samples of one pass a phrase voice's wrap re-syncs may touch: `at` seconds from each cycle's start (a pass
    is 64 16ths), as a boolean mask, True where kept."""
    keep = np.ones(L, dtype=bool)
    if cycle_16ths:
        n = int(round(at * SR))
        for j in range(0, max(1, 64 // cycle_16ths)):
            a = int(round(j * cycle_16ths * L / 64.0))
            keep[a:a + n] = False
    return keep


# ------------------------------------------------------------------ the checks
@dataclass
class Check:
    """One row of the report: what, its readings, whether it passes (None: reported without a gate), and why not."""
    kit: str
    group: str            # mix, dry, lane, master, tempo, song, multisample, cpu
    name: str
    values: dict = field(default_factory=dict)
    ok: object = None
    note: str = ''
    known: str = ''       # an accepted gap: why, and its owner (KNOWN)

    def as_json(self):
        return {'kit': self.kit, 'group': self.group, 'name': self.name, 'ok': self.ok, 'note': self.note, 'known': self.known,
                'values': json.loads(json.dumps(self.values, default=_jsonable))}


def _jsonable(v):
    if isinstance(v, (np.floating, np.integer)):
        return v.item()
    if isinstance(v, np.ndarray):
        return v.tolist()
    if isinstance(v, float) and not math.isfinite(v):
        return None
    return str(v)


def gate_check(kit, group, name, got, want, kicks, note=''):
    r = compare(got, want, kicks)
    failing = [k for k, v in r['pass'].items() if not v]
    why = note
    if failing:
        why = (note + '; ' if note else '') + 'fails ' + ', '.join(failing) + ': likely ' + OWNERS['mix' if group != 'master' else 'master']
    return Check(kit, group, name, {**r['diff'], 'got_lufs_i': r['got']['lufs_i'], 'want_lufs_i': r['want']['lufs_i'],
                                    'got_tp': r['got']['true_peak'], 'want_tp': r['want']['true_peak']},
                 not failing, why)


def mix_checks(kit, renders):
    """Pass 2 of the club and pre-master renders against the Club and Wet loops."""
    from tools.sound import engine_ref as E
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    mix = PM.load(kit)
    kicks = E.full_kicks(kit)
    out = []
    for v in VARIANTS:
        wet_item, club_item = E.items(kit, v)
        club = load(wav_path(renders, kit, f'loop_{v}_bassb'))
        pre = load(wav_path(renders, kit, f'loop_{v}_bassb_premaster'))
        club_ref, club_rep = E.reference(kit, club_item)
        wet_ref, wet_rep = E.reference(kit, wet_item)
        if club is None:
            out.append(Check(kit, 'mix', f'{v}: club vs {club_item}', ok=False, note='no render'))
        else:
            out.append(gate_check(kit, 'mix', f'{v}: club vs {club_item}', club[0][L:2 * L] * D.undb(club_rep['gain_db']),
                                  club_ref, kicks))
        if pre is None:
            out.append(Check(kit, 'mix', f'{v}: pre-master vs {wet_item}', ok=False, note='no render'))
        else:
            out.append(gate_check(kit, 'mix', f'{v}: pre-master vs {wet_item}', E.wet_file(kit, pre[0][L:2 * L], wet_rep, mix),
                                  wet_ref, kicks))
    return out


def dry_checks(kit, renders):
    """Pass 2 of the dry renders (no returns, no master) against the dry loops, bass A and B."""
    from tools.sound import engine_ref as E
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    mix = PM.load(kit)
    kicks = E.full_kicks(kit)
    out = []
    for (v, b), item in DRY.get(kit, {}).items():
        x = load(wav_path(renders, kit, f'loop_{v}_bass{b}_dry'))
        if x is None:
            out.append(Check(kit, 'dry', f'{v} bass {b.upper()} vs {item}', ok=False, note='no render'))
            continue
        ref, rep = E.reference(kit, item)
        out.append(gate_check(kit, 'dry', f'{v} bass {b.upper()} vs {item}', E.wet_file(kit, x[0][L:2 * L], rep, mix), ref, kicks))
    return out


def lane_check(kit, variant, bass, channel, tap, want, L, phrase, cycle_16ths=0, label=None):
    """One mix channel's dry tap against the recipe's lane: a phrase lane's null per pass (pass 2 gated outside its
    wraps' re-syncs), a sampled lane's bands."""
    name = (label or f'{variant} bass {bass.upper()}') + f': {channel}' + (' (phrase)' if phrase else '')
    have_tap = tap is not None and rms_db(pass_slice(tap, L, 1)) > SILENT_DB
    have_ref = want is not None and rms_db(want) > SILENT_DB
    if not have_tap and not have_ref:
        return None
    if not have_ref:
        return Check(kit, 'lane', name, {'tap_rms_db': rms_db(pass_slice(tap, L, 1))}, False,
                     'the recipe plays nothing here: likely ' + OWNERS['extra'])
    if not have_tap:
        return Check(kit, 'lane', name, {'ref_rms_db': rms_db(want)}, False,
                     'the engine plays nothing here: likely ' + OWNERS['missing'])
    first, s1 = best_null(tap, want, L, 0, start=int(round(CHASE_S * SR)))
    second, s2 = best_null(tap, want, L, 1)
    worst, judged = band_diff(pass_slice(tap, L, 1, s2), want)
    level = rms_db(pass_slice(tap, L, 1, s2)) - rms_db(want)
    values = {'null_pass1_db': first, 'shift_pass1': s1, 'null_pass2_db': second, 'shift_pass2': s2,
              'worst_band': worst, 'bands_judged': judged, 'level_db': level}
    if phrase:
        first, s1 = cycle_null(tap, want, L, 0, cycle_16ths, start=int(round(CHASE_S * SR)))
        second, s2 = cycle_null(tap, want, L, 1, cycle_16ths)
        outside, _ = cycle_null(tap, want, L, 1, cycle_16ths, resync=True)
        values.update({'null_pass1_db': first, 'shift_pass1': s1, 'null_pass2_db': second, 'shift_pass2': s2,
                       'null_pass2_outside_resync_db': outside})
        ok = first <= LANE_NULL_DB and outside <= LANE_NULL_DB
        note = '' if ok else (f'nulls {first:.1f} / {outside:.1f} dB outside the re-syncs (gate {LANE_NULL_DB:.0f}): likely '
                              + OWNERS['phrase'])
        if ok and second > LANE_NULL_DB:
            note = (f'pass 2 whole: {second:.1f} dB; outside the 1 ms re-syncs at its cycle wraps (P6, by design): '
                    f'{outside:.1f} dB')
    else:
        ok = worst is not None and abs(worst[1]) <= LANE_BAND_DB
        note = '' if ok else f'{worst[0]:g} Hz off by {worst[1]:+.2f} dB (gate {LANE_BAND_DB:g}): likely ' + OWNERS['sampled']
    return Check(kit, 'lane', name, values, ok, note)


def lane_checks(kit, renders, root=None):
    """Every mix channel's tap against the recipe's lane: the default renders (kick B, bass B, stab A) in both
    variants, and each switch's other side (bass A in both variants; kick A, stab B with vox) on that switch's lane,
    where the recipe's trace holds that side (an info row says where it doesn't)."""
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    phrases = set(phrase_channels(kit, root))
    cycles = phrase_cycles(kit, root)
    kit_switches = switches(kit, root)
    out = []
    for v in VARIANTS:
        cap = references(kit, v)
        jobs = [(f'loop_{v}_bassb', None, None, f'{v} bass B')]
        if 'bass' in kit_switches:
            jobs.append((f'loop_{v}_bassa', 'bass', 'a', f'{v} bass A'))
        if v == 'vox':
            for sw in kit_switches:
                if sw != 'bass':
                    side = 'a' if DEFAULT_SIDE[sw] == 'b' else 'b'
                    jobs.append((side_render(sw, side).name, sw, side, f'{v} {sw} {side.upper()}'))
        for name, switch, side, label in jobs:
            m = meta_of(wav_path(renders, kit, name))
            if m is None:
                out.append(Check(kit, 'lane', label, ok=False, note='no render'))
                continue
            compared = 0
            for c in m['channels']:
                ch = c['id']
                if switch is None:
                    want = lane_reference(cap, ch, 'b')
                else:
                    if c['lane'] != SWITCH_LANE[switch] or (ch, side) not in cap.sides:
                        continue
                    want = D.st(cap.sides[(ch, side)])
                t = load(taps_dir(renders, kit, name) / f'{ch}.wav')
                r = lane_check(kit, v, side or 'b', ch, None if t is None else t[0], want, L, ch in phrases, cycles.get(ch, 0), label)
                if r is not None:
                    out.append(r)
                    compared += 1
            if switch is not None and compared == 0:
                out.append(Check(kit, 'lane', f'{label}: {SWITCH_LANE[switch]} lane', {}, None,
                                 "the recipe's trace holds no lane for this side: covered by the dry loops only"))
    return out


def section_checks(kit, renders):
    """8 bars of every Song section: finite, sounding, under the ceiling; their loudness against the Club loop."""
    from tools.sound import mixmeter as MM
    out = []
    for v in VARIANTS:
        club = load(wav_path(renders, kit, f'loop_{v}_bassb'))
        club_lufs = None
        if club is not None:
            from tools.sound import plugin_mix as PM
            L, _ = PM.loop_info(kit)
            club_lufs = MM.lufs_i(club[0][L:2 * L])
        for b in 'ab':
            for n in range(1, 64):
                got = load(wav_path(renders, kit, section_render(v, b, n).name))
                if got is None:
                    break
                x, m = got
                section = m.get('section') or f'section {n}'
                name = f'{v} bass {b.upper()}: {n:02d} {section} (bar {m.get("start_bar")})'
                if not np.all(np.isfinite(x)):
                    out.append(Check(kit, 'song', name, {}, False, 'NaN or infinity'))
                    continue
                level = rms_db(x)
                if level <= SILENT_DB:
                    out.append(Check(kit, 'song', name, {'rms_db': level}, None, 'silent'))
                    continue
                tp = MM.true_peak_db(x, loop=False)
                lufs = MM.lufs_i(x, loop=False)
                values = {'lufs_i': lufs, 'true_peak': tp, 'vs_club_lu': None if club_lufs is None else lufs - club_lufs,
                          'cpu_percent': m.get('cpu_percent'), 'max_voices': m.get('max_voices')}
                ok = tp <= SECTION_CEILING_DB
                out.append(Check(kit, 'song', name, values, ok,
                                 '' if ok else f'true peak {tp:.2f} dBTP over {SECTION_CEILING_DB}: likely ' + OWNERS['master']))
    return out


def multisample_checks(kit, renders, root=None):
    """The phrase lanes on their multisamples (--phrases off) against the recipe's lanes: reported, no gate. For FUT04
    also the squelch ticks (AcidLine1's 16ths 8 and 24): the bass lane's sub (below 120 Hz) over the 40 ms from each
    tick against the recipe's lane there."""
    from tools.sound import plugin_mix as PM
    L, bpm = PM.loop_info(kit)
    name = multisample_render().name
    m = meta_of(wav_path(renders, kit, name))
    if m is None:
        return []
    cap = references(kit, 'vox')
    phrases = set(phrase_channels(kit, root))
    out = []
    # the whole loop with the phrase lanes on their multisamples, at the kit's tempo, against the Club loop
    from tools.sound import engine_ref as E
    _, club_item = E.items(kit, 'vox')
    club_ref, club_rep = E.reference(kit, club_item)
    mixed = load(wav_path(renders, kit, name))
    if mixed is not None:
        r = compare(mixed[0][L:2 * L] * D.undb(club_rep['gain_db']), club_ref, E.full_kicks(kit))
        out.append(Check(kit, 'multisample', f'club mix with phrases off vs {club_item}', r['diff'], None,
                         'the phrase lanes on their multisamples at the kit tempo'))
    lanes = {c['id']: c['lane'] for c in m['channels']}
    for lane in sorted({lanes[ph] for ph in phrases if ph in lanes}):
        # the lane's channels together, the engine's and the recipe's: the multisample may sit on a sibling channel
        got = lane_sum(renders, kit, name, m, lane)
        want = None
        for ch, ln in lanes.items():
            r = lane_reference(cap, ch, 'b') if ln == lane else None
            if r is not None:
                want = r if want is None else want + r
        if got is None or want is None:
            continue
        ph = '+'.join(sorted(c for c in phrases if lanes.get(c) == lane))
        worst, judged = band_diff(pass_slice(got, L, 1), want)
        values = {'null_pass2_db': null_db(pass_slice(got, L, 1), want), 'worst_band': worst, 'bands_judged': judged,
                  'level_db': rms_db(pass_slice(got, L, 1)) - rms_db(want)}
        if kit == 'FUT04' and lane == 'bass':
            values['squelch_ticks'] = squelch_ticks(pass_slice(got, L, 1), want, bpm, L)
        out.append(Check(kit, 'multisample', f'{lane} lane ({ph}) on the multisample', values, None,
                         'phrases off: what plays off the kit tempo and from MIDI in'))
    return out


def squelch_ticks(got, want, bpm, L):
    """FUT04's squelch ticks: for each tick (16ths 8 and 24 of each 2-bar cycle), the sub band (below 120 Hz) over 40 ms
    from the tick, engine against recipe, dB; and the same window's full band."""
    from scipy import signal
    sos = signal.butter(4, 120.0, 'low', fs=SR, output='sos')
    lo_got, lo_want = signal.sosfilt(sos, got, axis=0), signal.sosfilt(sos, want, axis=0)
    s16 = L / 64.0
    rows = []
    for cycle in (0, 32):
        for step in (8, 24):
            a = int(round((cycle + step) * s16))
            b = a + int(0.040 * SR)
            rows.append({'16th': cycle + step, 'sub_db': rms_db(lo_got[a:b]) - rms_db(lo_want[a:b]),
                         'sub_got_dbfs': rms_db(lo_got[a:b]), 'full_db': rms_db(got[a:b]) - rms_db(want[a:b])})
    return rows


def master_checks(kit, renders):
    """The look-back master (the plugin's) against master.py's two-sided hold on the same mix, pass 2, and the exact
    one against the Club loop: reported, no gate (the gates hold the look-back master)."""
    from tools.sound import engine_ref as E
    from tools.sound import mixmeter as MM
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    kicks = E.full_kicks(kit)
    out = []
    for v in VARIANTS:
        look = load(wav_path(renders, kit, f'loop_{v}_bassb'))
        exact = load(wav_path(renders, kit, exact_render(v).name))
        if look is None or exact is None:
            continue
        a, b = look[0][L:2 * L], exact[0][L:2 * L]
        r = compare(a, b, kicks)
        values = {**r['diff'], 'lookback_lufs_i': MM.lufs_i(a), 'exact_lufs_i': MM.lufs_i(b),
                  'lookback_tp': MM.true_peak_db(a), 'exact_tp': MM.true_peak_db(b), 'null_db': null_db(a, b),
                  'latency_lookback': look[1].get('latency'), 'latency_exact': exact[1].get('latency')}
        out.append(Check(kit, 'master', f'{v}: look-back against exact (same mix)', values, None,
                         "the DC stage is the causal 5 Hz blocker in both (master.py's zero-phase clean can't stream)"))
        _, club_item = E.items(kit, v)
        club_ref, club_rep = E.reference(kit, club_item)
        c = gate_check(kit, 'master', f'{v}: exact master vs {club_item}', b * D.undb(club_rep['gain_db']), club_ref, kicks)
        c.ok = None
        out.append(c)
    return out


def tempo_checks(kit, renders, root=None):
    """The club mix off the kit's tempo, pass 2 (that tempo's loop length), against the Club loop: loudness, bands,
    true peak and the kick crest (not short-term loudness, whose curve is in time); and each phrase lane's tap against
    its own at the kit's tempo: its bands, and its clicks (samples whose second difference passes the loudest at the
    kit's tempo by 6 dB). Reported, no gate: the gates are at the kit's tempo."""
    from tools.sound import engine_ref as E
    from tools.sound import plugin_mix as PM
    L, bpm = PM.loop_info(kit)
    kicks = E.full_kicks(kit)
    base = meta_of(wav_path(renders, kit, 'loop_vox_bassb'))
    _, club_item = E.items(kit, 'vox')
    club_ref, club_rep = E.reference(kit, club_item)
    out = []
    for pct in TEMPO_PCT:
        r = tempo_render(bpm, pct)
        got = load(wav_path(renders, kit, r.name))
        if got is None:
            continue
        x, m = got
        Lt = int(round(L * bpm / m['bpm']))
        a = x[Lt:2 * Lt] * D.undb(club_rep['gain_db'])
        ga, gb = measure(a, [int(round(k * Lt / L)) for k in kicks]), measure(club_ref, kicks)
        worst_band = max(((fc, ga['bands'][fc] - gb['bands'][fc]) for fc in ga['bands']), key=lambda t: abs(t[1]))
        crest = np.abs(np.array(ga['kick_crest']) - np.array(gb['kick_crest'])) if len(ga['kick_crest']) else np.zeros(1)
        values = {'bpm': m['bpm'], 'lufs_i': ga['lufs_i'] - gb['lufs_i'], 'bands': worst_band,
                  'true_peak': ga['true_peak'] - gb['true_peak'], 'kick_crest': float(np.max(crest))}
        out.append(Check(kit, 'tempo', f'{pct:+d} % ({m["bpm"]:.2f} BPM): club vs {club_item}', values, None,
                         'inside the phrase lanes\' 2 %: slices anchored on their onsets' if abs(pct) <= 2 else
                         'past 2 %: the phrase lanes play their multisamples'))
        if base is None:
            continue
        for lane in phrase_lanes(kit, root):
            # the lane's channels together: past 2 % its notes play the multisample, which may sit on a sibling channel
            here, there = lane_sum(renders, kit, r.name, m, lane), lane_sum(renders, kit, 'loop_vox_bassb', base, lane)
            if here is None or there is None or rms_db(pass_slice(there, L, 1)) <= SILENT_DB:
                continue
            here, there = pass_slice(here, Lt, 1), pass_slice(there, L, 1)
            worst, judged = band_diff(here, there)
            out.append(Check(kit, 'tempo', f'{pct:+d} %: {lane} lane (phrase) against itself at the kit tempo',
                             {'worst_band': worst, 'bands_judged': judged, 'level_db': rms_db(here) - rms_db(there),
                              'clicks': clicks(here, there)}, None, ''))
    return out


def rate_checks(kit, renders):
    """The club mix at 44.1 and 96 kHz, resampled to 48 kHz for measuring, pass 2, against the Club loop with every
    gate, and against the 48 kHz render's own readings. Reported: the gates are held at 48 kHz, where the samples are."""
    from tools.sound import engine_ref as E
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    kicks = E.full_kicks(kit)
    _, club_item = E.items(kit, 'vox')
    club_ref, club_rep = E.reference(kit, club_item)
    base = load(wav_path(renders, kit, 'loop_vox_bassb'))
    out = []
    for rate in RATES:
        got = load(wav_path(renders, kit, rate_render(rate).name), resample=True)
        if got is None:
            continue
        a = got[0][L:2 * L] * D.undb(club_rep['gain_db'])
        r = compare(a, club_ref, kicks)
        values = dict(r['diff'])
        if base is not None:
            values['vs_48k'] = compare(a, base[0][L:2 * L] * D.undb(club_rep['gain_db']), kicks)['diff']
        out.append(Check(kit, 'rate', f'{rate} Hz: club vs {club_item}', values, None,
                         f"resampled to 48 kHz to measure; latency {got[1].get('latency')} samples at {rate} Hz"))
    return out


def lane_sum(renders, kit, name, meta, lane):
    """A render's taps of every mix channel on a lane, summed; None when none was written."""
    x = None
    for c in meta['channels']:
        if c['lane'] == lane:
            t = load(taps_dir(renders, kit, name) / f'{c["id"]}.wav')
            if t is not None:
                x = t[0] if x is None else x + t[0]
    return x


def clicks(x, ref, margin_db=6.0):
    """Samples of x whose second difference (either side) passes ref's largest by margin_db: steps a fade didn't
    smooth."""
    d2 = lambda u: np.abs(np.diff(D.st(u), n=2, axis=0)).max(axis=1)  # noqa: E731
    top = float(d2(ref).max()) if len(ref) > 2 else 0.0
    if top <= 0.0:
        return 0
    return int(np.count_nonzero(d2(x) > top * 10 ** (margin_db / 20)))


def cpu_checks(kit, renders):
    """The engine's CPU per render (the render thread's CPU clock against the audio's length): the loops at 512-sample
    blocks and the stress at 128 with the voices full. Reported: this machine isn't the macos-15 runner the 15 % gate
    is measured on."""
    out = []
    for r in plan_loops() + [bench_render()]:
        m = meta_of(wav_path(renders, kit, r.name))
        if m is not None:
            out.append(Check(kit, 'cpu', r.name, {'cpu_percent': m.get('cpu_percent'), 'max_voices': m.get('max_voices'),
                                                   'p99_block_percent': m.get('p99_block_percent'),
                                                   'worst_block_percent': m.get('worst_block_percent'),
                                                   'block': m.get('block'), 'held_notes': m.get('held_notes')}, None, ''))
    return out


GROUPS = ('mix', 'dry', 'lane', 'master', 'tempo', 'rate', 'song', 'multisample', 'cpu')


def run_kit(kit, renders, root=None, groups=GROUPS):
    out = []
    fns = {'mix': mix_checks, 'dry': dry_checks, 'lane': lambda k, r: lane_checks(k, r, root), 'song': section_checks,
           'master': master_checks, 'tempo': lambda k, r: tempo_checks(k, r, root), 'rate': rate_checks,
           'multisample': lambda k, r: multisample_checks(k, r, root), 'cpu': cpu_checks}
    for g in groups:
        out += fns[g](kit, renders)
    return out


# ------------------------------------------------------------------ the report
def _fmt(v, digits=2):
    if v is None:
        return ''
    if isinstance(v, (tuple, list)) and len(v) == 2 and all(isinstance(u, (int, float)) for u in v):
        return f'{v[1]:+.{digits}f} @ {v[0]:g} Hz'
    if isinstance(v, float):
        return '-inf' if v == -math.inf else ('inf' if v == math.inf else f'{v:+.{digits}f}')
    return str(v)


def _shifts(s):
    return '/'.join(f'{u:+d}' for u in s) if isinstance(s, (list, tuple)) else f'{s:+d}'


def verdict(ok, known=''):
    if ok is False and known:
        return 'known gap'
    return {True: 'pass', False: '**FAIL**', None: 'info'}[ok]


def mark_known(checks):
    """Marks the checks KNOWN names (in place) and returns the ones that fail the run: failing and not known."""
    for c in checks:
        c.known = KNOWN.get((c.kit, c.group, c.name), '') if c.ok is False else ''
    return [c for c in checks if c.ok is False and not c.known]


def report_md(checks, kits):
    lines = ['# v0.1 fidelity report', '',
             'groovecore render against the approved round 5 loops (compare_render.py). Pass 2 unless named; renders '
             'in aligned time. Gates: LUFS-I ±0.3 LU, short-term ±0.5 LU, 1/3 octaves 40 Hz-16 kHz ±0.75 dB, true peak '
             f'±0.5 dB, kick crest ±0.5 dB; phrase lanes null ≤ {LANE_NULL_DB:.0f} dB per pass; sampled lanes ±{LANE_BAND_DB:g} dB '
             'per 1/3 octave.', '']
    failed = mark_known(checks)
    known = [c for c in checks if c.known]
    lines += [f'**{len(failed)} failing** of {sum(c.ok is not None for c in checks)} gated checks'
              + (f', and {len(known)} known gaps.' if known else '.'), '']
    for kit in kits:
        ks = [c for c in checks if c.kit == kit]
        if not ks:
            continue
        lines += [f'## {kit}', '']
        gate_rows = [c for c in ks if c.group in ('mix', 'dry', 'master', 'rate') or (c.group in ('tempo', 'multisample') and 'lufs_i' in c.values)]
        if gate_rows:
            lines += ['| check | LUFS-I | short-term | worst 1/3 oct | true peak | kick crest | result |',
                      '|---|---|---|---|---|---|---|']
            for c in gate_rows:
                v = c.values
                lines.append(f"| {c.group}: {c.name} | {_fmt(v.get('lufs_i'))} | {_fmt(v.get('lufs_s'))} | {_fmt(v.get('bands'))} "
                             f"| {_fmt(v.get('true_peak'))} | {_fmt(v.get('kick_crest'))} | {verdict(c.ok, c.known)} |")
            lines.append('')
        lane_rows = [c for c in ks if (c.group in ('lane', 'multisample') and 'lufs_i' not in c.values) or (c.group == 'tempo' and 'clicks' in c.values)]
        if lane_rows:
            lines += ['| lane | null pass 1 (shift) | null pass 2 (shift) | worst band | level | result |',
                      '|---|---|---|---|---|---|']
            for c in lane_rows:
                v = c.values
                p1 = '' if 'null_pass1_db' not in v else f"{_fmt(v['null_pass1_db'], 1)} ({_shifts(v['shift_pass1'])})"
                p2 = '' if 'null_pass2_db' not in v else (f"{_fmt(v['null_pass2_db'], 1)}" + (f" ({_shifts(v['shift_pass2'])})" if 'shift_pass2' in v else ''))
                if 'null_pass2_outside_resync_db' in v:
                    p2 += f"; {_fmt(v['null_pass2_outside_resync_db'], 1)} outside the re-syncs"
                extra = f" ({v['clicks']} clicks)" if 'clicks' in v else ''
                lines.append(f"| {c.group}: {c.name}{extra} | {p1} | {p2} | {_fmt(v.get('worst_band'))} | {_fmt(v.get('level_db'))} | {verdict(c.ok, c.known)} |")
                if 'squelch_ticks' in v:
                    for t in v['squelch_ticks']:
                        lines.append(f"| ↳ squelch tick 16th {t['16th']} | | | sub {t['sub_db']:+.1f} dB ({t['sub_got_dbfs']:.1f} dBFS) | full {t['full_db']:+.1f} dB | info |")
            lines.append('')
        song_rows = [c for c in ks if c.group == 'song']
        if song_rows:
            lines += ['| Song section (8 bars) | LUFS-I | vs Club loop | true peak | CPU % | voices | result |', '|---|---|---|---|---|---|---|']
            for c in song_rows:
                v = c.values
                lines.append(f"| {c.name} | {_fmt(v.get('lufs_i'))} | {_fmt(v.get('vs_club_lu'))} | {_fmt(v.get('true_peak'))} "
                             f"| {v.get('cpu_percent', '')} | {v.get('max_voices', '')} | {verdict(c.ok, c.known)} |")
            lines.append('')
        cpu_rows = [c for c in ks if c.group == 'cpu']
        if cpu_rows:
            lines += ['| render | block | CPU % of one core | 99 % of blocks under | worst block % | voices (held) |',
                      '|---|---|---|---|---|---|']
            for c in cpu_rows:
                v = c.values
                lines.append(f"| {c.name} | {v.get('block')} | {v.get('cpu_percent')} | {v.get('p99_block_percent')} "
                             f"| {v.get('worst_block_percent')} | {v.get('max_voices')} ({v.get('held_notes') or 0}) |")
            lines.append('')
        notes = [c for c in ks if c.note and c.ok is False and not c.known]
        if notes:
            lines += ['Failing, with the likely cause:', '']
            lines += [f'- {c.group}: {c.name}: {c.note}' for c in notes]
            lines.append('')
        known_rows = [c for c in ks if c.known]
        if known_rows:
            lines += ['Known gaps (accepted for now):', '']
            lines += [f"- {c.group}: {c.name}: {c.note.split(': likely ')[0]}: {c.known}" for c in known_rows]
            lines.append('')
    return '\n'.join(lines) + '\n'


def read_report(folder):
    """The checks a report.json holds, as Checks (for --rewrite: a report again after KNOWN changes)."""
    rows = json.loads((Path(folder) / 'report.json').read_text())
    return [Check(r['kit'], r['group'], r['name'], r['values'], r['ok'], r['note']) for r in rows]


def write_report(checks, kits, out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'report.md').write_text(report_md(checks, kits))
    (out / 'report.json').write_text(json.dumps([c.as_json() for c in checks], indent=1) + '\n')
    return out / 'report.md'


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--renders', default='build/renders')
    ap.add_argument('--kit', action='append', choices=KITS)
    ap.add_argument('--groovecore', help='render what the renders folder lacks with this groovecore')
    ap.add_argument('--content', help='the content folder (default: $WUBANSIA_CONTENT_DIR)')
    ap.add_argument('--fresh', action='store_true', help='render everything again')
    ap.add_argument('--keep-taps', action='store_true', help="keep each kit's dry taps after its checks (about 150 MB a kit)")
    ap.add_argument('--songs', default='all', choices=tuple(SONG_SETS),
                    help="which Song renders to make: all (both variants, bass A and B), some (vox with B, no-vox with A), none")
    ap.add_argument('--out', default='build/v0.1-fidelity')
    ap.add_argument('--only', action='append', choices=GROUPS)
    ap.add_argument('--rewrite', action='store_true', help="write the report again from <out>/report.json (after KNOWN changes)")
    a = ap.parse_args(argv)
    root = Path(__file__).resolve().parents[2]
    kits = a.kit or list(KITS)
    if a.rewrite:
        checks = [c for c in read_report(a.out) if c.kit in kits]
        path = write_report(checks, kits, a.out)
        failed = mark_known(checks)
        print(f'{len(failed)} failing of {sum(c.ok is not None for c in checks)} gated checks: {path}')
        return 1 if failed else 0
    from tools.sound import engine_ref as E
    missing = [k for k in kits if E.r5_dir(k) is None]
    if missing:
        print(f'compare_render: no round 5 render folder for {", ".join(missing)} (set WUBANSIA_R5_BUILD)', file=sys.stderr)
        return 2
    checks = []
    for kit in kits:
        if a.groovecore:
            problems = ensure(a.groovecore, kit, a.renders, root, a.content, a.fresh, songs=a.songs)
            for p in problems:
                print('compare_render:', p, file=sys.stderr)
        checks += run_kit(kit, a.renders, root, tuple(a.only or GROUPS))
        if not a.keep_taps:
            import shutil
            for d in Path(a.renders, kit.lower()).glob('*_taps'):
                shutil.rmtree(d, ignore_errors=True)
    path = write_report(checks, kits, a.out)
    failed = mark_known(checks)
    print(f'{len(failed)} failing of {sum(c.ok is not None for c in checks)} gated checks: {path}')
    for c in failed:
        print(f'  FAIL {c.kit} {c.group}: {c.name}: {c.note}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
