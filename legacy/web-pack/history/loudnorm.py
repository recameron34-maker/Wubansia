import json, sys, soundfile as sf, numpy as np, pyloudnorm as pyln, pedalboard as pb
sys.path.insert(0, '/home/claude/work/audio')
from dsp import peaks
man_path = sys.argv[1]; target = float(sys.argv[2]); cats = sys.argv[3].split(',')
man = json.load(open(man_path))
meter = pyln.Meter(44100)
out = []
for m in man:
    if m['cat'] not in cats:
        continue
    path = '/home/claude/work/site/' + m['file']
    x, sr = sf.read(path)
    L = meter.integrated_loudness(x)
    y = x * 10 ** ((target - L) / 20)
    if np.abs(y).max() > 0.89:
        arr = y.T.astype(np.float32) if y.ndim == 2 else y[None].astype(np.float32)
        arr = pb.Pedalboard([pb.Compressor(threshold_db=-2.5, ratio=20, attack_ms=0.3, release_ms=60)])(arr, sr)
        y = arr.T if y.ndim == 2 else arr[0]
        y = y * min(1.0, 0.89 / np.abs(y).max())
    sf.write(path, y.astype(np.float32), sr, subtype='PCM_16')
    m['peaks'] = peaks(np.asarray(y, dtype=float), 72)
    out.append(round(meter.integrated_loudness(np.asarray(y, dtype=float)), 1))
json.dump(man, open(man_path, 'w'))
print('normalized', len(out), 'files; LUFS now', min(out), 'to', max(out))
