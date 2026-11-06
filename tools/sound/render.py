"""Render a kit to 48 kHz / 24-bit FLAC masters, run sample QA on the files, write the reports and sheets.

    python3 tools/sound/render.py --kit FUT01 [--set s1a] [--only NAME ...] [--out build/s1a-listen]
                                  [--no-sheets] [--repro] [--jobs N]

Any kit with a module in tools/sound/kits/ renders (kits.names()); one whose DNA still has a TODO field is refused.

Per item: render every take (note x layer x round robin), high-pass it (Item.hp_hz), finish its edges (one-shots are
trimmed at -90 dB re peak and faded; loops keep their length), then give the whole instrument one gain: the loudest
take's true peak at -1.02 dBTP, or the item's LUFS target, capped there. Files are <KIT>_<Group>_<Sound>_v<layer>_
rr<n>.flac, mono where L = R. QA reads the files back. Kicks and bass shots also get the groove loop check, on the kit's
GROOVE (the loop goes to qa/). --repro renders every item a second time, each in a fresh process (no cache or state
carried over), and compares the bytes.

Writes <out> (default build/<kit>, or build/<kit>-<set>) /*.flac, qa_report.json (with the render environment: the
masters' bytes depend on it, ADR 0007), qa_report.md, sheets/<kit>_<group>_<name>.png and qa/*_<groove>.flac.
Exit status: 0 when every check passes, 1 when any fails, 2 on a usage error (an unknown kit or item, a kit that isn't
ready) or when a vocal item lacks the models.
"""
import argparse
import hashlib
import io
import json
import multiprocessing
import os
import platform
import statistics
import sys
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from importlib import metadata
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402
import scipy  # noqa: E402
import soundfile as sf  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import kits  # noqa: E402
from tools.sound import qa  # noqa: E402
from tools.sound.models import ModelsMissing  # noqa: E402
from tools.sound.item import Out, UnknownName  # noqa: E402

SR = D.SR


@dataclass
class Rendered:
    item: object
    takes: list
    outs: list          # what each render returned (Out); .audio is replaced by the master
    pre: list           # each take before the shared gain
    audio: list         # the masters, float
    gain_db: float
    capped: bool        # the LUFS target would have gone over the ceiling


# ------------------------------------------------------------------ rendering
def as_out(r):
    return r if isinstance(r, Out) else Out(np.asarray(r, dtype=float))


def subsonic(x, hp_hz, whole_loop):
    """The renderer's DC and sub-sonic high-pass. A file that loops from sample 0 is filtered circularly, so the
    filter's start-up doesn't land on the seam."""
    if not hp_hz:
        return x
    if whole_loop:
        n = len(x)
        return D.dc_block(np.concatenate([x, x, x]), hp_hz)[n:2 * n]
    return D.dc_block(x, hp_hz)


def finish_take(item, out):
    x = np.asarray(out.audio, dtype=float)
    if x.ndim not in (1, 2) or (x.ndim == 2 and x.shape[1] != 2) or not len(x) or not np.all(np.isfinite(x)):
        raise ValueError(f'{item.name}: render must return finite audio shaped (n,) or (n, 2), got {x.shape}')
    if not item.loop:
        return D.finish(subsonic(x, item.hp_hz, False))
    if out.loop is None:
        out.loop = (0, len(x))
    x = subsonic(x, item.hp_hz, out.loop[0] == 0)
    return x if out.loop[0] == 0 else D.finish(x, oneshot=False)  # an attack before the loop gets the fade-in


def shared_gain(item, xs):
    """One gain for the instrument: (gain dB, capped)."""
    tp = max(D.true_peak_db(x) for x in xs)
    if item.level is None:
        return D.TP_TARGET - tp, False
    g = item.level - max(qa.lufs(x) for x in xs)
    return (D.TP_TARGET - tp, True) if tp + g > D.TP_TARGET else (g, False)


def render_item(item, kit):
    takes = item.takes(kit)
    outs = [as_out(item.render(t)) for t in takes]
    pre = [finish_take(item, o) for o in outs]
    gain_db, capped = shared_gain(item, pre)
    audio = [x * D.undb(gain_db) for x in pre]
    for o, x in zip(outs, audio):
        o.audio = x
    return Rendered(item, takes, outs, pre, audio, float(gain_db), capped)


