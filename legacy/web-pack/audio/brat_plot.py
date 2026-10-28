import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import numpy as np, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy import signal
import vocals as V
SP = PACK_ROOT + '/out/tmp/'
vs = ['af_heart', 'af_bella', 'af_sarah', 'af_nicole', 'af_sky', 'af_nova', 'af_jessica', 'af_kore', 'bf_emma', 'bf_alice', 'bf_lily', 'bf_isabella']
t = 'Luxury is so hot.'
fig, axs = plt.subplots(len(vs), 2, figsize=(14, 2.0 * len(vs)))
for i, v in enumerate(vs):
    x = V.tts(t, v, 0.95)
    f0, sp, ap = V.analyze(x)
    tt = np.arange(len(f0)) * 0.005
    m = np.where(f0 > 0, V.hz2m(f0), np.nan)
    axs[i, 0].plot(tt, m, '.', ms=2); axs[i, 0].set_ylim(40, 75); axs[i, 0].set_ylabel(v, rotation=0, ha='right'); axs[i, 0].grid(alpha=.3)
    f, ts, S = signal.spectrogram(x, 24000, nperseg=512, noverlap=448)
    axs[i, 1].pcolormesh(ts, f, 10 * np.log10(S + 1e-12), vmin=-110, vmax=-40, shading='auto'); axs[i, 1].set_ylim(0, 8000)
plt.tight_layout(); plt.savefig(SP + 'brat_contours.png', dpi=60)
