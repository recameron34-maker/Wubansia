import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import sys, json, time
import numpy as np
import vocals as V, vocals2 as V2
FEM = ['af_alloy','af_aoede','af_bella','af_heart','af_jessica','af_kore','af_nicole','af_nova','af_river','af_sarah','af_sky','bf_alice','bf_emma','bf_isabella','bf_lily']
LINES = ['Luxury is so hot.', 'Ew. Is that economy?', 'Bitch, I\'m expensive.']
res = {}
for v in FEM:
    row = []
    for t in LINES:
        x = V.tts(t, v, 0.95)
        heard = V2.asr(x, V.TTS_SR)
        f0, sp, ap = V.analyze(x)
        m = V.hz2m(f0[f0 > 0])
        row.append((round(V2.score(t, heard), 2), round(float(np.median(m)), 1), round(float(np.percentile(m, 95) - np.percentile(m, 5)), 1), round(len(x) / V.TTS_SR, 2), heard))
    res[v] = row
    print(f"{v:12s} " + ' | '.join(f"asr {r[0]:.2f} med {r[1]:.0f} rng {r[2]:4.1f} {r[3]:.2f}s '{r[4]}'" for r in row), flush=True)
json.dump(res, open(PACK_ROOT + '/out/tmp/brat_aud.json', 'w'))
