"""Brat pack: a rich girl being a brat about luxury. Spoken lines, talk-sung / sung chants (keyed loops) and ad-libs."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
import pedalboard as pb
from dsp import SR, bfilt, board, step_seconds, write, peaks
import vocals as V
import vocals2 as V2
import brat as B
import render_vocals2 as RV

V.analyze = B.analyze_refined     # stricter voicing for every WORLD step in this run (attitude, chants, sung words)
SITE = PACK_ROOT + '/site/audio'
K = RV.K
OUT = []
N = RV.N

# ------------------------------------------------------------------ attitude presets
US = dict(lift=1.0, g=1.3, formant=1.03)
DEAD = dict(lift=1.0, g=1.4, formant=1.03)          # bored, drawn-out
POSH = dict(lift=0.5, g=1.4, formant=1.02)
PITCH = dict(lift=4.0, g=1.2, formant=1.09)          # hyperpop
NONE = dict()


def P(base, **kw):
    d = dict(base); d.update(kw); return d


# id, genre, text, voice, style, attitude, bpm, explicit
SPOKEN = [
    ('b01', 'ukg', 'Champagne, darling. Obviously.', 'bf_isabella', 'brat', P(POSH, fry=0.14), 133, False),
    ('b02', 'ukg', "If it's not designer, I don't want it.", 'af_heart', 'brat', US, 134, False),
    ('b03', 'ukg', "Hot girls don't wait in line.", 'bf_emma', 'bratphone', P(POSH, lift=1.0, g=1.35), 136, False),
    ('b04', 'ukg', 'Fuck it. Buy both.', 'af_heart', 'brat', P(US, fry=0.16), 138, True),
    ('b05', 'ukg', 'First class, bitch. Keep up.', 'bf_isabella', 'brat', POSH, 132, True),
    ('b06', 'mth', 'Luxury is so hot right now.', 'af_nicole', 'bratwhisper', NONE, 128, False),
    ('b07', 'mth', 'Diamonds are, like, mandatory.', 'af_bella', 'brat', P(DEAD, lift=1.5, g=1.5, formant=1.04, uptalk=3.5), 129, False),
    ('b08', 'mth', "My taste is expensive. You can't afford me.", 'af_bella', 'bratphone', P(DEAD, fry=0.15), 131, False),
    ('b09', 'mth', 'Spoil me, or fuck off.', 'af_nicole', 'bratwhisper', NONE, 130, True),
    ('b10', 'mth', 'Shut the fuck up and pour the champagne.', 'af_bella', 'brat', DEAD, 132, True),
    ('b11', 'sg', "Eew. Economy? I don't fly economy.", 'af_heart', 'brat', US, 134, False),
    ('b12', 'sg', 'Is that a fake bag? Tragic.', 'af_heart', 'bratphone', P(US, fry=0.14), 132, False),
    ('b13', 'sg', 'This dress costs more than your car.', 'bf_isabella', 'brat', P(POSH, fry=0.14), 133, False),
    ('b14', 'sg', 'Cheap boys can fuck right off.', 'bf_isabella', 'bratpitch', PITCH, 135, True),
    ('b15', 'sg', "Bitch, I'm the V.I.P.", 'af_heart', 'bratpitch', PITCH, 136, True),
    ('b16', 'ch', 'Rich is the new hot.', 'af_heart', 'brat', P(US, fry=0.14), 128, False),
    ('b17', 'ch', "Put it on daddy's card.", 'af_sarah', 'brat', P(US, g=1.35, uptalk=3.0), 129, False),
    ('b18', 'ch', "I'm not spoiled. I'm just expensive.", 'af_heart', 'brat', US, 130, False),
    ('b19', 'ch', "Pay for my drinks, or don't fucking talk to me.", 'af_bella', 'brat', DEAD, 128, True),
    ('b20', 'ch', 'Money looks so good on me, bitch.', 'bf_isabella', 'bratphone', POSH, 132, True),
]
# fallback voices if the first choice doesn't transcribe cleanly
ALT = {'af_heart': ['af_sarah', 'af_bella'], 'af_bella': ['af_heart', 'af_sarah'], 'af_sarah': ['af_heart', 'af_bella'],
       'bf_isabella': ['bf_emma', 'af_heart'], 'bf_emma': ['bf_isabella', 'af_heart'], 'af_nicole': ['af_heart', 'af_bella']}

ADLIBS = [  # id, genre, text, voice, attitude, bpm, explicit
    ('ba01', 'ukg', 'Eew.', 'af_heart', US, 134, False),
    ('ba02', 'ukg', 'Obviously.', 'bf_isabella', P(POSH, fry=0.12), 133, False),
    ('ba03', 'mth', 'Whatever.', 'af_bella', P(DEAD, fry=0.14), 128, False),
    ('ba04', 'mth', 'Duh.', 'af_bella', P(DEAD, g=1.15), 130, False),
    ('ba05', 'sg', 'Kiss kiss.', 'af_heart', P(US, lift=2.0), 134, False),
    ('ba06', 'sg', 'Bye!', 'bf_isabella', P(POSH, g=1.6), 136, False),
    ('ba07', 'ch', 'Hard pass.', 'af_bella', P(DEAD, fry=0.14), 128, False),
    ('ba08', 'ch', 'Excuse me?', 'af_sarah', P(US, uptalk=4.0), 129, False),
    ('ba09', 'ukg', 'So hot.', 'af_heart', P(US, fry=0.18), 132, False),
    ('ba10', 'mth', 'Pay me.', 'af_heart', DEAD, 131, False),
    ('ba11', 'sg', 'Cute.', 'bf_isabella', P(POSH, g=1.6), 133, False),
    ('ba12', 'ch', 'Bitch, please.', 'af_bella', P(DEAD, fry=0.14), 130, True),
]

# ------------------------------------------------------------------ chants (4-bar keyed loops)
# talk: (step16, note, text) spoken on the grid, pitch pulled toward the note (contour keeps some speech melody)
# sung: (step16, len16, note, text) through the sung-hook engine, '~' = next syllable of the same word
CHANTS = [
    ('bh1', 'mth', 'MTH01', 'af_heart', 'talk', False, 'Luxury is hot, broke is not', dict(contour=0.35, formant=1.03), 'brat', [
        (0, 'A3', 'Luxury'), (4, 'A3', 'is'), (6, 'C4', 'hot.'), (16, 'G3', 'Broke'), (20, 'G3', 'is'), (22, 'A3', 'not.'),
        (32, 'A3', 'Luxury'), (36, 'A3', 'is'), (38, 'E4', 'hot.'), (48, 'G3', 'Broke'), (52, 'A3', 'is'), (54, 'A3', 'not.'), (60, 'C4', 'Bye.')]),
    ('bh2', 'mth', 'MTH04', 'af_bella', 'talk', True, 'Rich bitch energy', dict(contour=0.2, formant=1.02), 'brat', [
        (0, 'C4', 'Rich bitch.'), (8, 'Eb4', 'Energy.'), (16, 'C4', 'Rich bitch.'), (24, 'Bb3', 'Energy.'),
        (32, 'C4', 'Rich bitch.'), (40, 'Eb4', 'Energy.'), (48, 'C4', 'Rich bitch.'), (56, 'G3', 'Energy.')]),
    ('bh3', 'ukg', 'UKG05', ['af_heart', 'af_sarah'], 'sung', False, 'Expensive taste, pretty face', dict(harm=True), 'brat', [
        (0, 2, 'Bb4', 'Expensive'), (2, 2, 'C5', '~'), (4, 2, 'Bb4', '~'), (6, 6, 'G4', 'taste'),
        (16, 2, 'Eb5', 'pretty'), (18, 2, 'D5', '~'), (20, 8, 'Bb4', 'face'),
        (32, 2, 'Bb4', 'Expensive'), (34, 2, 'C5', '~'), (36, 2, 'Eb5', '~'), (38, 6, 'F5', 'taste'),
        (48, 2, 'G5', 'pretty'), (50, 2, 'F5', '~'), (52, 10, 'Eb5', 'face')]),
    ('bh4', 'sg', 'SG02', 'af_heart', 'talk', False, 'Big bass, big bag', dict(contour=0.3, formant=1.03), 'brat', [
        (0, 'A3', 'Big bass,'), (8, 'C#4', 'big bag.'), (16, 'A3', 'Big bass,'), (24, 'E4', 'big bag.'),
        (32, 'A3', 'Big bass,'), (40, 'C#4', 'big bag.'), (48, 'A3', 'Big bass,'), (56, 'E4', 'big bag.')]),
    ('bh5', 'ch', 'CH05', ['af_heart', 'af_sarah'], 'sung', False, 'Buy me diamonds, baby', dict(harm=True), 'brat', [
        (0, 3, 'F5', 'Buy'), (3, 3, 'Db5', 'me'), (6, 2, 'Eb5', 'diamonds'), (8, 6, 'Db5', '~'),
        (16, 2, 'Bb4', 'baby'), (18, 10, 'F4', '~'),
        (32, 3, 'F5', 'Buy'), (35, 3, 'Db5', 'me'), (38, 2, 'Eb5', 'diamonds'), (40, 6, 'F5', '~'),
        (48, 2, 'Ab5', 'baby'), (50, 10, 'F5', '~')]),
    ('bh6', 'ukg', 'UKG06', 'af_heart', 'talk', True, 'First class, bitch', dict(contour=0.0, formant=1.1, octave=True), 'bratpitch', [
        (0, 'E4', 'First class,'), (8, 'B4', 'bitch.'), (16, 'G4', 'Champagne only.'),
        (32, 'E4', 'First class,'), (40, 'B4', 'bitch.'), (48, 'E4', 'First class,'), (56, 'D5', 'bitch.'), (61, 'B4', 'Bye!')]),
]

# ------------------------------------------------------------------ scoring
NORM = {'daw': 'duh', 'dah': 'duh', 'by': 'buy', 'bye': 'buy', 'eew': 'ew', 'eww': 'ew', 'ewww': 'ew', 'eeew': 'ew', 'vip': 'vip', 'uhh': 'uh', 'um': 'uh',
        'darlin': 'darling', 'kiss': 'kiss', 'daddys': 'daddys', 'okay': 'ok'}


def nwords(s):
    s = s.replace('V.I.P.', 'VIP')
    return [NORM.get(w, w) for w in V2.words(s)]


def score(text, heard):
    want = set(nwords(text)); got = nwords(heard)
    if not want:
        return 0.0
    hit = sum(1 for w in want if w in got) / len(want)
    noise = sum(1 for g in got if g not in want) / max(1, len(got))
    return hit - 0.3 * noise


def best_take(text, voices, speeds=(1.0, 0.92, 1.08), need=0.9, rounds=2):
    """Try the first voice at three speeds (Kokoro varies a little run to run, so up to `rounds` passes);
    only reach for the fallback voices if nothing transcribes cleanly."""
    best = None
    for i, v in enumerate(voices):
        for _ in range(rounds):
            for sp in speeds:
                x = V.tts(text, v, sp)
                heard = V2.asr(x, V.TTS_SR)
                sc = score(text, heard) + (0.02 if sp == 1.0 else 0) - 0.05 * i
                if best is None or sc > best[0]:
                    best = (sc, x, sp, heard, v)
            if best[0] >= need:
                return best
    return best


def slug(s):
    return RV.slug(s)


def verify_units(units, voice, tries=6):
    """Same idea for the talk chants: keep a take of each spoken unit that whisper hears as written."""
    for w in dict.fromkeys(units):
        k = (w, voice, 1.0)
        if k in B._W:
            continue
        best = None
        for _ in range(tries):
            x = V.tts(w, voice, 1.0)
            sc = score(w, V2.asr(x, V.TTS_SR))
            if best is None or sc > best[0]:
                best = (sc, x)
            if sc >= 1.0:
                break
        f0, sp_, ap = B._HARVEST(best[1])
        B._W[k] = (best[1], f0, sp_, ap)
        if best[0] < 1.0:
            print(f'      (no clean take of "{w}" for {voice}; best {best[0]:.2f})', flush=True)


def verify_words(words_, voice, tries=8):
    """Kokoro sometimes mispronounces a word said on its own ('spoil' -> 'spile'), and the sung engine builds each word from
    such a take. Pre-load the sung-word cache with a take whisper actually hears as that word."""
    for w in dict.fromkeys(words_):
        if (w, voice) in V2._WORD:
            continue
        best = None
        variants = [w + '.', w.capitalize() + '.', w + '!', w.capitalize() + '!']
        for k in range(tries):
            sp = (0.95, 0.9, 1.0)[(k // len(variants)) % 3]
            x = V.tts(variants[k % len(variants)], voice, sp)
            sc = score(w, V2.asr(x, V.TTS_SR))
            if best is None or sc > best[0]:
                best = (sc, x)
            if sc >= 1.0:
                break
        x = best[1]
        f0, sp_, ap = V.analyze(x)
        V2._WORD[(w, voice)] = (x, f0, sp_, ap, np.log(np.sum(sp_[:, 8:200], axis=1) + 1e-12))
        if best[0] < 1.0:
            print(f'      (no clean take of "{w}" for {voice}; best {best[0]:.2f})', flush=True)


def save(kind, sid, title, wet, dry, bpm, genre, voice, explicit, note, keylabel=None, cam=None, bars=None, trim=True):
    os.makedirs(f'{SITE}/vox', exist_ok=True); os.makedirs(f'{SITE}/vox_dry', exist_ok=True)
    wet = write(f'{SITE}/vox/{sid}.wav', RV.tail_trim(wet) if trim else wet, -1.0)
    folder = {'spoken': 'Vocals/Brat/Spoken', 'chant': 'Vocals/Brat/Chants', 'adlib': 'Vocals/Brat/Ad-libs'}[kind]
    ksuf = ('_' + keylabel.replace(' minor', 'min').replace(' major', 'maj').replace('#', 's') + ('-' + cam if cam else '')) if keylabel else ''
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_Brat-{RV.GN[genre].replace(' ', '')}_{slug(title)[:34]}{ksuf}_{bpm}bpm"
    e = {'id': sid, 'cat': 'vox', 'group': 'brat', 'kind': kind, 'genre': genre, 'title': title, 'voice': V.VOICE_DESC.get(voice, voice),
         'explicit': explicit, 'bpm': bpm, 'key': keylabel, 'camelot': cam, 'note': note, 'bars': bars,
         'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'pack': f'{folder}/{base}.wav', 'peaks': peaks(wet, 72), 'stereo': True}
    if dry is not None:
        write(f'{SITE}/vox_dry/{sid}.wav', RV.tail_trim(dry, -65) if trim else dry, -1.0)
        e['dry'] = f'audio/vox_dry/{sid}.wav'; e['packDry'] = f'{folder}/Dry/{base}_DRY.wav'
    OUT.append({k: v for k, v in e.items() if v is not None})


STYLE_NOTE = {'brat': 'close & glossy, delay throw', 'bratphone': 'voice-note phone filter', 'bratpitch': 'pitched-up hyperpop',
              'bratwhisper': 'close whisper', 'bratadlib': 'ad-lib, short echo'}


def render_spoken(only=None):
    for sid, genre, text, voice, style, att, bpm, explicit in SPOKEN:
        if only and sid not in only:
            continue
        t0 = time.time()
        sc, x, sp, heard, v = best_take(text, [voice] + ALT.get(voice, []))
        y = B.attitude(x, **att)
        x44 = bfilt(V.up(y), 'highpass', 70, 2)
        wet = B.prepend_breath(B.brat_chain(x44, style, bpm), -31 if style != 'bratwhisper' else -26)
        h2 = V2.asr(wet.mean(axis=1), SR)
        note = STYLE_NOTE[style] + (' · valley-girl rise' if att.get('uptalk') else '') + (' · vocal fry' if att.get('fry') else '')
        save('spoken', sid, text, wet, V2.clean(x44), bpm, genre, v, explicit, note)
        OUT[-1]['asr'] = h2
        print(f'  {sid} {v:11s} {sp:.2f}x score {sc:4.2f} | {text:48s} | heard: {h2}  ({time.time()-t0:.0f}s)', flush=True)


def render_adlibs(only=None):
    for sid, genre, text, voice, att, bpm, explicit in ADLIBS:
        if only and sid not in only:
            continue
        sc, x, sp, heard, v = best_take(text, [voice] + ALT.get(voice, []), need=0.99)
        y = B.attitude(x, **att)
        x44 = bfilt(V.up(y), 'highpass', 70, 2)
        wet = B.brat_chain(x44, 'bratadlib', bpm)
        save('adlib', sid, text, wet, None, bpm, genre, v, explicit, 'ad-lib, short echo' + (' · vocal fry' if att.get('fry') else ''))
        OUT[-1]['asr'] = V2.asr(wet.mean(axis=1), SR)
        if score(text, OUT[-1]['asr']) < 0.5:     # whisper hallucinates on tiny clips with echo; check the dry performance too
            OUT[-1]['asr'] += ' | dry: ' + V2.asr(y, V.TTS_SR)
        print(f'  {sid} {v:11s} score {sc:4.2f} | {text:16s} | heard: {OUT[-1]["asr"]}', flush=True)


def render_chants(only=None):
    for sid, genre, kid, voice, mode, explicit, title, opt, style, ev in CHANTS:
        if only and sid not in only:
            continue
        t0 = time.time()
        kit = K[kid]; bpm, sw = kit['bpm'], kit['swing']
        L = int(4 * 16 * step_seconds(bpm) * SR)
        if mode == 'talk':
            evs = [(s, N(n), w) for s, n, w in ev]
            verify_units([w for _, _, w in evs], voice)
            c, f = opt['contour'], opt['formant']
            lead = B.chant_line(evs, voice, bpm, sw, 4, contour=c, formant=f)
            d1 = B.chant_line(evs, voice, bpm, sw, 4, contour=c, formant=f, cents=8, delay=0.012)
            d2 = B.chant_line(evs, voice, bpm, sw, 4, contour=c, formant=f, cents=-8, delay=0.021)
            octv = B.chant_line([(s, m + 12, w) for s, m, w in evs], voice, bpm, sw, 4, contour=c, formant=f) if opt.get('octave') else None
            wet = B.chant_mix(lead, [d1, d2], bpm, L, style, octv)
            note = ('talk-sung, autotuned flat' if c == 0 else 'talk-sung chant') + ' + 2 doubles' + (' + octave' if octv is not None else '') + f', sits on {kid} {kit["name"]}'
        else:
            notes = [(s, l, N(n), w) for s, l, n, w in ev]
            o = RV.auto_octave(notes, False)
            notes = [(s, l, m + o, w) for s, l, m, w in notes]
            best = None
            for v in (voice if isinstance(voice, list) else [voice]):
                verify_words([w for _, _, _, w in notes if w != '~'], v)
                ld = V2.sing_line2(notes, v, bpm, sw, 4, vib=16)
                h = V2.asr(ld[:L], SR); sc = score(title, h) - (0.35 if '(' in h or '[' in h else 0)
                key = nwords(title)[0]; want_n = sum(1 for e in ev if nwords(e[3] if e[3] != '~' else 'x')[:1] == [key])
                sc *= 0.5 + 0.5 * min(1.0, nwords(h).count(key) / max(1, want_n))
                print(f'      {sid} try {v:10s} octave {o:+d} score {sc:4.2f} heard: {h}', flush=True)
                if best is None or sc > best[0]:
                    best = (sc, v, ld)
            _, voice, lead = best
            d1 = V2.sing_line2(notes, voice, bpm, sw, 4, cents=7, delay=0.014, vib=12)
            d2 = V2.sing_line2(notes, voice, bpm, sw, 4, cents=-7, delay=0.023, vib=20)
            har = V2.sing_line2(V2.harmony_notes(notes, RV.SCALE(kit['root'], kit['mode'])), voice, bpm, sw, 4, vib=14) if opt.get('harm') else None
            wet = V2.hook_mix(lead, [d1, d2], har, bpm, L)
            wet = np.stack([B.air(wet[:, 0]), B.air(wet[:, 1])], axis=1)
            note = f"sung lead + 2 doubles{' + 3rd harmony' if har is not None else ''}, sits on {kid} {kit['name']}"
        heard = V2.asr(lead[:L], SR)
        save('chant', sid, title, wet, V2.clean(lead[:L]), bpm, genre, voice, explicit, note, kit['key']['label'], kit['key']['camelot'], 4, trim=False)
        OUT[-1]['asr'] = heard
        print(f'  {sid} {voice:11s} {kit["key"]["label"]:9s} {bpm} | {title:32s} | heard: {heard}  ({time.time()-t0:.0f}s)', flush=True)


if __name__ == '__main__':
    only = sys.argv[1:] or None
    t0 = time.time()
    print('spoken'); render_spoken(only)
    print('chants'); render_chants(only)
    print('adlibs'); render_adlibs(only)
    path = PACK_ROOT + '/out/manifest_brat.json'
    if only:   # merge re-renders into the full manifest
        full = json.load(open(path)); pos = {m['id']: i for i, m in enumerate(full)}
        for e in OUT:
            full[pos[e['id']]] = e
        OUT[:] = full
    json.dump(OUT, open(path, 'w'))
    print('done', len(OUT), 'in', round(time.time() - t0), 's')
