"""Wave-5 multisamples for the instrument library: a rave Hoover (looped, stereo), a bassline Donk (one-shots) and the
Vocal Mosaic (each sung hook cut into its syllables, one WAV per chop, plus the dry hook for the tempo-synced key).
Writes WAVs into lib/Samples and a sidecar inst/w5_meta.json (roots, loop points, chop words and pitches)."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys
import numpy as np
import soundfile as sf

sys.path.insert(0, PACK_ROOT + '/inst')
sys.path.insert(0, PACK_ROOT + '/audio')
from render_inst import saw_per, per_phase, polyblep, circ_lp, loop_len, write_wav, rms, seam_db, m2f, nname  # noqa: E402
from dsp import SR, bfilt, noise, n_samples  # noqa: E402

OUT = PACK_ROOT + '/inst/lib/Samples'
META = {}
rng = np.random.default_rng(20261026 + 5)


def save(key, rel, x, root, loop=None, **extra):
    write_wav(os.path.join(OUT, rel), x, root, loop)
    META[key] = dict(path='Samples/' + rel, root=root, frames=x.shape[0], ch=1 if x.ndim == 1 else 2, loop=loop, peak=round(float(np.abs(x).max()), 3), **extra)
    if loop:
        META[key]['seam'] = round(float(np.max([seam_db(x[:, c] if x.ndim == 2 else x, loop[0], loop[1]) for c in range(x.shape[1] if x.ndim == 2 else 1)])), 2)


# ---------------------------------------------------------------- hoover (rave, Alpha Juno style): detuned saws, a width-modulated pulse, a sub-octave saw
def pulse_per(M, L, n, V, pw0=0.5, depth=0.32, phase0=0.0):
    """A pulse built from two saws, the second offset by a width that swings V whole times per loop (so the loop stays exact)."""
    ph, inc = per_phase(M, L, n, phase0)
    k = np.arange(n)
    pw = pw0 + depth * np.sin(2 * np.pi * V * k / L)
    p1, p2 = ph % 1.0, (ph + pw) % 1.0
    s1 = 2 * p1 - 1 - polyblep(p1, inc)
    s2 = 2 * p2 - 1 - polyblep(p2, inc)
    return s1 - s2


def hoover(m, T=2.0):
    f = m2f(m)
    N, L = loop_len(f, T, even=True)
    V = 1                                                       # one width sweep per loop (0.5 Hz)
    chans = []
    for side in (0, 1):
        x = np.zeros(L)
        cents = [(0, 1.0), (-14, 0.75), (14, 0.75), (-28, 0.5), (28, 0.5)]
        used = set()
        for c, a in cents:
            d = int(round(f * T * (2 ** (c / 1200) - 1)))
            if c and d == 0:
                d = 1 if c > 0 else -1
            while d in used and d != 0:                         # distinct cycle counts, so voices beat instead of doubling
                d += 1 if d > 0 else -1
            used.add(d)
            d += (1 if side and c else 0) * (1 if c > 0 else -1) if abs(c) > 20 else 0
            x += a * saw_per(N + d, L, L, rng.uniform())
        x += 0.9 * pulse_per(N, L, L, V, phase0=rng.uniform() if side else 0.0)
        x += 0.65 * saw_per(N // 2, L, L, rng.uniform())        # the octave below
        chans.append(x)
    x = np.stack(chans, 1)
    x -= x.mean(0)
    x = circ_lp(x, 5200 if m < 60 else 6400, 2)
    x = x / rms(x) * 0.22
    x = np.tanh(1.4 * x) / np.tanh(1.4)
    x -= x.mean(0)
    return x, L


# ---------------------------------------------------------------- donk: a sine that drops into pitch, a filtered square bite and a woody knock
def donk(m, dur=0.42):
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    fp = f * (1 + 1.0 * np.exp(-t / 0.011))
    ph = np.cumsum(fp) / SR
    x = np.sin(2 * np.pi * ph) * np.exp(-t / 0.11)
    sq = np.sign(np.sin(2 * np.pi * f * t + 0.3))
    x += 0.2 * bfilt(sq, 'lowpass', min(1600, 9 * f)) * np.exp(-t / 0.05)
    x += 0.12 * bfilt(noise(n), 'bandpass', (min(3 * f, 3000), min(7 * f, 9000))) * np.exp(-t / 0.006)
    x = np.tanh(1.6 * x) / np.tanh(1.6)
    fi, fo = int(0.0008 * SR), int(0.03 * SR)
    x[:fi] *= np.linspace(0, 1, fi)
    x[-fo:] *= np.linspace(1, 0, fo)
    return x


# ---------------------------------------------------------------- vocal mosaic: every sung note of a hook as its own chop
NOTE_NAMES = ['C', 'Cs', 'D', 'Eb', 'E', 'F', 'Fs', 'G', 'Ab', 'A', 'Bb', 'B']
PC = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def nnum(s):
    pc = PC[s[0]] + (1 if '#' in s else -1 if 'b' in s[1:2] else 0)
    return pc + 12 * (int(s[-1]) + 1)


def warp(p, s):
    base = np.floor(p / 2) * 2
    f = p - base
    return base + (f * 2 * s if f <= 1 else 2 * s + (f - 1) * (2 - 2 * s))


def zc(x, i, win):
    lo, hi = max(1, i - win), min(len(x) - 2, i + win)
    seg = np.abs(x[lo:hi])
    return lo + int(np.argmin(seg))


MOSAIC = ['h19', 'h20', 'h21', 'h22', 'h02', 'h03', 'h06', 'h11', 'h12', 'h14', 'h16', 'bh3', 'bh5', 'h24', 'h29', 'h30']


def mosaic():
    V = json.load(open(PACK_ROOT + '/inst/vocals_meta.json'))
    out = {}
    for hid in MOSAIC:
        v = V[hid]
        x, sr = sf.read(f'{PACK_ROOT}/out/vox_src/vox_dry/{hid}.wav')
        assert sr == SR and x.ndim == 1
        s16 = 60 / v['bpm'] / 4
        mel = v['mel']
        ons = [warp(m[0], v['swing']) * s16 for m in mel]
        hop = int(0.0025 * SR)
        env = np.sqrt(np.convolve(x ** 2, np.ones(2 * hop) / (2 * hop), 'same'))

        def start_of(t):   # a consonant before the beat belongs to the chop: back up to the quietest point just before it, if there is a gap
            i0, i1 = max(0, int((t - 0.075) * SR)), int((t + 0.004) * SR)
            post = float(np.sqrt(np.mean(x[int(t * SR):int((t + 0.04) * SR)] ** 2)) + 1e-9)
            j = i0 + int(np.argmin(env[i0:i1])) if i1 > i0 else 0
            return j / SR if env[j] < 0.35 * post else max(0.0, t - 0.014)
        starts = [start_of(t) if t > 0.02 else 0.0 for t in ons]
        word = None
        chops = []
        for i, m in enumerate(mel):
            step, ln, note, w = m
            if w != '~':
                word, part = w, 0
            else:
                part += 1
            t0 = starts[i]
            t_end = warp(step + ln, v['swing']) * s16 + 0.22          # the note plus a short natural tail
            if i + 1 < len(mel):
                t_end = min(t_end, starts[i + 1] - 0.002)
            else:      # the loop's end may carry the first word's lead-in (consonant, breath) wrapped round: leave it out
                last_end = warp(mel[-1][0] + mel[-1][1], v['swing']) * s16
                gap = len(x) / SR - last_end + ons[0]
                wrapped = max(0.0, (0.38 if gap > 0.45 else 0.17) - ons[0])
                t_end = min(t_end, len(x) / SR - wrapped)
            t_end = min(t_end, len(x) / SR)
            a, b = zc(x, int(t0 * SR), 40), zc(x, int(t_end * SR), 60)
            y = x[a:b].copy()
            fi, fo = int(0.003 * SR), min(int(0.018 * SR), len(y) // 3)
            y[:fi] *= np.linspace(0, 1, fi)
            y[-fo:] *= np.linspace(1, 0, fo)
            sung = nnum(note) + v['oct']
            label = word.strip('.,!?').replace("'", '') + (f'-{part + 1}' if part or (i + 1 < len(mel) and mel[i + 1][3] == '~') else '')
            chops.append(dict(word=label, sung=sung, y=y, sec=round(len(y) / SR, 3)))
        peak = max(np.abs(c['y']).max() for c in chops)
        g = 0.89 / peak
        meta = []
        for k, c in enumerate(chops):
            nm = f"{hid.upper()}_{k + 1:02d}_{c['word']}_{NOTE_NAMES[c['sung'] % 12]}{c['sung'] // 12 - 1}.wav"
            rel = f'Vocals/Mosaic/{hid.upper()}/{nm}'
            save(f'mosaic_{hid}_{k + 1:02d}', rel, c['y'] * g, c['sung'], word=c['word'])
            meta.append(dict(key=f'mosaic_{hid}_{k + 1:02d}', word=c['word'], sung=c['sung'], sec=c['sec']))
        whole = x / np.abs(x).max() * 0.89
        save(f'mosaic_{hid}_loop', f'Vocals/Mosaic/{hid.upper()}/{hid.upper()}_WholeHook_{v["bpm"]}bpm.wav', whole, 72, None, bpm=v['bpm'], bars=4)
        out[hid] = dict(title=v['title'], bpm=v['bpm'], chops=meta)
        print(hid, len(chops), 'chops:', ' '.join(f"{c['word']}({c['sec']}s)" for c in chops))
    return out


def main():
    hv = {}
    for m in range(24, 73, 3):
        x, L = hoover(m)
        hv[m] = (x, L)
    g = 0.89 / max(np.abs(x).max() for x, _ in hv.values())
    for m, (x, L) in hv.items():
        save(f'hoover_{m}', f'Bass/Hoover/Hoover_{nname(m)}.wav', x * g, m, (0, L - 1))
    dk = {m: donk(m) for m in range(24, 61, 3)}
    g = 0.89 / max(np.abs(x).max() for x in dk.values())
    for m, x in dk.items():
        save(f'donk_{m}', f'Bass/Donk/Donk_{nname(m)}.wav', x * g, m)
    mo = mosaic()
    META['_mosaic'] = mo
    json.dump(META, open(PACK_ROOT + '/inst/w5_meta.json', 'w'), indent=1)
    seams = sorted(((v['seam'], k) for k, v in META.items() if isinstance(v, dict) and v.get('seam') is not None), reverse=True)[:4]
    print(len(META) - 1, 'samples; worst seams', seams)


if __name__ == '__main__':
    main()
