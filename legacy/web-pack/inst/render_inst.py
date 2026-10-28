"""Multisamples for the playable instruments (Decent Sampler / SFZ).

Sustaining sounds are built to loop perfectly: every oscillator runs a whole number of cycles inside the loop
(frequencies M * SR / L for integers M), static tone shaping is done on the spectrum of one loop (circular),
and anything that changes over time (organ percussion, key click) is confined to the attack segment before the
loop starts. Writes WAVs with an embedded smpl loop chunk and a JSON sidecar of roots / loop points.
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, struct, sys
import numpy as np
import soundfile as sf
import pedalboard as pb

sys.path.insert(0, PACK_ROOT + '/audio')
from dsp import SR, adsr, svf, bfilt, saw, square, noise, exp_env, n_samples, sat  # noqa: E402
import wave3 as W  # noqa: E402

OUT = PACK_ROOT + '/inst/lib/Samples'
META = {}
rng = np.random.default_rng(20261026)
NAMES = ['C', 'Cs', 'D', 'Eb', 'E', 'F', 'Fs', 'G', 'Ab', 'A', 'Bb', 'B']


def nname(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def m2f(m):
    return 440.0 * 2 ** ((m - 69) / 12)


# ---------------------------------------------------------------- periodic building blocks
def per_phase(M, L, n, phase0=0.0, vib=None):
    """Phase in cycles of an oscillator that completes exactly M cycles every L samples.
    vib=(depth, V): sinusoidal vibrato with V whole cycles per loop (keeps the loop exact)."""
    k = np.arange(n, dtype=float)
    ph = M * k / L + phase0
    inc = np.full(n, M / L)
    if vib:
        d, V = vib
        ph -= M * d / (2 * np.pi * V) * np.cos(2 * np.pi * V * k / L)
        inc = inc * (1 + d * np.sin(2 * np.pi * V * k / L))
    return ph, inc


def polyblep(t, dt):
    out = np.zeros_like(t)
    m1 = t < dt
    x = t[m1] / dt[m1]
    out[m1] = x + x - x * x - 1.0
    m2 = t > 1.0 - dt
    x = (t[m2] - 1.0) / dt[m2]
    out[m2] = x * x + x + x + 1.0
    return out


def saw_per(M, L, n, phase0=0.0, vib=None):
    ph, inc = per_phase(M, L, n, phase0, vib)
    p = ph % 1.0
    return 2.0 * p - 1.0 - polyblep(p, inc)


def circ_lp(x, fc, order=2):
    """Zero-phase Butterworth-shaped low-pass on one loop, applied to its spectrum (keeps the loop exact)."""
    L = x.shape[0]
    X = np.fft.rfft(x, axis=0)
    f = np.fft.rfftfreq(L, 1 / SR)
    H = 1 / np.sqrt(1 + (f / fc) ** (2 * order))
    if x.ndim == 2:
        H = H[:, None]
    return np.fft.irfft(X * H, L, axis=0)


def loop_len(f, T, even=False):
    N = round(f * T)
    if even and N % 2:
        N += 1
    L = round(N * SR / f)
    return N, L


# ---------------------------------------------------------------- WAV writer with smpl loop chunk
def write_wav(path, x, root=None, loop=None):
    """16-bit PCM. loop=(start, end) inclusive frames -> smpl chunk so other samplers pick the loop up too."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.asarray(x, dtype=float)
    ch = 1 if x.ndim == 1 else x.shape[1]
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype('<i2').tobytes()
    fmt = struct.pack('<HHIIHH', 1, ch, SR, SR * 2 * ch, 2 * ch, 16)
    chunks = b'fmt ' + struct.pack('<I', len(fmt)) + fmt
    if root is not None or loop is not None:
        loops = b''
        if loop:
            loops = struct.pack('<IIIIII', 0, 0, loop[0], loop[1], 0, 0)
        smpl = struct.pack('<IIIIIIIII', 0, 0, int(1e9 / SR), root if root is not None else 60, 0, 0, 0, 1 if loop else 0, 0) + loops
        chunks += b'smpl' + struct.pack('<I', len(smpl)) + smpl
    chunks += b'data' + struct.pack('<I', len(pcm)) + pcm
    if len(pcm) % 2:
        chunks += b'\0'
    with open(path, 'wb') as fh:
        fh.write(b'RIFF' + struct.pack('<I', 4 + len(chunks)) + b'WAVE' + chunks)


