"""Wave 8 vocals: tech-house vocal shots for the Chris Lake / Riordan-style kits. Short, pitched down where it suits,
driven, a little bit-crushed, a short room and one dotted-eighth slap. Original words only.
WAV sources go to out/vox_src/{vox,vox_dry}/<id>.wav; entries to out/manifest_w8_vox.json.
Run: python3 render_w8_vox.py [id ...]"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import difflib, json, os, re, sys
import numpy as np
from dsp import SR, peaks, bfilt, board, pb, to_stereo
import vocals as V
import vocals2 as V2
import vocals3 as V3
import render_vocals2 as RV
import render_vocals6 as R6

W = PACK_ROOT
MAN = f'{W}/out/manifest_w8_vox.json'
BASE = 'Vocals/Ad-libs/Tech House (Chris Lake & Riordan-style)'
SHOTS = [   # id, text, voice, pitch (semitones), note
    ('ts01', 'Yeah!', 'am_onyx', -3, 'deep, pitched down three semitones'),
    ('ts02', 'Yo!', 'am_onyx', 0, 'deep voice, straight'),
    ('ts03', 'Hey!', 'af_sky', 0, 'bright, for the offbeats'),
    ('ts04', 'Jack!', 'am_onyx', -2, 'pitched down, driven'),
    ('ts05', 'Louder!', 'bf_isabella', 0, 'for the build'),
    ('ts06', 'Come on!', 'bm_george', -2, 'pitched down'),
    ('ts07', 'Grooove!', 'am_michael', -2, 'pitched down'),
    ('ts08', 'Move!', 'af_nicole', 0, 'bright'),
]


TITLE = {'ts07': 'Groove!'}     # the spelling that makes the voice say it right isn't the one to print


def letters(s):
    return re.sub(r'[^a-z]', '', s.lower())


def fuzzy(a, b):
    return difflib.SequenceMatcher(None, letters(a), letters(b)).ratio()


def best_take(text, voice, speeds=(1.0, 0.92, 1.08)):
    best = None
    for sp in speeds:
        x = V.tts(text, voice, sp)
        heard = V2.asr(x, V3.TTS_SR)
        f = fuzzy(text, heard) + (0.02 if sp == 1.0 else 0)
        if best is None or f > best[0]:
            best = (f, x, sp, heard)
    return best


def tech_chain(x44, pitch, male, bpm=128):
    y = board(x44, [pb.PitchShift(semitones=pitch)]) if pitch else x44
    y = V2.clean(y, male)
    live = np.flatnonzero(np.abs(y) > np.abs(y).max() * 10 ** (-45 / 20))
    y = y[max(0, live[0] - int(0.006 * SR)):].copy()
    y[:int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))
    y = RV.tail_trim(y, -60)
    dry = V3.finish(y, -12.5)
    v = board(y, [pb.Compressor(threshold_db=-22, ratio=5.0, attack_ms=1.5, release_ms=80)])
    v = V.parallel_dist(v, 10, 0.3)
    v = np.tanh(2.0 * (v / (np.abs(v).max() + 1e-9) + 0.1 * (v / (np.abs(v).max() + 1e-9)) ** 2)) / np.tanh(2.0)
    v = 0.78 * v + 0.22 * board(v, [pb.Bitcrush(bit_depth=9)])
    v = board(v, [pb.HighpassFilter(110 if male else 160), pb.PeakFilter(2800, 3.0, 0.9)])
    n = len(v) + int(1.4 * SR)
    wet = np.zeros((n, 2))
    wet[:len(v)] += to_stereo(v)
    room = board(to_stereo(np.concatenate([v, np.zeros(n - len(v))])), [pb.Reverb(room_size=0.28, damping=0.5, wet_level=1.0, dry_level=0.0, width=0.9), pb.HighpassFilter(300)])
    wet += 0.16 * room
    d = int(0.75 * 60 / bpm * SR)                       # one dotted-eighth slap, high-passed, a little to the right
    slap = bfilt(np.concatenate([np.zeros(d), v]), 'highpass', 600, 2)[:n - 0] if d + len(v) <= n else bfilt(np.concatenate([np.zeros(d), v]), 'highpass', 600, 2)[:n]
    slap = np.concatenate([slap, np.zeros(max(0, n - len(slap)))])[:n]
    wet[:, 0] += 0.12 * slap
    wet[:, 1] += 0.2 * slap
    wet = RV.tail_trim(wet, -55)
    return V3.finish(wet, -12.0), dry


def main(only):
    man = json.load(open(MAN)) if os.path.exists(MAN) else []
    man = [e for e in man if not only or e['id'] not in only]
    for sid, text, voice, pitch, note in SHOTS:
        if only and sid not in only:
            continue
        f, x, sp, heard = best_take(text, voice)
        male = voice in V3.MALE
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        wet, dry = tech_chain(x44, pitch, male)
        R6.write_src(sid, wet, dry)
        hw = V2.asr(wet.mean(axis=1), SR)
        hd = V2.asr(dry, SR)
        title = TITLE.get(sid, text)
        base = f"VOX_{sid.upper()}_TechHouse_{re.sub(r'[^A-Za-z]', '', title.title())}_128bpm"
        man.append({'id': sid, 'cat': 'vox', 'group': 'adlibs', 'genre': 'mth', 'fam': 'th', 'title': title, 'voice': R6.DESC.get(voice, voice), 'style': 'shot',
                    'explicit': False, 'bpm': 128, 'note': f'tech-house vocal shot, {note}: driven, a little bit-crushed, short room and a dotted-eighth slap',
                    'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav', 'pack': f'{BASE}/{base}.wav',
                    'packDry': f'{BASE}/Dry/{base}_DRY.wav', 'peaks': peaks(wet, 72), 'stereo': True, 'new': True, 'wave': 8, 'asr': heard})
        print(f'{sid} {text!r} {voice} take {sp} closeness {f:.2f} | tts: {heard} | dry: {hd} | wet: {hw} | {len(wet) / SR:.2f}s', flush=True)
    man.sort(key=lambda e: e['id'])
    json.dump(man, open(MAN, 'w'), indent=0)


if __name__ == '__main__':
    main(set(sys.argv[1:]))
