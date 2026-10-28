"""Spectrogram + waveform sheet for a list of WAVs: python3 sheet.py out.png file1 file2 ..."""
import sys, numpy as np, soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy import signal
out, files = sys.argv[1], sys.argv[2:]
n = len(files)
cols = 2
rows = (n + cols - 1) // cols
fig, axes = plt.subplots(rows, cols * 2, figsize=(16, 2.0 * rows), gridspec_kw={'width_ratios': [1, 2] * cols})
axes = np.atleast_2d(axes)
for i, f in enumerate(files):
    x, sr = sf.read(f, always_2d=True)
    xm = x.mean(1)
    r, c = divmod(i, cols)
    a1, a2 = axes[r, c * 2], axes[r, c * 2 + 1]
    t = np.arange(len(xm)) / sr
    a1.plot(t, x[:, 0], lw=0.4)
    if x.shape[1] > 1: a1.plot(t, x[:, 1], lw=0.4, alpha=0.6)
    a1.set_ylim(-1, 1); a1.set_title(f.split('/')[-1][:48], fontsize=7); a1.tick_params(labelsize=6)
    nper = 2048 if len(xm) > 8192 else 512
    fr, tt, S = signal.spectrogram(xm, sr, nperseg=nper, noverlap=nper * 3 // 4)
    a2.pcolormesh(tt, fr, 10 * np.log10(S + 1e-12), shading='auto', vmin=-110, vmax=-20, cmap='magma')
    a2.set_yscale('symlog', linthresh=200); a2.set_ylim(20, sr / 2); a2.tick_params(labelsize=6)
for j in range(n, rows * cols):
    r, c = divmod(j, cols)
    axes[r, c * 2].axis('off'); axes[r, c * 2 + 1].axis('off')
plt.tight_layout()
plt.savefig(out, dpi=80)
print('wrote', out)