def rms(x):
    return float(np.sqrt(np.mean(np.square(x))))


def seam_db(x, ls, le):
    """Jump across the loop seam (x[le] -> x[ls]) relative to the ordinary sample-to-sample step there, in dB.
    Around 0 dB means the wrap is indistinguishable from any other pair of neighbouring samples."""
    jump = np.abs(x[ls] - x[le]).mean()
    step = 0.5 * (np.abs(x[ls + 1] - x[ls]).mean() + np.abs(x[le] - x[le - 1]).mean())
    return 20 * np.log10((jump + 1e-9) / (step + 1e-9))


def save(key, rel, x, root, loop=None, **extra):
    path = os.path.join(OUT, rel)
    write_wav(path, x, root, loop)
    n = x.shape[0]
    META[key] = dict(path='Samples/' + rel, root=root, frames=n, ch=1 if x.ndim == 1 else 2, loop=loop, peak=float(np.abs(x).max()), **extra)
    if loop:
        META[key]['seam'] = round(seam_db(x, loop[0], loop[1]), 2)


# ---------------------------------------------------------------- Reese
def reese(m, T=2.0):
    f = m2f(m)
    N, L = loop_len(f, T)
    x = np.zeros(L)
    for d, a in ((0, 1.0), (-1, 0.85), (1, 0.85), (2, 0.38)):
        x += a * saw_per(N + d, L, L, rng.uniform())
    x -= x.mean()
    x = circ_lp(x, 3600, 2)
    x = x / rms(x) * 0.25
    x = np.tanh(1.5 * x) / np.tanh(1.5)
    x -= x.mean()
    return x, L


# ---------------------------------------------------------------- garage organ (M1-ish drawbars)
ORGAN_H = [(0.5, 0.30), (1, 1.0), (2, 0.8), (3, 0.5), (4, 0.4), (6, 0.18), (8, 0.12)]


def organ(m, T=0.6, A=0.32):
    f = m2f(m)
    N, L = loop_len(f, T, even=True)
    a_n = int(A * SR)
    n = a_n + L
    k = np.arange(n)
    x = np.zeros(n)
    for h, amp in ORGAN_H:
        if f * h > SR * 0.45:
            continue
        roll = 1 / np.sqrt(1 + (f * h / 5200) ** 4)
        x += amp * roll * np.sin(2 * np.pi * (h * N) * k / L + rng.uniform(0, 2 * np.pi))
    t = k / SR
    win = np.clip((A - t) / 0.06, 0, 1)                       # the percussion and click are gone before the loop starts
    x += 0.45 * np.sin(2 * np.pi * 3 * N * k / L) * np.exp(-t / 0.05) * win
    x += 0.05 * bfilt(noise(n), 'bandpass', (2500, 7000)) * exp_env(n, 0.003) * win
    x = x / 2.6
    x = np.tanh(1.3 * 1.4 * x) / np.tanh(1.4)
    x -= x[a_n:].mean()
    return x, (a_n, a_n + L - 1)


# ---------------------------------------------------------------- string ensemble (stereo, vibrato)
def strings(m, T=2.0):
    f = m2f(m)
    N, L = loop_len(f, T)
    V = max(1, round(5.2 * L / SR))
    out = []
    for offs in ((-2, -1, 1), (-1, 1, 2)):
        x = np.zeros(L)
        for d in offs:
            x += saw_per(N + d, L, L, rng.uniform(), vib=(0.0032, V))
        x += 0.55 * saw_per(N, L, L, rng.uniform(), vib=(0.0032, V))
        out.append(x)
    x = np.stack(out, 1)
    x -= x.mean(0)
    x = circ_lp(x, 4300 if m < 72 else 5200, 2)
    lo = circ_lp(x, 140, 2)
    x = x - 0.6 * lo                                           # thin the very bottom so chords stay clear
    return x, L


