"""Written phrases and loops, for any kit: the 16th grid on the kit's one swing, hits placed with their tails wrapping
round the loop, processing over two passes so a loop joins, the ducker a bus insert runs on the kick, and a kit's files
exactly as render.py writes them (so a context loop plays the kit's own masters).

Every lane of a phrase sits on one Grid built from the kit (its BPM and SWING), so hits that share a step land together
(plan section 2.3). They came out of FUT01's context loop (recipes/fut01/context.py) unchanged, so FUT01 renders the same
bytes.

    G = Grid(K, 64)                                  # 4 bars of 16ths at the kit's tempo and swing
    buf = hits_lane(K, 'KickA', ((0, 1.0), (10, 0.8)), G)
"""
import functools
import io

import numpy as np
import soundfile as sf

from tools.sound import dsp48 as D
from tools.sound.item import Out

SR = D.SR


def loop_len(kit, steps):
    """A loop of `steps` 16ths at the kit's tempo, in samples."""
    return int(round(steps * kit.STEP * SR))


class Grid:
    """A phrase's 16th grid: `steps` 16ths over `length` samples (default loop_len(kit, steps)), every odd 16th on the
    kit's one swing."""

    def __init__(self, kit, steps, length=None):
        self.swing = kit.SWING
        self.steps = steps
        self.length = loop_len(kit, steps) if length is None else length
        self.s16 = self.length / steps

    def at(self, step, swing=None):
        """A 16th as a sample position: odd 16ths land later, the same for every lane; it wraps round the loop."""
        swing = self.swing if swing is None else swing
        s = step + (2 * swing - 1 if step % 2 else 0)
        return int(np.floor(s * self.s16 + 0.5)) % self.length


def place(buf, x, pos):
    """Add a sound at pos, its tail wrapping round the loop (buf: (L, 2))."""
    np.add.at(buf, (pos + np.arange(len(x))) % len(buf), D.st(x))


def pan(x, p):
    """Equal-power pan (unity in the centre), p in -1..1."""
    a = (p + 1) * np.pi / 4
    return D.st(x) * (np.sqrt(2) * np.array([np.cos(a), np.sin(a)]))


def second_pass(y, seam, L, power=False):
    """y[L:2L] with its last `seam` seconds crossfaded into y[L - seam:L], the samples that precede it, so it joins
    when it loops. power: an equal-power crossfade, for a held voice whose two ends are different moments of it."""
    out = np.array(y[L:2 * L])
    X = int(seam * SR)
    u = np.linspace(0.0, 1.0, X)[:, None]
    w_out, w_in = (np.cos(0.5 * np.pi * u), np.sin(0.5 * np.pi * u)) if power else (1 - u, u)
    out[-X:] = w_out * out[-X:] + w_in * y[L - X:L]
    return out


def two_pass(fn, x, seam):
    """fn over two passes of a loop, keeping the second; its last `seam` seconds blend into the samples that precede
    the second pass, so the loop joins even where fn's state isn't exactly periodic (a shimmer's grains, a tape's
    wow)."""
    return second_pass(fn(np.concatenate([x, x])), seam, len(x))


def circular(fn, x):
    """A filter run over three passes of a loop, keeping the middle: its state joins at the seam."""
    L = len(x)
    return fn(np.concatenate([x, x, x]))[L:2 * L]


def pump_db(onsets, L, depth_db, ramp, hold, release):
    """A ducker's gain in dB per sample over a loop of L samples: at each onset (a kick), down depth_db over `ramp`,
    held, then a raised-cosine back up over `release`."""
    g = np.zeros(L)
    t = np.arange(L)
    for p in onsets:
        tau = ((t - p) % L) / SR
        u = np.clip((tau - ramp - hold) / release, 0.0, 1.0)
        e = np.where(tau < ramp, tau / ramp, np.where(u < 1.0, 0.5 + 0.5 * np.cos(np.pi * u), 0.0))
        g = np.minimum(g, -depth_db * e)
    return g


@functools.lru_cache(maxsize=None)
def kit_files(kit, name):
    """Every file of a kit item exactly as render.py writes it (rendered, given its one gain, through 24-bit FLAC and
    back), keyed by (note, layer, rr), as Outs with their loop points; with the item and its gain in dB. kit: the kit's
    module."""
    from tools.sound import kits
    from tools.sound import render as R
    item = kits.items(kit, only=(name,))[0]
    done = R.render_item(item, kit.KIT)
    got = {}
    for t, o in zip(done.takes, done.outs):
        y, _ = sf.read(io.BytesIO(R.encode(o.audio)), dtype='float64')
        got[(t.note, t.layer, t.rr)] = Out(D.st(y), loop=o.loop)
    return item, got, done.gain_db


def hits_lane(kit, name, hits, grid):
    """One-shots of a kit item on the grid: (16th, velocity) each, the layer nearest its velocity (the louder on a
    tie), round robins in turn."""
    item, got, _ = kit_files(kit, name)
    buf = np.zeros((grid.length, 2))
    for k, (step, v) in enumerate(hits):
        layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))
        place(buf, got[(None, layer, 1 + k % item.rr)].audio, grid.at(step))
    return buf
