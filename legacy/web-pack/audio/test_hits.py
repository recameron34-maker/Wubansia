import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import numpy as np, time
from dsp import *
from hits import ONE_SHOTS
t=time.time()
for name, fn, label, grp, key in ONE_SHOTS:
    x = fn()
    y = write(f'{PACK_ROOT}/site/audio/hits/{name}.wav', x, -1.0)
    m = y if y.ndim==1 else y.mean(1)
    spec = np.abs(np.fft.rfft(m*np.hanning(len(m))))
    fr = np.fft.rfftfreq(len(m), 1/SR)
    cent = (spec*fr).sum()/spec.sum()
    pk = fr[np.argmax(spec)]
    rms = 20*np.log10(np.sqrt((m**2).mean())+1e-12)
    print(f'{name:16s} {len(m)/SR:5.2f}s  rms {rms:6.1f} dB  centroid {cent:7.0f} Hz  peakfreq {pk:7.1f} Hz  key {key}')
print('took', round(time.time()-t,1))