# ---------------------------------------------------------------- Rhodes (two velocity layers)
def rhodes(m, vel):
    x = W.rhodes_note(m, 5.0, vel, 5.0)
    y = pb.Pedalboard([pb.HighpassFilter(30), pb.Distortion(drive_db=3), pb.Gain(-2)])(x.astype(np.float32)[None, :], SR)[0].astype(float)
    pk = np.abs(y).max()
    env = np.maximum.accumulate(np.abs(y[::-1]))[::-1]
    end = int(np.argmax(env < pk * 10 ** (-58 / 20))) or len(y)
    end = min(len(y), end + int(0.05 * SR))
    y = y[:end]
    fl = int(0.06 * SR)
    y[-fl:] *= np.linspace(1, 0, fl)
    return y


# ---------------------------------------------------------------- dub chord stab (dry; delay and room come from the plug-in)
def dub_dry(midis, dur=1.25, gate=0.85):
    n = n_samples(dur)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for mm in midis:
        f = m2f(mm)
        for d in (-7, 7):
            fv = f * 2 ** (d / 1200)
            x += saw(np.full(n, fv), phase0=rng.uniform()) + 0.5 * square(np.full(n, fv), pw=0.4)
    x /= len(midis) * 3
    x = svf(x, 420 + 2600 * np.exp(-t / 0.035) + 220 * np.exp(-t / 0.4), 1.6, 'lp')
    x = bfilt(x, 'highpass', 180, 2) * adsr(n, 0.002, 0.08, 0.32, 0.06, gate)
    fl = int(0.02 * SR)
    x[-fl:] *= np.linspace(1, 0, fl)
    return x


def main():
    os.makedirs(OUT, exist_ok=True)
    # Reese: every 3 semitones C1-C4, mono, whole file loops
    for m in range(24, 61, 3):
        x, L = reese(m)
        x = x / np.abs(x).max() * 0.89
        save(f'reese_{m}', f'Bass/Reese/Reese_{nname(m)}.wav', x, m, (0, L - 1))
    # Garage organ: C2-C6 every 3 semitones
    org = {}
    for m in range(36, 85, 3):
        org[m] = organ(m)
    g = 0.89 / max(np.abs(x).max() for x, _ in org.values())
    for m, (x, lp) in org.items():
        save(f'organ_{m}', f'Keys/GarageOrgan/GarageOrgan_{nname(m)}.wav', x * g, m, lp)
    # Strings: C2-C6 every 3 semitones, stereo loops
    st = {m: strings(m) for m in range(36, 85, 3)}
    for m, (x, L) in st.items():
        y = x / rms(x) * 0.16
        y = y / max(1.0, np.abs(y).max() / 0.89)
        save(f'strings_{m}', f'Keys/Strings/Strings_{nname(m)}.wav', y, m, (0, L - 1))
    # Rhodes: C2-C6 every 3 semitones, soft and hard layers (same gain per note keeps the layer balance)
    for m in range(36, 85, 3):
        hard = rhodes(m, 118)
        soft = rhodes(m, 62)
        gain = 0.2 / rms(hard[:int(0.4 * SR)])
        gain = min(gain, 0.89 / np.abs(hard).max())
        save(f'rhodes_{m}_hard', f'Keys/Rhodes/Rhodes_{nname(m)}_hard.wav', hard * gain, m, None, vel='hard')
        save(f'rhodes_{m}_soft', f'Keys/Rhodes/Rhodes_{nname(m)}_soft.wav', soft * gain, m, None, vel='soft')
    # Dub chords, all rooted on A3 (57)
    CH = {'m7': [57, 60, 64, 67], 'm9': [57, 60, 64, 67, 71], 'm11': [57, 62, 67, 72, 76], 'maj9': [57, 61, 64, 68, 71]}
    dub = {k: dub_dry(v) for k, v in CH.items()}
    g = 0.89 / max(np.abs(x).max() for x in dub.values())
    for k, x in dub.items():
        save(f'dub_{k}', f'Keys/DubChords/DubChord_A{k}.wav', x * g, 57, None, chord=k)
    json.dump(META, open(PACK_ROOT + '/inst/inst_meta.json', 'w'), indent=1)
    bad = {k: v['seam'] for k, v in META.items() if v.get('seam') is not None and v['seam'] > 3}
    print(len(META), 'samples; worst seams:', sorted(((v['seam'], k) for k, v in META.items() if v.get('seam') is not None), reverse=True)[:5], 'bad:', bad)


if __name__ == '__main__':
    main()
