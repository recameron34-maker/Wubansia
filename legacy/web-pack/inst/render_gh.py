"""Garage-house sound set in the style of Disclosure's early records (original synthesis, no samples from their music):
a tight deep-house kit with plate claps and swung hats, a plucky detuned-saw chord synth, a round glide bass,
a detuned square stab, a warm pad, and four 128 BPM grooves built from the new kit."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys
import numpy as np
import pedalboard as pb

sys.path.insert(0, PACK_ROOT + '/audio')
sys.path.insert(0, PACK_ROOT + '/inst')
from dsp import SR, bfilt, noise, exp_env, n_samples, sat, svf, saw, square, sine  # noqa: E402
import hits as H  # noqa: E402
from render_inst import (per_phase, polyblep, saw_per, circ_lp, loop_len, write_wav, rms, seam_db, nname, m2f)  # noqa: E402

LIB = PACK_ROOT + '/inst/lib'
META = {}
rng = np.random.default_rng(2013)


def save(key, rel, x, root=None, loop=None, **extra):
    write_wav(os.path.join(LIB, rel), x, root, loop)
    META[key] = dict(path=rel, root=root, frames=x.shape[0], ch=1 if x.ndim == 1 else 2, loop=loop, peak=float(np.abs(x).max()), **extra)
    if loop:
        META[key]['seam'] = round(seam_db(x, loop[0], loop[1]), 2)


def plate(x, size=0.32, wet=0.22, damp=0.55, width=0.6):
    y = pb.Pedalboard([pb.Reverb(room_size=size, damping=damp, wet_level=wet, dry_level=1.0, width=width)])(np.stack([x, x]).astype(np.float32), SR)
    return y.T.astype(float)


def norm(x, peak=0.89):
    return x / np.abs(x).max() * peak


# ---------------------------------------------------------------- kit
def kit():
    hits = {}
    k = H.kick(43.65, f_start=165, tau_p=0.028, tau_a=0.13, dur=0.38, click=0.32, drive=1.9, harm=0.08)       # round, short, tuned to F
    hits['kick_gh'] = ('Kick-GarageHouse-Round_F', norm(k))
    c = H.clap(spread=(0, 0.009, 0.019, 0.029), tail_tau=0.055, band=(1000, 2900), room=0.0, dur=0.5)
    c = plate(c, 0.3, 0.26, 0.6, 0.7)
    hits['clap_gh'] = ('Clap-GarageHouse-Plate', norm(c, 0.85))
    s = H.snare(t1=192.0, t2=340.0, tone_tau=0.045, noise_tau=0.075, hp=2000, lp=12000, tone_amt=0.5, noise_amt=0.8, dur=0.3, drive=1.4)
    hits['snare_gh'] = ('Snare-GarageHouse-Crisp', norm(s, 0.85))
    r = H.rim(f1=1760.0, f2=505.0, tau=0.007, dur=0.11, drive=2.2)
    hits['rim_gh'] = ('Rim-GarageHouse', norm(r, 0.8))
    ch = H.hat(0.026, 0.12, scale=1.0, metal_amt=0.6, noise_amt=0.6, band=(7000, 13000), hp=7500)
    hits['chat_gh'] = ('ClosedHat-GarageHouse', norm(ch, 0.75))
    oh = H.hat(0.21, 0.62, scale=1.0, metal_amt=0.6, noise_amt=0.55, band=(6800, 12800), hp=7200)
    hits['ohat_gh'] = ('OpenHat-GarageHouse', norm(oh, 0.75))
    sh = H.shaker(dur=0.14, attack=0.009, tau=0.026)
    hits['shaker_gh'] = ('Shaker-GarageHouse', norm(sh, 0.7))
    n = n_samples(0.35)
    t = np.arange(n) / SR
    snap = bfilt(noise(n), 'bandpass', (1100, 3600), 2) * exp_env(n, 0.011, 0.0004) + 0.5 * np.sin(2 * np.pi * 1580 * t) * exp_env(n, 0.006, 0.0003)
    snap = plate(sat(snap * 1.4, 1.3), 0.22, 0.18, 0.5, 0.5)
    hits['snap_gh'] = ('Snap-GarageHouse', norm(snap, 0.8))
    tb = H.tamb(0.3)
    hits['tamb_gh'] = ('Tambourine-GarageHouse', norm(tb, 0.7))
    for key, (name, x) in hits.items():
        save(key, f'Samples/Drums/GarageHouse/{name}.wav', x, None, None, title=name)
    return {k: v[1] for k, v in hits.items()}


# ---------------------------------------------------------------- grooves at 128 BPM, 2 bars, swung 16ths, tails folded onto the start
BPM = 128
STEP = 60 / BPM / 4


def groove(hits, pat, swing=0.58, bars=2, human=0.0025):
    L = int(round(bars * 16 * STEP * SR))
    out = np.zeros((L + SR, 2))
    gains = dict(kick_gh=1.0, clap_gh=0.62, snare_gh=0.5, rim_gh=0.34, chat_gh=0.3, ohat_gh=0.28, shaker_gh=0.26, snap_gh=0.36, tamb_gh=0.22)
    pans = dict(kick_gh=0, clap_gh=0, snare_gh=0, rim_gh=-0.25, chat_gh=0.2, ohat_gh=0.28, shaker_gh=-0.3, snap_gh=0.12, tamb_gh=0.38)
    for lane, rows in pat.items():
        x = hits[lane]
        xs = (x[:, 0], x[:, 1]) if x.ndim == 2 else (x, x)
        for b in range(bars):
            row = rows[b % len(rows)]
            for s, ch in enumerate(row):
                if ch == '.':
                    continue
                v = int(ch) / 9
                pos = (b * 16 + s) * STEP + (STEP * (swing - 0.5) * 2 if s % 2 else 0) + rng.normal(0, human) * (lane not in ('kick_gh',))
                i = max(0, int(round(pos * SR)))
                g = gains[lane] * v ** 1.4
                p = pans[lane]
                for c in (0, 1):
                    seg = xs[c][:len(out) - i] * g
                    out[i:i + len(seg), c] += seg * np.sqrt(0.5 - 0.5 * p if c == 0 else 0.5 + 0.5 * p) * 1.41
    # fold the tail beyond the loop back onto the start so it repeats seamlessly
    y = out[:L].copy()
    y[:len(out) - L] += out[L:]
    y = pb.Pedalboard([pb.Compressor(threshold_db=-14, ratio=2.5, attack_ms=8, release_ms=90), pb.Gain(2)])(y.T.astype(np.float32), SR).T.astype(float)
    return norm(y, 0.89)


GROOVES = {
    'gh_loop1': ('TwoStepShuffle', 'Two-step shuffle', {
        'kick_gh': ['9.........8.....', '9.........8...6.'],
        'clap_gh': ['....9.......9...'],
        'chat_gh': ['5.7.5.7.5.7.5.7.', '5.7.5.7.5.7.6.8.'],
        'shaker_gh': ['.5.6.5.6.5.6.5.6'],
        'rim_gh': ['......6.....5...', '...5..6.......5.'],
        'ohat_gh': ['..............6.', '......5.......6.']}),
    'gh_loop2': ('FourFloorSwing', 'Four-floor swing', {
        'kick_gh': ['9...9...9...9...'],
        'clap_gh': ['....9.......9...'],
        'ohat_gh': ['..7...7...7...7.'],
        'chat_gh': ['6.5.6.5.6.5.6.56'],
        'tamb_gh': ['.....5.......5..'],
        'shaker_gh': ['.4.5.4.5.4.5.4.5']}),
    'gh_loop3': ('RimShakerGroove', 'Rim & shaker groove', {
        'kick_gh': ['9......7..9.....', '9......7..9...7.'],
        'snap_gh': ['....9.......9...'],
        'rim_gh': ['..6..5...6..5.6.', '..6..5...6...56.'],
        'shaker_gh': ['5656565656565656'],
        'chat_gh': ['..5...5...5...5.']}),
    'gh_loop4': ('SkippyGarage', 'Skippy garage', {
        'kick_gh': ['9.......7.8.....', '9.....6...8.....'],
        'snare_gh': ['....9.......9..5', '....9.......9...'],
        'clap_gh': ['....8.......8...'],
        'chat_gh': ['7.57.57.57.57.5.', '7.57.57.57.5776.'],
        'ohat_gh': ['......5.......5.'],
        'rim_gh': ['.5.......5....5.']}),
}


# ---------------------------------------------------------------- melodic multisamples
def pluck(m, dur=1.35):
    """Detuned-saw chord pluck: two saws +/-9 cents and a soft square through a fast filter envelope."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m)
    x = (saw(np.full(n, f * 2 ** (-9 / 1200)), phase0=rng.uniform()) + saw(np.full(n, f * 2 ** (9 / 1200)), phase0=rng.uniform())
         + 0.55 * square(np.full(n, f), phase0=rng.uniform(), pw=0.5) + 0.35 * sine(np.full(n, f / 2)))
    cut = 260 + 5600 * np.exp(-t / 0.11) * min(1.0, 1.4 - (m - 36) / 80) + 600 * np.exp(-t / 0.6)
    x = svf(x / 3, cut, 1.15, 'lp')
    env = np.exp(-t / 0.42)
    a = n_samples(0.0025)
    env[:a] *= np.linspace(0, 1, a)
    x = sat(x * env * 1.6, 1.3)
    fl = n_samples(0.08)
    x[-fl:] *= np.linspace(1, 0, fl)
    return x


