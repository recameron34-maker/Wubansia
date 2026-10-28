"""Wave 8 FX for the groove-house and tech-house kits: an endless (Shepard) riser, an uplifter, a downlifter, a DJ
backspin, a tech impact and a crash into a long reverb. Original synthesis. Writes lib/Samples/FX/... and w8_fx.json."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys
import numpy as np
import soundfile as sf
import pedalboard as pb

sys.path.insert(0, PACK_ROOT + '/audio')
sys.path.insert(0, PACK_ROOT + '/inst')
from dsp import SR, bfilt, board  # noqa: E402
import hits as H  # noqa: E402
import render_w8 as R8  # noqa: E402

W = PACK_ROOT
LIB = f'{W}/inst/lib'
OUT = {}
rng = np.random.default_rng(4242)


def bars(n, bpm):
    return n * 4 * 60 / bpm


def endless_riser(bpm=128, nb=4):
    """Shepard-Risset glissando: octave-spaced partials that climb forever under a bell-shaped spectral window, plus a
    noise swell whose high-pass rises; the whole thing gets louder and brighter and stops dead on the downbeat."""
    T = bars(nb, bpm)
    n = int(T * SR)
    t = np.arange(n) / SR
    rate = 1.5 / T                                   # octaves per second of the climb
    f_lo, octs = 55.0, 7
    Lc, Rc = np.zeros(n), np.zeros(n)
    for k in range(octs):
        pos = (k + rate * t) % octs                  # position in octaves above f_lo
        f = f_lo * 2 ** pos
        amp = 0.5 - 0.5 * np.cos(2 * np.pi * pos / octs)
        ph = np.cumsum(f) / SR
        wave = np.sin(2 * np.pi * ph) + 0.25 * np.sin(4 * np.pi * ph + 0.3)
        Lc += amp * wave
        Rc += amp * np.sin(2 * np.pi * ph * 1.003 + 1.1) + amp * 0.25 * np.sin(4 * np.pi * ph * 1.003)
    tone = np.stack([Lc, Rc], 1) / octs
    nz = rng.standard_normal((n, 2))
    nz = np.stack([R8.ladder(nz[:, c], lambda tt: 200 + 7000 * (tt / T) ** 2, res=0.2, mode='hp12') for c in (0, 1)], 1)
    x = tone * (0.25 + 0.75 * (t / T) ** 1.5)[:, None] + 0.35 * nz * ((t / T) ** 2.2)[:, None]
    x = board(x, [pb.Chorus(rate_hz=0.4, depth=0.25, centre_delay_ms=7, feedback=0.0, mix=0.3)])
    x[-int(0.004 * SR):] *= np.linspace(1, 0, int(0.004 * SR))[:, None]
    x[:int(0.4 * SR)] *= np.linspace(0, 1, int(0.4 * SR))[:, None] ** 2
    return R8.norm(x, 0.89), T


def uplifter(bpm=128, nb=2):
    T = bars(nb, bpm)
    n = int(T * SR)
    t = np.arange(n) / SR
    nz = rng.standard_normal((n, 2))
    y = np.stack([R8.ladder(nz[:, c], lambda tt: 300 * (30 ** (tt / T)), res=0.55, mode='bp12') for c in (0, 1)], 1)
    y = board(y, [pb.Phaser(rate_hz=0.8, depth=0.7, centre_frequency_hz=1500, feedback=0.4, mix=0.4)])
    y *= (0.1 + 0.9 * (t / T) ** 1.8)[:, None]
    y[-int(0.003 * SR):] *= np.linspace(1, 0, int(0.003 * SR))[:, None]
    return R8.norm(y, 0.89), T


def downlifter(bpm=128, nb=2):
    T = bars(nb, bpm)
    n = int(T * SR)
    t = np.arange(n) / SR
    nz = rng.standard_normal((n, 2))
    y = np.stack([R8.ladder(nz[:, c], lambda tt: 9000 * (1 / 30) ** (tt / T), res=0.45, mode='bp12') for c in (0, 1)], 1)
    sub = np.sin(2 * np.pi * np.cumsum(90 * (1 / 3) ** (t / T)) / SR) * np.exp(-t / (T / 2.5))
    y = y * (np.exp(-t / (T / 2.2)))[:, None] * 0.8 + sub[:, None] * 0.5
    y[:int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))[:, None]
    return R8.norm(R8.fade_out(y, 0.2), 0.89), T


def backspin(bpm=130):
    """A DJ backspin: one bar of the groove-house chords and drums pulled backwards fast, slowing to a stop."""
    lp = f'{LIB}/Samples/Loops/Groove House/GV01_GlowChords_Csmin-12A_130bpm.wav'
    dp = f'{LIB}/Samples/Loops/Groove House/GV01_DrumLoop_CanalLightsGroove_130bpm.wav'
    a, _ = sf.read(lp, always_2d=True)
    d, _ = sf.read(dp, always_2d=True)
    m = min(len(a), len(d))
    src = 0.6 * a[:m] + 0.8 * d[:m]
    T = 1.1
    n = int(T * SR)
    t = np.arange(n) / SR
    v = 3.2 * (1 - t / T) ** 1.6                     # backwards speed, falling to zero
    pos = int(1.6 * SR) - np.cumsum(v)               # start a little way into the loop and run backwards
    pos = np.clip(pos, 0, m - 2)
    i0 = np.floor(pos).astype(int)
    fr = pos - i0
    y = src[i0] * (1 - fr)[:, None] + src[i0 + 1] * fr[:, None]
    y = np.stack([R8.ladder(y[:, c], lambda tt: 12000 * (1 - tt / T) ** 1.2 + 300, res=0.1) for c in (0, 1)], 1)
    y *= (1 - (t / T) ** 3)[:, None]
    y[:int(0.003 * SR)] *= np.linspace(0, 1, int(0.003 * SR))[:, None]
    return R8.norm(R8.fade_out(y, 0.05), 0.89), T


def impact():
    T = 3.2
    n = int(T * SR)
    t = np.arange(n) / SR
    sub = np.sin(2 * np.pi * np.cumsum(30 + 34 * np.exp(-t / 0.18)) / SR) * np.exp(-t / 0.9)
    crack = bfilt(rng.standard_normal(n), 'bandpass', (400, 6000), 2) * np.exp(-t / 0.06)
    crack = np.tanh(3 * crack) * 0.6
    body = np.sin(2 * np.pi * np.cumsum(120 * np.exp(-t / 0.05) + 60) / SR) * np.exp(-t / 0.15) * 0.5
    dry = sub * 0.9 + crack + body
    wet = board(R8.stereo(np.concatenate([np.zeros(int(0.01 * SR)), crack + body])[:n]),
                [pb.Reverb(room_size=0.92, damping=0.6, wet_level=1.0, dry_level=0.0, width=1.0), pb.HighpassFilter(150), pb.LowpassFilter(6000)])
    y = R8.stereo(dry) + 0.6 * wet
    return R8.norm(R8.fade_out(y, 0.5), 0.89), T


def crash_verb():
    c = H.crash(2.4)
    T = 4.5
    n = int(T * SR)
    x = np.zeros(n)
    x[:len(c)] = c
    wet = board(R8.stereo(x), [pb.Reverb(room_size=0.93, damping=0.45, wet_level=1.0, dry_level=0.0, width=1.0), pb.HighpassFilter(400)])
    y = R8.stereo(x) + 0.7 * wet
    return R8.norm(R8.fade_out(y, 0.6), 0.89), T


def main():
    specs = [
        ('fx8_riser', 'Endless Riser', 'Shepard riser: octave-spaced tones that climb forever, a noise swell on top; stops dead on the downbeat, 4 bars at 128 BPM', endless_riser, 'Tech House', 128, 4),
        ('fx8_uplift', 'Uplifter', 'noise through a resonant band-pass sweeping up five octaves, with a phaser; 2 bars at 128 BPM', uplifter, 'Tech House', 128, 2),
        ('fx8_down', 'Downlifter', 'noise sweeping down with a falling sub, 2 bars at 128 BPM', downlifter, 'Tech House', 128, 2),
        ('fx8_backspin', 'Backspin', 'a bar of the groove-house chords and drums pulled backwards and slowing to a stop', backspin, 'Groove House', None, None),
        ('fx8_impact', 'Tech Impact', 'sub drop, distorted crack and a long dark tail', impact, 'Tech House', None, None),
        ('fx8_crash', 'Crash Into Reverb', 'a crash into a long hall for transitions', crash_verb, 'Groove House', None, None),
    ]
    for sid, title, note, fn, fam, bpm, nb in specs:
        y, T = fn()
        stem = title.replace(' ', '')
        rel = f"Samples/FX/{fam}/{'GV' if fam == 'Groove House' else 'TH'}_FX_{stem}" + (f'-{nb}bars_{bpm}bpm' if bpm else '') + '.wav'
        R8.write_wav(f'{LIB}/{rel}', y)
        OUT[sid] = dict(path=rel, title=title, note=note, fam='gv' if fam == 'Groove House' else 'th', bpm=bpm, bars=nb, ch=2)
        print(sid, rel, f'{len(y) / SR:.2f}s')
    json.dump(OUT, open(f'{W}/inst/w8_fx.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
