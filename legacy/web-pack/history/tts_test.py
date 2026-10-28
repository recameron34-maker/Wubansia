import sherpa_onnx, soundfile as sf, time, sys
D = "/home/claude/models/kokoro-multi-lang-v1_0/"
def make(lang, lex):
    cfg = sherpa_onnx.OfflineTtsConfig(
        model=sherpa_onnx.OfflineTtsModelConfig(
            kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(
                model=D+"model.onnx", voices=D+"voices.bin", tokens=D+"tokens.txt",
                data_dir=D+"espeak-ng-data", dict_dir=D+"dict",
                lexicon=D+lex, lang=lang),
            num_threads=2, debug=False, provider="cpu"),
        max_num_sentences=1)
    return sherpa_onnx.OfflineTts(cfg)
t0=time.time()
us = make("en-us", "lexicon-us-en.txt")
gb = make("en", "lexicon-gb-en.txt")
print("load", time.time()-t0)
tests = [
 (gb, 26, "Big up all the ravers. Rewind!", "bm_george"),
 (gb, 27, "Bring it back, selecta.", "bm_lewis"),
 (gb, 24, "Back to the garage.", "bm_daniel"),
 (gb, 21, "Lost in the music.", "bf_emma"),
 (us, 6, "Come closer. Take it deeper.", "af_nicole"),
 (us, 16, "Move your fucking body.", "am_michael"),
 (us, 17, "This shit is filthy.", "am_onyx"),
 (us, 3, "Feel the bass.", "af_heart"),
 (us, 2, "Shake that ass.", "af_bella"),
 (us, 14, "Turn it up.", "am_fenrir"),
]
for tts, sid, text, name in tests:
    t=time.time()
    a = tts.generate(text, sid=sid, speed=1.0)
    if len(a.samples)==0: print("FAIL", name); continue
    sf.write(f"vtest/{name}.wav", a.samples, a.sample_rate)
    print(name, a.sample_rate, len(a.samples)/a.sample_rate, "s", round(time.time()-t,2))
