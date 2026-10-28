import sherpa_onnx, soundfile as sf, numpy as np, json, re
from scipy import signal
D='/home/claude/models/sherpa-onnx-whisper-base.en/'
rec = sherpa_onnx.OfflineRecognizer.from_whisper(encoder=D+'base.en-encoder.int8.onnx', decoder=D+'base.en-decoder.int8.onnx',
                                                tokens=D+'base.en-tokens.txt', language='en', task='transcribe', num_threads=2)
man = json.load(open('/home/claude/work/out/manifest_vox.json'))
def norm(s): return re.sub(r'[^a-z ]','', s.lower().replace('-', ' ')).split()
for m in man:
    if m['group'] in ('chops','stabs') or m['id'] not in ('c07','e08','e13'): continue
    for kind in ('dry','file'):
        x, sr = sf.read('/home/claude/work/site/'+m[kind])
        if x.ndim==2: x = x.mean(1)
        x = signal.resample_poly(x, 160, 441).astype(np.float32)
        s = rec.create_stream(); s.accept_waveform(16000, x); rec.decode_stream(s)
        txt = s.result.text.strip()
        want = set(norm(m['title'])); got = set(norm(txt))
        hit = len(want & got)/max(1,len(want))
        if kind=='dry': dry_txt, dry_hit = txt, hit
    flag = '' if min(hit, dry_hit) >= 0.6 else '  <-- check'
    print(f"{m['id']:4s} {m['title'][:34]:34s} | dry: {dry_txt[:38]:38s} | wet: {txt[:38]:38s}{flag}")