def encode(x):
    """FLAC bytes, 48 kHz PCM_24, mono where L = R."""
    x = np.asarray(x, dtype=float)
    if x.ndim == 2 and np.array_equal(x[:, 0], x[:, 1]):
        x = x[:, 0]
    buf = io.BytesIO()
    sf.write(buf, x, SR, subtype='PCM_24', format='FLAC')
    return buf.getvalue()


def write_flac(path, x):
    data = encode(x)
    Path(path).write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def load_kit(kit):
    """The kit's module, ready to render (kits.load: UnknownName, or KitNotReady while its DNA has a TODO)."""
    return kits.load(kit)


def _render_named(job):
    kit, name = job
    return render_item(kits.items(load_kit(kit), only=(name,))[0], kit)


def _hashes_named(job):
    return [hashlib.sha256(encode(x)).hexdigest() for x in _render_named(job).audio]


def repro_hashes(jobs_list, jobs=1):
    """The masters' SHA-256 rendered again, each item in a fresh process (spawned, one item per process), so nothing a
    first render cached (Kokoro takes, WORLD frames, the context loop's files) or left behind can carry over."""
    ctx = multiprocessing.get_context('spawn')
    with ProcessPoolExecutor(max_workers=jobs, mp_context=ctx, max_tasks_per_child=1) as pool:
        return list(pool.map(_hashes_named, jobs_list))


# ------------------------------------------------------------------ the render environment
def simd():
    """numpy's SIMD level: its baseline and the dispatch targets in use (none when pinned, tools/sound/__init__.py)."""
    try:
        from numpy._core._multiarray_umath import __cpu_baseline__, __cpu_dispatch__, __cpu_features__
    except ImportError:
        return {'baseline': None, 'dispatch': None}
    return {'baseline': list(__cpu_baseline__), 'dispatch': [t for t in __cpu_dispatch__ if __cpu_features__.get(t)]}


