"""One PNG sheet per sound: waveform, spectrogram and the band envelopes the mud gate reads.

Light theme, colours from the dataviz reference palette (surface, ink, categorical slots 1-4, the blue sequential ramp).
The band panel shows each band against its own peak, as the gate measures it, with the -30 dB line and, for gated
sounds, the 120 ms (150-400 Hz) and 150 ms (400 Hz-1 kHz) marks.
"""
import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402
from scipy import signal  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import qa  # noqa: E402

SR = D.SR
SURFACE, INK, INK2, MUTED, GRID, AXIS = '#fcfcfb', '#0b0b0b', '#52514e', '#898781', '#e1e0d9', '#c3c2b7'
SERIES = ('#2a78d6', '#eb6834', '#1baf7a', '#eda100')
BLUES = LinearSegmentedColormap.from_list('blues', [SURFACE, '#cde2fb', '#86b6ef', '#3987e5', '#256abf', '#104281',
                                                    '#0d366b'])
BANDS = (('30-120 Hz', 30, 120), ('150-400 Hz', 150, 400), ('400 Hz-1 kHz', 400, 1000), ('2-6 kHz', 2000, 6000))


def _axes(ax, xlabel, ylabel):
    ax.set_facecolor(SURFACE)
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ('top', 'right'):
        ax.spines[side].set_visible(False)
    for side in ('left', 'bottom'):
        ax.spines[side].set_color(AXIS)
    ax.tick_params(colors=MUTED, labelsize=8)
    ax.set_xlabel(xlabel, color=INK2, fontsize=9)
    ax.set_ylabel(ylabel, color=INK2, fontsize=9)


def _spectrogram(ax, m):
    hop = max(128, len(m) // 1500)
    half = np.zeros(1024)                                   # frames centred from t = 0, so the attack shows
    f, t, S = signal.spectrogram(np.concatenate([half, m, half]), SR, window='hann', nperseg=2048,
                                 noverlap=max(0, 2048 - hop), mode='psd')
    t = t - len(half) / SR
    S = 10 * np.log10(S + 1e-20)
    S -= S.max()
    fl = np.geomspace(20, SR / 2, 360)
    img = np.array([np.interp(fl, f, col) for col in S.T]).T
    ext = [t[0] if len(t) else 0, t[-1] if len(t) else len(m) / SR, np.log10(20), np.log10(SR / 2)]
    im = ax.imshow(img, origin='lower', aspect='auto', extent=ext, cmap=BLUES, vmin=-90, vmax=0,
                   interpolation='nearest')
    ticks = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
    ax.set_yticks(np.log10(ticks), ['20', '50', '100', '200', '500', '1k', '2k', '5k', '10k', '20k'])
    _axes(ax, 'time (s)', 'Hz')
    ax.grid(False)
    cb = plt.colorbar(im, ax=ax, pad=0.01)
    cb.ax.tick_params(colors=MUTED, labelsize=7)
    cb.set_label('dB re peak', color=INK2, fontsize=8)
    cb.outline.set_edgecolor(AXIS)


def _bands(ax, m, gated, span):
    n = min(len(m), int(span * SR))
    t = np.arange(n) / SR * 1000
    ends = []
    for (label, lo, hi), colour in zip(BANDS, SERIES):
        e = qa.env_db(qa.band_zp(m, lo, hi))[:n]
        e -= e.max()
        ax.plot(t, np.maximum(e, -90), color=colour, linewidth=2, label=label)
        ends.append([max(e[-1], -88.0), label])
    ends.sort(key=lambda p: -p[0])                          # direct labels at the right edge, 7 dB apart
    for i in range(1, len(ends)):
        ends[i][0] = min(ends[i][0], ends[i - 1][0] - 7)
    lift = max(0.0, -88.0 - ends[-1][0])
    for y, label in ends:
        ax.annotate(label, (1.0, y + lift), xycoords=('axes fraction', 'data'), xytext=(4, 0),
                    textcoords='offset points', va='center', color=INK2, fontsize=7, annotation_clip=False)
    ax.axhline(-30, color=INK2, linewidth=1, linestyle='--')
    ax.annotate('-30 dB', (t[-1] if len(t) else 0, -30), xytext=(-4, 3), textcoords='offset points', color=INK2,
                fontsize=7, ha='right')
    if gated:
        for ms, label, side in ((qa.MUD_FALL_MS, '150-400 Hz gate', 'right'), (qa.BOX_FALL_MS, '400 Hz-1 kHz gate',
                                                                                  'left')):
            ax.axvline(ms, color=MUTED, linewidth=1, linestyle=':')
            ax.annotate(f'{label}, {ms:g} ms', (ms, -86), xytext=(-3 if side == 'right' else 3, 0),
                        textcoords='offset points', color=INK2, fontsize=7, ha=side)
    ax.set_ylim(-90, 3)
    ax.set_xlim(0, t[-1] if len(t) else 1)
    _axes(ax, 'time (ms)', 'dB re band peak')
    ax.legend(loc='lower left', bbox_to_anchor=(0, 1.0), ncol=4, fontsize=7, frameon=False, labelcolor=INK2)


def draw(path, x, title, lines=(), gated=False):
    """Write one sheet. x: the take (float, (n,) or (n, 2)); lines: text under the title (about, QA summary)."""
    x = D.st(x)
    m = x.mean(1)
    side = 0.5 * (x[:, 0] - x[:, 1])
    t = np.arange(len(m)) / SR
    fig = plt.figure(figsize=(11, 8.6), dpi=110, facecolor=SURFACE)
    gs = fig.add_gridspec(3, 1, height_ratios=(1.0, 1.6, 1.3), hspace=0.42, top=0.86, bottom=0.07, left=0.07,
                          right=0.90)
    fig.text(0.07, 0.95, title, color=INK, fontsize=13, fontweight='bold')
    for i, line in enumerate(lines):
        fig.text(0.07, 0.925 - 0.022 * i, line, color=INK2, fontsize=8.5)
    ax = fig.add_subplot(gs[0])
    ax.plot(t, m, color=SERIES[0], linewidth=0.6, label='mid')
    if np.any(side):
        ax.plot(t, side, color=SERIES[1], linewidth=0.6, label='side')
    ax.set_xlim(0, t[-1] if len(t) else 1)
    ax.set_ylim(-1, 1)
    _axes(ax, 'time (s)', 'amplitude')
    if np.any(side):
        ax.legend(loc='upper right', fontsize=7, frameon=False, labelcolor=INK2)
    _spectrogram(fig.add_subplot(gs[1]), m)
    _bands(fig.add_subplot(gs[2]), m, gated, span=0.6 if gated or len(m) / SR <= 3 else 3.0)
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)
