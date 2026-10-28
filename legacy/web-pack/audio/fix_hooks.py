"""Re-sing the weaker hooks: try voice/octave variants, keep whichever lead transcribes best, then render the full mix."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, time
import numpy as np
import render_vocals2 as RV
import vocals2 as V2
from dsp import SR, step_seconds

VARIANTS = {
    'h02': [('bf_emma', -12), ('af_heart', -12), ('af_bella', -12), ('af_heart', 0)],
    'h04': [('af_heart', -12), ('af_bella', -12), ('bf_emma', -12)],
    'h08': [('af_bella', 0), ('af_heart', 0), ('bf_emma', 0), ('af_heart', -12)],
    'h10': [('af_heart', -12), ('af_bella', -12), ('af_heart', 0), ('bf_emma', -12)],
    'h11': [('bm_george', 0), ('bm_fable', 0), ('am_puck', 0), ('am_michael', 0)],
    'h13': [('af_nicole', -12), ('af_bella', -12), ('af_heart', -12), ('af_bella', 0)],
    'h14': [('af_heart', 0), ('af_heart', -12), ('af_bella', -12), ('bf_emma', -12)],
    'h15': [('bf_emma', 0), ('bf_emma', -12), ('af_heart', -12), ('af_bella', -12)],
    'h17': [('am_fenrir', 0), ('am_michael', 0), ('am_puck', 0), ('bm_george', 0)],
    'h18': [('af_bella', 0), ('af_heart', 0), ('af_bella', -12)],
}


def judge(title, heard):
    sc = V2.score(title, heard)
    if '(' in heard or '[' in heard:   # whisper tags like (buzzing) mean audible artefacts
        sc -= 0.35
    return sc


def main():
    hooks = {h[0]: h for h in RV.HOOKS}
    man = json.load(open(PACK_ROOT + '/out/manifest_vox2.json'))
    pos = {m['id']: i for i, m in enumerate(man)}
    for hid, opts in VARIANTS.items():
        sid, genre, kid, voice0, explicit, harm, oct0, title, mel = hooks[hid]
        kit = RV.K[kid]; bpm, sw = kit['bpm'], kit['swing']
        base = [(s, l, RV.N(n), w) for s, l, n, w in mel]
        L = int(4 * 16 * step_seconds(bpm) * SR)
        best = None
        for voice, octv in opts:
            notes = [(s, l, m + octv, w) for s, l, m, w in base]
            lead = V2.sing_line2(notes, voice, bpm, sw, 4, vib=16 if voice in RV.MALE else 18)
            heard = V2.asr(lead[:L], SR)
            sc = judge(title, heard)
            print(f'  {hid} {voice:10s} {octv:+3d}  score {sc:5.2f}  heard: {heard[:90]}', flush=True)
            if best is None or sc > best[0]:
                best = (sc, voice, octv, lead, heard)
        sc, voice, octv, lead, heard = best
        notes = [(s, l, m + octv, w) for s, l, m, w in base]
        d1 = V2.sing_line2(notes, voice, bpm, sw, 4, cents=7, delay=0.014, vib=13)
        d2 = V2.sing_line2(notes, voice, bpm, sw, 4, cents=-7, delay=0.023, vib=21)
        har = V2.sing_line2(V2.harmony_notes(notes, RV.SCALE(kit['root'], kit['mode'])), voice, bpm, sw, 4, vib=14) if harm else None
        wet = V2.hook_mix(lead, [d1, d2], har, bpm, L)
        dry = V2.clean(lead[:L], voice in RV.MALE)
        RV.OUT.clear()
        note = f"sung lead + 2 doubles{' + 3rd harmony' if harm else ''}, sits on {kid} {kit['name']}"
        RV.save('hooks', sid, title, wet, dry, bpm, genre, kit['key']['label'], kit['key']['camelot'], voice, 'sung', explicit, note,
                'Vocals/Sung Hooks', trim=False, bars=4)
        e = RV.OUT[-1]; e['asr'] = heard
        man[pos[hid]] = e
        print(f'  -> {hid} kept {voice} {octv:+d} (score {sc:.2f})', flush=True)
    json.dump(man, open(PACK_ROOT + '/out/manifest_vox2.json', 'w'))
    print('updated manifest')


if __name__ == '__main__' and False:
    t0 = time.time(); main(); print('done in', round(time.time() - t0), 's')


SPOKEN_FIX = {
    'v14': ('Bass in your face!', ['bm_george', 'am_puck', 'bm_fable', 'am_michael']),
    'v15': ('Dub plate pressure!', ['bm_george', 'bm_fable', 'am_puck']),
    'a04': ('Yeah! Yeah!', ['bf_emma', 'af_heart', 'af_bella']),
}


def fix_spoken():
    from dsp import bfilt, board
    import pedalboard as pb
    import vocals as V
    man = json.load(open(PACK_ROOT + '/out/manifest_vox2.json'))
    pos = {m['id']: i for i, m in enumerate(man)}
    specs = {s[0]: s for s in RV.SPOKEN}
    ads = {a[0]: a for a in RV.ADLIBS}
    for sid, (text, voices) in SPOKEN_FIX.items():
        best = None
        for v in voices:
            sc, x, sp, heard = V2.best_take(text, v)
            print(f'  {sid} {v:10s} score {sc:5.2f} heard: {heard}', flush=True)
            if best is None or sc > best[0]:
                best = (sc, x, sp, heard, v)
        sc, x, sp, heard, voice = best
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        RV.OUT.clear()
        if sid in specs:
            _, genre, title, _, style, bpm, pitch, explicit = specs[sid]
            wet = V2.spoken_chain(x44, style, bpm, pitch, voice in RV.MALE)
            note = {'mc': 'MC, delay throw', 'radio': 'pirate-radio filter'}.get(style, style) + f' · best of 3 takes ({sp:.2f}x)'
            RV.save('explicit' if explicit else 'clean', sid, title, wet, V2.clean(x44, voice in RV.MALE), bpm, genre, None, None, voice, style, explicit, note,
                    'Vocals/Explicit' if explicit else 'Vocals/Clean')
        else:
            _, genre, title, _, bpm = ads[sid]
            wet = V2.spoken_chain(x44, 'adlib', bpm, 0, voice in RV.MALE)
            RV.save('adlibs', sid, title, wet, V2.clean(x44, voice in RV.MALE), bpm, genre, None, None, voice, 'adlib', False, 'ad-lib, short echo', 'Vocals/Ad-libs')
        RV.OUT[-1]['asr'] = heard
        man[pos[sid]] = RV.OUT[-1]
        print(f'  -> {sid} kept {voice} ({sc:.2f})', flush=True)
    json.dump(man, open(PACK_ROOT + '/out/manifest_vox2.json', 'w'))


if __name__ == '__main__' and False:
    fix_spoken()