def glide_bass(m, T=1.0):
    """Round deep-house bass: sine fundamental with a filtered square on top. Whole file is one exact loop."""
    f = m2f(m)
    N, L = loop_len(f, T)
    k = np.arange(L)
    s = np.sin(2 * np.pi * N * k / L + 0.3)
    ph, inc = per_phase(N, L, L, 0.1)
    p = ph % 1.0
    sq = np.where(p < 0.5, 1.0, -1.0) + polyblep(p, inc) - polyblep((p + 0.5) % 1.0, inc)
    sq = circ_lp(sq, 700, 2)
    x = s + 0.42 * sq + 0.12 * np.sin(2 * np.pi * 2 * N * k / L)
    x = np.tanh(1.3 * x) / np.tanh(1.3)
    return x - x.mean(), L


def square_stab(m, T=1.5):
    """Two squares one beat apart (one beat per loop) plus a quiet sub square: the hollow stab-lead of 2010s garage house."""
    f = m2f(m)
    N, L = loop_len(f, T)
    def sqr(M, ph0):
        ph, inc = per_phase(M, L, L, ph0)
        p = ph % 1.0
        return np.where(p < 0.5, 1.0, -1.0) + polyblep(p, inc) - polyblep((p + 0.5) % 1.0, inc)
    x = sqr(N, 0.13) + 0.55 * sqr(N + 1, 0.61)          # one beat per loop, never cancels fully
    if N % 2 == 0:
        x += 0.3 * sqr(N // 2, 0.4)
    x = circ_lp(x, 3200 if m < 60 else 4200, 2)
    x = np.tanh(1.2 * x / 2) / np.tanh(1.2)
    return x - x.mean(), L


def deep_pad(m, T=2.0):
    """Warm saw pad: three detuned saws a side, slow vibrato, a sine an octave down, darker than the strings."""
    f = m2f(m)
    N, L = loop_len(f, T, even=True)
    V = max(1, round(4.2 * L / SR))
    out = []
    for offs in ((-2, 0, 1), (-1, 0, 2)):
        x = np.zeros(L)
        for d in offs:
            x += saw_per(N + d, L, L, rng.uniform(), vib=(0.0022, V))
        k = np.arange(L)
        x += 0.8 * np.sin(2 * np.pi * (N // 2) * k / L + rng.uniform(0, 6.28))
        out.append(x)
    x = np.stack(out, 1)
    x -= x.mean(0)
    x = circ_lp(x, 2100 if m < 60 else 2600, 2)
    x = x - 0.5 * circ_lp(x, 110, 2)
    return x, L


def main():
    hits = kit()
    for key, (fname, title, pat) in GROOVES.items():
        y = groove(hits, pat)
        save(key, f'Samples/Loops/Garage House/GH_DrumLoop_{fname}_128bpm.wav', y, None, None, title=title, bpm=128, bars=2)
    pl = {m: pluck(m) for m in range(36, 85, 3)}
    g = 0.89 / max(np.abs(x).max() for x in pl.values())
    for m, x in pl.items():
        save(f'pluck_{m}', f'Samples/Keys/GaragePluck/GaragePluck_{nname(m)}.wav', x * g, m)
    for m in range(24, 61, 3):
        x, L = glide_bass(m)
        save(f'gbass_{m}', f'Samples/Bass/GlideBass/GlideBass_{nname(m)}.wav', norm(x, 0.8), m, (0, L - 1))
    for m in range(36, 73, 3):
        x, L = square_stab(m)
        save(f'sqstab_{m}', f'Samples/Keys/SquareStab/SquareStab_{nname(m)}.wav', norm(x, 0.8), m, (0, L - 1))
    pads = {m: deep_pad(m) for m in range(36, 85, 3)}
    for m, (x, L) in pads.items():
        y = x / rms(x) * 0.16
        y = y / max(1.0, np.abs(y).max() / 0.89)
        save(f'dpad_{m}', f'Samples/Keys/DeepPad/DeepPad_{nname(m)}.wav', y, m, (0, L - 1))
    json.dump(META, open(PACK_ROOT + '/inst/gh_meta.json', 'w'), indent=1)
    seams = sorted(((v['seam'], k) for k, v in META.items() if v.get('seam') is not None), reverse=True)[:4]
    print(len(META), 'samples; worst seams', seams)


if __name__ == '__main__':
    main()