def cpu_model():
    try:
        for line in Path('/proc/cpuinfo').read_text().splitlines():
            if line.startswith('model name'):
                return line.split(':', 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def environment():
    """What the masters' bytes depend on besides the recipes (ADR 0007): the pins, numpy's SIMD level, the OpenBLAS
    kernel, the CPU.
    Vocal masters also depend on onnxruntime's kernels, which pick their instructions per CPU and can't be pinned, and
    on pyworld."""
    env = {'python': platform.python_version(), 'platform': f'{sys.platform} {platform.machine()}', 'cpu': cpu_model(),
           'numpy': np.__version__, 'numpy_simd': simd(), 'scipy': scipy.__version__, 'soundfile': sf.__version__,
           'libsndfile': sf.__libsndfile_version__,
           'NPY_ENABLE_CPU_FEATURES': os.environ.get('NPY_ENABLE_CPU_FEATURES'),
           'OPENBLAS_CORETYPE': os.environ.get('OPENBLAS_CORETYPE')}
    for pkg in ('onnxruntime', 'pyworld'):
        try:
            env[pkg] = metadata.version(pkg)
        except metadata.PackageNotFoundError:
            pass
    return env


# ------------------------------------------------------------------ QA of one rendered item
def file_gain_db(y, pre):
    """The gain between a take before the shared gain and its file (least squares)."""
    p, y = D.st(pre).ravel(), D.st(y).ravel()
    return float(D.db(np.dot(y, p) / (np.dot(p, p) + 1e-30)))


def qa_item(done, out_dir, kit_mod, repro=None):
    """Write an item's files, run its QA, return its report entry. repro: the SHA-256 of a second render
    (repro_hashes), or None when not checked."""
    it = done.item
    files, rows, gains = [], [], []
    for take, o, pre in zip(done.takes, done.outs, done.pre):
        path = out_dir / take.file
        sha = write_flac(path, o.audio)
        y, _ = sf.read(path, dtype='float64')
        checks = qa.check_take(y, take.spec(o), qa.file_format(path))
        if it.category == 'vox' and it.script and 'dry' in it.tags:
            checks.append(qa.check_transcript(y, it.script))
        entry = {'file': take.file, 'sha256': sha, 'seconds': round(len(y) / SR, 3),
                 'channels': 1 if y.ndim == 1 else y.shape[1]}
        if it.loop:
            entry['loop'] = [int(o.loop[0]), int(o.loop[1])]
        if o.meta:
            entry['meta'] = o.meta
        files.append({**entry, 'checks': [c.as_dict() for c in checks]})
        rows.append({'layer': take.layer, 'rr': take.rr, 'note': take.note, 'x': y})
        gains.append(round(file_gain_db(y, pre), 2))
    checks = qa.check_layers(rows) + [qa.check_gains(gains)]
    loop_file = None
    if not it.loop and it.qa.get('loop_check', qa.CATEGORY_RULES[it.category]['loop_check']):
        first = done.takes[0].note
        layers = {t.vel: r['x'] for t, r in zip(done.takes, rows) if t.rr == 1 and t.note == first}
        loop_checks, loop = qa.check_loop(layers, kit_mod.BPM, qa.GROOVES[kit_mod.GROOVE], mud_mode(it),
                                          choke=bool(it.qa.get('loop_choke')))
        checks += loop_checks
        (out_dir / 'qa').mkdir(exist_ok=True)
        loop_file = f'qa/{kit_mod.KIT}_{it.group}_{it.name}_{kit_mod.GROOVE}.flac'
        write_flac(out_dir / loop_file, loop * min(1.0, D.undb(D.TP_TARGET - D.true_peak_db(loop))))
    checks += qa.check_content(names=[it.name, it.group] + [f['file'] for f in files],
                               strings=[it.about, *it.tags], script=it.script, flags=it.flags, tags=it.tags,
                               category=it.category)
    if repro is not None:
        same = list(repro) == [f['sha256'] for f in files]
        checks.append(qa.Check('reproducible', same, None, 'same SHA-256 rendered twice',
                               'the second render in a fresh process' if same else
                               'a second render, in a fresh process, gave different bytes'))
    else:
        checks.append(qa.na('reproducible', 'not checked (--repro)'))
    return {'name': it.name, 'group': it.group, 'category': it.category, 'about': it.about,
            'flags': sorted(it.flags), 'tags': list(it.tags), 'qa_overrides': dict(it.qa),
            'gain_db': round(done.gain_db, 2), 'capped': done.capped, 'files': files,
            'checks': [c.as_dict() for c in checks], 'loop_file': loop_file}


def mud_mode(it):
    return it.qa.get('mud') or qa.CATEGORY_RULES[it.category]['mud']


def failures(entry):
    out = [(f['file'], c) for f in entry['files'] for c in f['checks'] if c['status'] == 'fail']
    return out + [('instrument', c) for c in entry['checks'] if c['status'] == 'fail']


def _sheet(done, entry, path):
    from tools.sound import sheet
    takes = done.takes
    notes = sorted({t.note for t in takes}, key=lambda n: -1 if n is None else n)
    mid = notes[len(notes) // 2]
    k = max(i for i, t in enumerate(takes) if t.note == mid and t.rr == 1)  # top layer, rr1, middle zone
    fails = failures(entry)
    total = sum(len(f['checks']) for f in entry['files']) + len(entry['checks'])
    summary = (f"{len(takes)} files · one gain {entry['gain_db']:+.2f} dB" + (' (capped at the ceiling)' if
               entry['capped'] else '') + f" · {total - len(fails)} of {total} checks pass")
    lines = [entry['about'], summary]
    if fails:
        lines.append('failed: ' + ', '.join(sorted({c['name'] for _, c in fails})))
    sheet.draw(path, done.audio[k], takes[k].file, [ln for ln in lines if ln],
               gated=mud_mode(done.item) in ('gate', 'bass-body'))


# ------------------------------------------------------------------ reports
def _value(c):
    v = c['value']
    return '' if v is None else (f'{v:g}' if isinstance(v, (int, float)) else str(v))


def write_reports(out_dir, report):
    (out_dir / 'qa_report.json').write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    s = report['summary']
    md = [f"# QA report: {report['kit']} \"{report['title']}\"", '',
          f"Set: {report['set'] or 'all'} · {s['items']} items · {s['files']} files · "
          f"{s['failed_checks']} failed checks" + (f" in {', '.join(s['failed_items'])}" if s['failed_items'] else ''),
          '', 'Limits: plan section 5.2 (`tools/sound/qa.py`). Values are in the units of each limit.', '',
          f"Rendered on {_env_line(report['environment'])}.", '',
          '| Item | Category | Files | Gain | Result |', '|---|---|---|---|---|']
    for e in report['items']:
        f = failures(e)
        res = 'pass' if not f else f"{len(f)} failed: " + ', '.join(sorted({c['name'] for _, c in f}))
        md.append(f"| {e['group']}/{e['name']} | {e['category']} | {len(e['files'])} | {e['gain_db']:+.2f} dB"
                  f"{' (capped)' if e['capped'] else ''} | {res} |")
    gated = [e for e in report['items'] if any(c['name'].startswith('mud_') for f in e['files'] for c in f['checks'])]
    if gated:
        md += ['', '## Mud gate', '', "Worst file of each instrument; loop = the 2-bar groove loop on the kit's GROOVE "
               '(for a loop file: its own gaps between kicks).', '',
               '| Item | 150-400 Hz fall (ms) | vs 30-120 Hz after 80 ms (dB) | share (dB) | 400 Hz-1 kHz fall (ms) '
               '| loop vs sub (dB) | loop before hit (dB) |', '|---|---|---|---|---|---|---|']
        for e in gated:
            pool = [c for f in e['files'] for c in f['checks']] + e['checks']   # a loop file carries its own gaps
            md.append(f"| {e['group']}/{e['name']} | " + ' | '.join(
                _worst(n, pool) for n in ('mud_150_400_fall', 'mud_vs_sub_after_80ms', 'mud_share', 'box_400_1k_fall',
                                         'loop_mud_vs_sub', 'loop_mud_before_hit')) + ' |')
    md += _rule_tables(report['items'])
    md += _club_table(report['items'])
    special = [e for e in report['items'] if e['flags'] or e['qa_overrides']]
    if special:
        md += ['', '## Exceptions', '', 'Every flag and every override of a category rule, with its reason.', '',
               '| Item | Flags | QA overrides | Why |', '|---|---|---|---|']
        for e in special:
            over = ', '.join(f'{k}={v}' for k, v in e['qa_overrides'].items() if k != 'why')
            md.append(f"| {e['group']}/{e['name']} | {', '.join(e['flags'])} | {over} | "
                      f"{e['qa_overrides'].get('why', '')} |")
    fails = [(e, f) for e in report['items'] for f in [failures(e)] if f]
    if fails:
        md += ['', '## Failures']
        for e, f in fails:
            md += ['', f"### {e['group']}/{e['name']}", '']
            md += [f"- {where}: **{c['name']}** = {_value(c)} (limit {c['limit']}) {c['detail']}".rstrip()
                   for where, c in f]
    (out_dir / 'qa_report.md').write_text('\n'.join(md) + '\n', encoding='utf-8')


def _worst(name, pool):
    """The worst value of a check over a pool of checks, marked FAIL if any failed; n/a where it didn't apply."""
    vals = [c['value'] for c in pool if c['name'] == name and c['value'] is not None]
    bad = [c for c in pool if c['name'] == name and c['status'] == 'fail']
    return ('n/a' if not vals and not bad else ('inf' if not vals else f'{max(vals):g}')) + (' FAIL' if bad else '')


def _rule_tables(entries):
    """The kick crest (each kick's lowest top-layer reading) and the bass-body readings (worst file), ADR 0011."""
    md = []
    pools = {e['name']: [c for f in e['files'] for c in f['checks']] + e['checks'] for e in entries}
    kicks = [e for e in entries if any(c['name'] == 'kick_crest' and c['status'] != 'n/a' for c in pools[e['name']])]
    if kicks:
        md += ['', '## Kick crest', '', f'True peak over the loudest 50 ms, the top layer: at least '
               f'{qa.KICK_CREST_MIN_DB:g} dB (a sine reads 3.01). Lowest file of each kick.', '',
               '| Item | Crest (dB) |', '|---|---|']
        for e in kicks:
            vals = [c['value'] for c in pools[e['name']] if c['name'] == 'kick_crest' and c['value'] is not None]
            bad = any(c['name'] == 'kick_crest' and c['status'] == 'fail' for c in pools[e['name']])
            md.append(f"| {e['group']}/{e['name']} | {min(vals):g}{' FAIL' if bad else ''} |")
    body = [e for e in entries if any(c['name'].startswith('body_') for c in pools[e['name']])]
    if body:
        names = ('body_vs_sub', 'body_over_saw', 'body_share', 'loop_body_vs_sub', 'loop_mud_before_hit', 'body_duck')
        md += ['', '## Bass body', '', 'The proposed bass-body rule (ADR 0011). Worst file of each instrument; loop = '
               'the groove loop, or a phrase\'s own gaps between its kicks.', '',
               '| Item | vs 30-120 Hz (dB) | over the saw (dB) | share (dB) | loop vs sub (dB) | loop before hit (dB) '
               '| under the kick (dB) |', '|---|---|---|---|---|---|---|']
        for e in body:
            md.append(f"| {e['group']}/{e['name']} | " + ' | '.join(_worst(n, pools[e['name']]) for n in names) + ' |')
    return md


def _club_table(entries):
    """What each club loop's master did (its report, in the file's metadata): loudness, the cap and the reduction it
    used, and the crest of the loop's hits, in its input and in the club loop (ADR 0014)."""
    rows = []
    for e in entries:
        for f in e['files'] if 'club' in e['tags'] else ():
            m = (f.get('meta') or {}).get('master')
            if not m or m.get('silent'):
                continue
            p = m.get('punch') or {}
            k, c = p.get('kick'), p.get('clap')
            cap = f"{m['settings']['max_reduction_db']:g}" + (' (declared)' if 'club_max_reduction_db' in
                                                              e['qa_overrides'] else '')
            kick = (f"{statistics.median(k['in_db']):.2f} -> {statistics.median(k['out_db']):.2f} "
                    f"({-k['median_loss_db']:+.2f}; lowest {k['min_out_db']:.2f})" if k else 'not given')
            clap = (f"{-c['median_loss_db']:+.2f} (in {c['band_hz'][0] / 1000:g}-{c['band_hz'][1] / 1000:g} kHz "
                    f"{-c['band_median_loss_db']:+.2f})" if c else 'not given')
            pk = m.get('peaks') or {}
            away = pk.get('away_from_hits') or []
            over = 'not given'
            if pk.get('kick'):
                over = f"kicks {pk['kick']['max_db']:+.1f}"
                if away:
                    over += f", elsewhere {away[0]['db']:+.1f} at {away[0]['s']:.2f} s"
            out, reduction = m['output'], f"{m['peak_reduction_db']:.2f}"
            parts = f"{m['clip']['reduction_db']:.2f} / {m['limit']['gr_max_db']:.2f}"
            rows.append(f"| {e['group']}/{e['name']} | {out['lufs_i']:.2f} | {out['plr']:.2f} | {cap} | "
                        f"{reduction} ({parts}) | {over} | {kick} | {clap} | {m['latency']['samples']} |")
    if not rows:
        return []
    about = ("What each club loop's master did (ADR 0014, its report in the file's metadata). The cap: "
             f"{qa.CLUB_MAX_REDUCTION_DB:g} dB unless the item declares its own. Over the knee: how far the driven "
             "loop goes over the clipper's knee in the kicks' first 30 ms, and at its loudest moment away from every "
             "hit. "
             "Crest: ADR 0011's, per hit as the loop plays it: the median over its full-velocity kicks in the master's "
             'input and the club loop, and the median change of each kick and clap, in to out.')
    head = ('| Item | LUFS-I | PLR (dB) | Cap (dB) | Peak reduction: clip / limit (dB) '
            '| Deepest over the clip knee (dB) | Kick crest (dB) | Clap crest change (dB) | Latency (samples) |')
    return ['', '## Club master', '', about, '', head, '|---|---|---|---|---|---|---|---|---|'] + rows


def _env_line(env):
    sim = env['numpy_simd']
    return (f"{env['cpu']} ({env['platform']}), Python {env['python']}, numpy {env['numpy']} (SIMD baseline "
            f"{', '.join(sim['baseline'] or ['?'])}, dispatch {', '.join(sim['dispatch'] or []) or 'none'}; "
            f"OpenBLAS kernel {env.get('OPENBLAS_CORETYPE') or 'auto'}), "
            f"scipy {env['scipy']}, libsndfile {env['libsndfile']}" +
            ''.join(f', {k} {env[k]}' for k in ('onnxruntime', 'pyworld') if k in env))


# ------------------------------------------------------------------ CLI
def run(kit, sets, only, out_dir, sheets=True, repro=False, jobs=1):
    kit_mod = load_kit(kit)
    items = kits.items(kit_mod, sets=sets, only=only)
    out_dir.mkdir(parents=True, exist_ok=True)
    if sheets:
        (out_dir / 'sheets').mkdir(exist_ok=True)
    jobs_list = [(kit, it.name) for it in items]
    again = repro_hashes(jobs_list, jobs) if repro else [None] * len(jobs_list)
    if jobs > 1:
        with ProcessPoolExecutor(max_workers=jobs) as pool:
            rendered = pool.map(_render_named, jobs_list)
            entries = [_finish(done, out_dir, kit_mod, sheets, h) for done, h in zip(rendered, again)]
    else:
        entries = [_finish(_render_named(j), out_dir, kit_mod, sheets, h) for j, h in zip(jobs_list, again)]
    failed = [e for e in entries if failures(e)]
    report = {'kit': kit_mod.KIT, 'title': kit_mod.TITLE, 'set': ','.join(sets) if sets else None,
              'environment': environment(), 'items': entries,
              'summary': {'items': len(entries), 'files': sum(len(e['files']) for e in entries),
                          'failed_checks': sum(len(failures(e)) for e in entries),
                          'failed_items': [f"{e['group']}/{e['name']}" for e in failed]}}
    write_reports(out_dir, report)
    return report


def _finish(done, out_dir, kit_mod, sheets, repro):
    entry = qa_item(done, out_dir, kit_mod, repro)
    if sheets:
        _sheet(done, entry, out_dir / 'sheets' / f'{kit_mod.KIT}_{done.item.group}_{done.item.name}.png')
    print(f"{entry['group']}/{entry['name']}: {len(entry['files'])} files, gain {entry['gain_db']:+.2f} dB, "
          f"{len(failures(entry))} failed checks", flush=True)
    return entry


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--kit', required=True, help=f'one of {", ".join(kits.names())}')
    ap.add_argument('--set', action='append', dest='sets', help='render only items in this set (repeatable)')
    ap.add_argument('--only', nargs='+', help='render only these item names')
    ap.add_argument('--out', type=Path, help='output folder (default build/<kit>, or build/<kit>-<set>)')
    ap.add_argument('--no-sheets', action='store_true', help='skip the PNG sheets')
    ap.add_argument('--repro', action='store_true',
                    help='render every item a second time, each in a fresh process, and compare the bytes')
    ap.add_argument('--jobs', type=int, default=1, help='render items in this many processes')
    a = ap.parse_args(argv)
    if a.out is None:
        a.out = Path('build') / '-'.join([a.kit, *(a.sets or [])])
    try:
        report = run(a.kit, a.sets, a.only, a.out, sheets=not a.no_sheets, repro=a.repro, jobs=a.jobs)
    except UnknownName as e:
        print(f'error: {e.args[0]}', file=sys.stderr)
        return 2
    except kits.KitNotReady as e:
        print(f'error: {e}', file=sys.stderr)
        return 2
    except ModelsMissing as e:
        print(f'error: {e} (the vocal items need the models; render the others with --only)', file=sys.stderr)
        return 2
    s = report['summary']
    print(f"{s['items']} items, {s['files']} files, {s['failed_checks']} failed checks -> {a.out / 'qa_report.md'}")
    return 1 if s['failed_checks'] else 0


if __name__ == '__main__':
    sys.exit(main())
