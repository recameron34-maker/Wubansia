import sherpa_onnx, soundfile as sf, numpy as np, pyworld as pw
D = "/home/claude/models/kokoro-multi-lang-v1_0/"
def make(lang, lex):
    cfg = sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(
        kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(model=D+"model.onnx", voices=D+"voices.bin", tokens=D+"tokens.txt",
        data_dir=D+"espeak-ng-data", dict_dir=D+"dict", lexicon=D+lex, lang=lang), num_threads=2, provider="cpu"), max_num_sentences=1)
    return sherpa_onnx.OfflineTts(cfg)
us = make("en-us", "lexicon-us-en.txt")
for txt in ["Ooh.", "Oooh.", "Yeah.", "Oh.", "Ah.", "Hey!", "Uh!", "Woo!", "Baby.", "Come on!", "Huh!", "Lose yourself.", "Rewind!"]:
    a = us.generate(txt, sid=3, speed=1.0)
    x = np.array(a.samples, dtype=np.float64)
    f0, t = pw.harvest(x, a.sample_rate, f0_floor=70, f0_ceil=700, frame_period=5.0)
    v = f0[f0>0]
    print(f"{txt:16s} {len(x)/a.sample_rate:5.2f}s voiced {len(v)*5/1000:5.2f}s  f0 med {np.median(v) if len(v) else 0:6.1f} range {v.min() if len(v) else 0:6.1f}-{v.max() if len(v) else 0:6.1f}")
