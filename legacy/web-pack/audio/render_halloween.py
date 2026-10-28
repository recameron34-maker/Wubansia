"""Halloween wave (vocals): spooky spoken drops (clean + explicit), ad-libs, an evil laugh and a witch's cackle, the witches'
chant from Macbeth (1606, public domain) sung as a coven loop and whispered, and three original sung hooks.

WAV sources go to out/vox_src/{vox,vox_dry}/<id>.wav (packed to Ogg FLAC by pack_vox_flac.py); manifest entries go to
out/manifest_halloween.json. All lyrics are original except the Macbeth couplet.
Run: python3 render_halloween.py [section|id ...]   sections: spoken adlibs laughs chant whisper hooks
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
from scipy.signal import fftconvolve
from dsp import SR, step_seconds, peaks, bfilt, board, pb, to_stereo
import vocals as V
import vocals2 as V2
import vocals3 as V3
import render_vocals2 as RV
import render_vocals6 as R6

W = PACK_ROOT
MAN = f'{W}/out/manifest_halloween.json'
KITS = R6.KITS
DESC = R6.DESC
N = RV.N
PRE = R6.PRE
LOG = []
V3.EXTRA_LEX.update({'mwa': 'm w ˈ ɑ', 'hee': 'h ˈ i', 'heh': 'h ˈ ɛ', 'cobwebs': 'k ˈ ɑ b w ɛ b z', "nobody's": 'n ˈ O b ˌ ɑ d i z',
                      'masks': 'm ˈ æ s k s', 'cameron': 'k ˈ æ m ə ɹ ə n', "everybody's": 'ˈ ɛ v ɹ i b ə d i z', 'a': 'ə',
                      "secret's": 's ˈ i k ɹ ɪ t s', 'zebedee': 'z ˈ ɛ b ə d i', 'spookington': 's p ˈ u k ɪ ŋ t ə n',
                      'mcgillicuddy': 'm ə ɡ ˈ ɪ l ɪ k ʌ d i'})


def log(*a):
    s = ' '.join(str(x) for x in a)
    print(s, flush=True)
    LOG.append(s)


def load_manifest():
    return json.load(open(MAN)) if os.path.exists(MAN) else []


def put(entry):
    entry['wave'] = 7
    m = [e for e in load_manifest() if e['id'] != entry['id']]
    m.append(entry)
    m.sort(key=lambda e: e['id'])
    json.dump(m, open(MAN, 'w'), indent=0)


def slug(t, n=40):
    """Title -> CamelCase file-name part, cut at a word boundary (at most n characters)."""
    import re
    out = ''
    for w in re.sub(r"[^A-Za-z0-9' ]+", ' ', t).replace("'", '').split():
        w = w[:1].upper() + w[1:]
        if out and len(out) + len(w) > n:
            break
        out += w
    return out


FOLDER = {'spoken': 'Halloween/Vocals/Spoken', 'explicit': 'Halloween/Vocals/Explicit', 'adlibs': 'Halloween/Vocals/Ad-libs & Laughs',
          'sung': 'Halloween/Vocals/Chants & Hooks', 'castle': 'Halloween/Murder at Cameron Castle', 'supper': 'Halloween/Murder at Cameron Castle',
          'zf': 'Halloween/Murder at Cameron Castle', 'names': 'Halloween/Murder at Cameron Castle/Name Stabs',
          'drama': 'Halloween/Murder at Cameron Castle/Drama Stabs'}


def dry_base(base, style):
    """A stab's key is the orchestral hit's; its dry is the voice alone, so the dry file name leaves the key out."""
    import re
    return re.sub(r'_[A-G]s?(?:min|maj)-\d+[AB](?=_\d+bpm$)', '', base) if style == 'stab' else base


def entry(sid, sub, title, voice, style, bpm, explicit, note, wet, dry, heard=None, **kw):
    folder = FOLDER['explicit' if explicit and sub == 'spoken' else sub]
    keybit = ''
    if kw.get('key'):
        keybit = '_' + kw['key'].replace(' minor', 'min').replace(' major', 'maj').replace('#', 's') + ('-' + kw['camelot'] if kw.get('camelot') else '')
    bars = f"-{kw['bars']}bars" if kw.get('bars') else ''
    tag = 'CameronCastle' if sub in ('castle', 'supper', 'zf', 'names', 'drama') else 'Halloween'
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_{tag}_{slug(kw.pop('short', title))}{bars}{keybit}_{bpm}bpm"
    e = {'id': sid, 'cat': 'vox', 'group': 'halloween', 'hsub': sub, 'genre': 'all', 'title': title, 'voice': DESC.get(voice, voice) if isinstance(voice, str) else voice,
         'style': style, 'explicit': explicit, 'bpm': bpm, 'note': note, 'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav',
         'dry': f'audio/vox_dry/{sid}.wav', 'pack': f'{folder}/{base}.wav', 'packDry': f'{folder}/Dry/{dry_base(base, style)}_DRY.wav', 'peaks': peaks(wet, 72),
         'stereo': True, 'new': True}
    if heard is not None:
        e['asr'] = heard
    e.update({k: v for k, v in kw.items() if v is not None})
    return e


# ================================================================= spoken drops (best of three takes, the pack's spoken chains)
SPOKEN = [
    # id, text, voice, style, bpm, pitch, explicit
    ('hv01', 'Welcome to the haunted house. Mind the stairs.', 'af_nicole', 'whisper', 128, 0, False),
    ('hv02', 'The dead are dancing tonight.', 'am_onyx', 'dark', 128, -2, False),
    ('hv03', "It's the witching hour.", 'bm_george', 'dark', 132, -2, False),
    ('hv04', "Don't go down to the basement.", 'bf_lily', 'whisper', 128, 0, False),
    ('hv05', "Who's behind the mask?", 'af_sky', 'whisper', 130, 0, False),
    ('hv06', 'Graveyard shift. Everybody move.', 'bm_lewis', 'mc', 132, 0, False),
    ('hv07', 'Something wicked is on the dancefloor.', 'am_michael', 'preacher', 128, 0, False),
    ('hv08', 'Lock the doors. Nobody leaves till sunrise.', 'af_kore', 'dark', 128, -1, False),
    ('hv09', 'Live from the graveyard, this is pirate radio.', 'bm_daniel', 'radio', 134, 0, False),
    ('hv10', "Trick or treat? Fuck it, I'll take both.", 'bf_isabella', 'diva', 132, 0, True),
    ('hv11', "Scary? Honey, I'm fucking terrifying.", 'af_bella', 'dark', 130, -1, True),
    ('hv12', 'Bite me. No, really. Bite me.', 'af_nicole', 'whisper', 128, 0, True),
    ('hv13', "Let's wake the fucking dead.", 'bm_george', 'mc', 132, 0, True),
]
ADLIBS = [
    # id, text, voice, bpm, explicit
    ('hv14', 'Boo!', 'af_bella', 128, False),
    ('hv15', 'Trick or treat!', 'bf_isabella', 132, False),
    ('hv16', 'Happy Halloween!', 'af_sarah', 128, False),
    ('hv17', 'Run!', 'bm_george', 132, False),
    ('hv18', 'Behind you!', 'bf_emma', 128, False),
    ('hv19', 'Oh, hell no!', 'af_kore', 128, True),
    ('hv20', 'Holy shit, a ghost!', 'am_onyx', 130, True),
]
STYLE_NOTE = {'mc': 'MC, delay throw', 'radio': 'pirate-radio filter', 'whisper': 'whisper, wide space', 'preacher': 'warm, close, big room',
              'diva': 'clean, plate + echo'}


def haunt(x, bpm, male):
    """Extra space for the dark drops: a long dark hall under the pack's dark chain."""
    st = to_stereo(x) if x.ndim == 1 else x
    hall = hall_ir(3.2, 0.03, seed=7, dark=0.55)
    out = np.zeros((len(st) + len(hall), 2))
    out[:len(st)] += st
    return addconv(out, st.mean(axis=1), hall, 0.11)


def addconv(out, x, ir, g):
    """out (n, 2) += x convolved with each channel of the stereo impulse ir, times g."""
    for c in (0, 1):
        y = fftconvolve(x, ir[:, c])
        k = min(len(out), len(y))
        out[:k, c] += y[:k] * g
    return out


def hall_ir(sec, predelay, seed=3, dark=0.5):
    """Stereo hall impulse: exponentially decaying noise, darker as it decays (one-pole low-pass sweeping down)."""
    rng = np.random.default_rng(seed)
    n = int(sec * SR)
    t = np.arange(n) / SR
    env = np.exp(-6.9 * t / sec)
    ir = rng.standard_normal((n, 2)) * env[:, None]
    from scipy import signal as sg
    # darken: blend a low-passed copy in as it decays
    lp = sg.lfilter(*sg.butter(2, 2200 / (SR / 2)), ir, axis=0)
    mix = np.clip(t / sec * (1 + dark), 0, 1)[:, None]
    ir = ir * (1 - mix) + lp * mix
    ir = sg.lfilter(*sg.butter(2, 180 / (SR / 2), 'high'), ir, axis=0)
    pd = np.zeros((int(predelay * SR), 2))
    ir = np.concatenate([pd, ir])
    return ir / np.sqrt(np.sum(ir ** 2) / 2) * 0.5


def render_spoken(only=None):
    for sid, text, voice, style, bpm, pitch, explicit in SPOKEN:
        if only and sid not in only and 'spoken' not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        male = voice in V3.MALE
        dry = board(x44, [pb.PitchShift(semitones=pitch)]) if (style == 'dark' and pitch) else x44
        wet = V2.spoken_chain(x44, style, bpm, pitch, male)
        if style == 'dark':
            wet = haunt(wet, bpm, male)
        wet, dry = RV.tail_trim(wet), RV.tail_trim(V2.clean(dry, male), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -13.0)
        note = (f'pitched {pitch} st, grit + delay, long dark hall' if style == 'dark' else STYLE_NOTE[style]) + f' · best of 3 takes ({sp:.2f}x)'
        R6.write_src(sid, wet, dry)
        put(entry(sid, 'spoken', text, voice, style, bpm, explicit, note, wet, dry, heard))
        log(f'{sid} {voice} {style} score {sc:.2f} heard: {heard}')


def render_adlibs(only=None):
    for sid, text, voice, bpm, explicit in ADLIBS:
        if only and sid not in only and 'adlibs' not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        male = voice in V3.MALE
        wet = RV.tail_trim(V2.spoken_chain(x44, 'adlib', bpm, 0, male))
        dry = RV.tail_trim(V2.clean(x44, male), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -12.5)
        R6.write_src(sid, wet, dry)
        put(entry(sid, 'adlibs', text, voice, 'adlib', bpm, explicit, 'ad-lib, short echo', wet, dry, heard))
        log(f'{sid} {voice} adlib score {sc:.2f} heard: {heard}')


# ================================================================= laughs: built syllable by syllable on the WORLD vocoder
# A laugh is a run of breathy "h" onsets and short vowels about five a second, high in the voice and falling across the run.
# The vowel's own spectral envelope (from a spoken "ha"/"hee") shapes both the vowel and its aspiration (an [h] is a voiceless
# copy of the vowel that follows). Each laugh is the first of up to eight takes the recogniser hears as laughing.
def vowel_frames(word, voice, speed=0.9):
    x, toks = V3.KO().speak([word + '.'], voice, speed)
    f0, sp, ap = V3.analyze(x)
    v = np.flatnonzero(f0 > 0)
    a, b = v[0] + int(len(v) * 0.2), v[0] + int(len(v) * 0.8)
    return f0[a:b], sp[a:b], ap[a:b]


def laugh_world(voice, word, syls, seed=3, breath=0.25, h_gain=0.35, speed=0.9):
    """syls: [(t0 sec, h sec, vowel sec, f0 start Hz, f0 end Hz, amp)] -> 44.1 kHz mono."""
    import pyworld as pw
    from scipy import signal as sg
    rng = np.random.default_rng(seed)
    vf0, vsp, vap = vowel_frames(word, voice, speed)
    FP = V3.FP
    total = max(t + h + v for t, h, v, *_ in syls) + 0.3
    nf = int(total * 1000 / FP) + 1
    nb = vsp.shape[1]
    f0 = np.zeros(nf)
    sp = np.full((nf, nb), 1e-14)
    ap = np.ones((nf, nb))
    kind = np.ones(nf, int)
    k = 0
    for t, h, v, fa, fb, amp in syls:
        i0, ih, iv = int(t * 1000 / FP), int(h * 1000 / FP), int(v * 1000 / FP)
        for j in range(ih):
            g = h_gain * (0.4 + 0.6 * j / max(1, ih - 1))
            sp[i0 + j] = vsp[(k + j) % len(vsp)] * (g * amp) ** 2
        for j in range(iv):
            q = j / max(1, iv - 1)
            env = min(1.0, (j + 1) / 3) * (1 - q ** 3) ** 0.7
            f0[i0 + ih + j] = fa * (fb / fa) ** (q ** 0.8) * 2 ** (rng.normal(0, 8) / 1200)
            sp[i0 + ih + j] = vsp[(k + j) % len(vsp)] * (amp * env + 1e-4) ** 2
            ap[i0 + ih + j] = np.clip(vap[(k + j) % len(vap)] + breath * (1 - env * 0.5), 0, 1)
            kind[i0 + ih + j] = 2
        k += ih + iv
    sp2, ap2 = V3.extend_hf(sp, ap, kind)
    y = pw.synthesize(np.ascontiguousarray(f0), sp2, ap2, 2 * V3.TTS_SR, FP)
    y = bfilt(sg.resample_poly(y, 147, 160), 'highpass', 60, 2)
    return y / (np.abs(y).max() + 1e-9) * 0.8


def laugh_run(n, period, h, v, f_hi, f_lo, last=None, rise=1.03, fall=0.94, amp_decay=0.3):
    s, t = [], 0.0
    for i in range(n):
        q = i / max(1, n - 1)
        f = f_hi * (f_lo / f_hi) ** q
        s.append((t, h, v, f * rise, f * fall, 1 - amp_decay * q))
        t += period
    if last:
        s.append((t, h * 1.3, last, f_lo * 1.05, f_lo * 0.8, 0.9))
    return s


def varispeed(x, rate):
    """Tape-style: rate < 1 plays slower and lower (formants too), like a slowed record."""
    from scipy import signal as sg
    from fractions import Fraction
    fr = Fraction(rate).limit_denominator(200)
    return sg.resample_poly(x, fr.denominator, fr.numerator)


def oneshot_mix(y, bpm, male, hall=2.6, hall_wet=0.22):
    """A vocal one-shot: trimmed from just before its first sound, polished, a dark hall and a dotted-8th throw."""
    dry = V3.polish(y, male, sat=0.2)
    live = np.flatnonzero(np.abs(dry) > np.abs(dry).max() * 10 ** (-50 / 20))
    dry = dry[max(0, int(live[0]) - int(0.01 * SR)):].copy()
    fi = int(0.003 * SR)
    dry[:fi] *= np.linspace(0, 1, fi)
    dry = RV.tail_trim(dry, -65)
    ir = hall_ir(hall, 0.025, seed=11, dark=0.6)
    wet = np.zeros((len(dry) + len(ir), 2))
    wet[:len(dry)] += to_stereo(dry)
    addconv(wet, dry, ir, hall_wet)
    dl = V3.ducked_delay(dry, bpm, 0.75, 0.35, 0.16, duck_db=4.0)
    wet[:min(len(dl), len(wet))] += dl[:len(wet)]
    return V3.finish(RV.tail_trim(wet), -13.0), V3.finish(dry, -12.5)


LAUGHS = [
    # id, title, voice, vowel, run, varispeed, note
    ('hv21', 'Evil laugh', 'am_onyx', 'ha', laugh_run(8, 0.19, 0.05, 0.085, 270, 190, last=0.45), 0.76,
     'a laugh slowed like a record to 0.76x (deeper and bigger), a quiet octave-down shadow, long dark hall'),
    ('hv22', "Witch's cackle", 'af_kore', 'hee', laugh_run(11, 0.14, 0.04, 0.075, 700, 520, last=0.55, rise=1.05, fall=0.9), 1.0,
     'high, nasal cackle, falling across the run, long dark hall'),
]


def render_laughs(only=None):
    for sid, title, voice, word, run, rate, pn in LAUGHS:
        if only and sid not in only and 'laughs' not in only:
            continue
        t0 = time.time()
        pick = None
        for seed in range(3, 11):
            y = laugh_world(voice, word, run, seed=seed)
            heard = V2.asr(y, SR)
            log(f'    {sid} seed {seed} heard: {heard}')
            if pick is None:
                pick = (seed, y, heard)
            if 'laugh' in heard.lower():
                pick = (seed, y, heard)
                break
        seed, y, heard = pick
        male = voice in V3.MALE
        if rate != 1.0:
            y = varispeed(y, rate)
            y = y + 0.28 * board(y, [pb.PitchShift(semitones=-12)])
        wet, dry = oneshot_mix(y, 128, male, hall=3.0, hall_wet=0.26)
        R6.write_src(sid, wet, dry)
        put(entry(sid, 'adlibs', title, voice, 'laugh', 128, False, pn + '; no fixed pitch', wet, dry, heard))
        log(f'{sid} {voice} seed {seed} | {len(wet) / SR:.2f}s | {time.time() - t0:.0f}s | heard (before effects): {heard}')


# ================================================================= the witches' chant (Macbeth, Act IV scene 1; 1606, public domain)
CHANT_KIT = 'MTH01'      # A minor, 128 BPM: also fits MTH07, GH01, CH01 and CH08
CHANT = [   # low and close, like a chant: the long words ('toil', 'burn') on a dotted 8th, nothing above C5 so the vowels stay clear
    (0, 2, 'A4', 'Double'), (2, 2, 'A4', '~'), (4, 2, 'C5', 'double'), (6, 2, 'A4', '~'), (8, 3, 'C5', 'toil'), (11, 1, 'B4', 'and'),
    (12, 2, 'A4', 'trouble'), (14, 2, 'G4', '~'),
    (16, 2, 'A4', 'Fire'), (18, 2, 'A4', '~'), (20, 3, 'C5', 'burn'), (23, 1, 'B4', 'and'), (24, 2, 'A4', 'cauldron'), (26, 2, 'G4', '~'),
    (28, 2, 'E4', 'bubble'), (30, 2, 'E4', '~')]


def coven_mix(lead, doubles, low, bpm, loop_n, pre_n):
    """Lead in the middle, doubles wide, a man an octave below; a long dark hall and a dotted-8th echo; 2-bar loop."""
    L = V3.polish(lead, False)
    mix = to_stereo(L)
    for d, p in zip(doubles, (-0.75, 0.75)):
        dd = board(V3.polish(d, False, air=2.0, sat=0.15), [pb.HighpassFilter(170), pb.Compressor(threshold_db=-24, ratio=4, attack_ms=3, release_ms=80)])
        mix += to_stereo(dd * 10 ** (-7.5 / 20), p)
    lo = board(V3.polish(low, True, sat=0.2), [pb.HighpassFilter(90), pb.LowShelfFilter(200, 2.0)])
    mix += to_stereo(lo * 10 ** (-9.5 / 20), 0.1)
    mix = board(mix, [pb.Compressor(threshold_db=-16, ratio=2.2, attack_ms=10, release_ms=120)])
    n = len(mix)
    ir = hall_ir(3.4, 0.03, seed=5, dark=0.6)
    wet = np.zeros((n + len(ir), 2))
    wet[:n] += mix
    m = mix.mean(axis=1)
    addconv(wet, m, ir, 0.14)
    dl = V3.ducked_delay(m, bpm, 0.75, 0.32, 0.12)
    wet[:min(len(dl), len(wet))] += dl[:len(wet)]
    loop = V3.loop_with_pre(wet, loop_n, pre_n)
    dry = V3.loop_with_pre(V3.polish(lead, False, sat=0.1), loop_n, pre_n, tail=int(0.4 * SR))
    return V3.finish(loop, -13.0), V3.finish(dry, -13.5)


def render_chant(only=None):
    if only and 'chant' not in only and 'hv23' not in only:
        return
    t0 = time.time()
    kit = KITS[CHANT_KIT]
    bpm, sw = kit['bpm'], 0.5                          # chanted straight, not swung
    bad = R6.check_melody('hv23', CHANT_KIT, CHANT)
    if bad:
        log('  hv23 notes outside the chord:', bad)
    notes = [(s, l, N(n), w) for s, l, n, w in CHANT]
    n = int(2 * 16 * step_seconds(bpm) * SR)
    total = n / SR + 1.0
    best = None
    for v in ('bf_emma', 'af_nicole', 'af_heart', 'bf_lily'):
        log(f'  hv23 voice {v}')
        sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=13, log=log, pre=PRE, loop_sec=n / SR, vib=12.0)
        if best is None or sc > best[0] + 1e-6:
            best = (sc, v, y, tk, heard)
        if sc >= 0.95:
            break
    sc, voice, lead, tk, heard = best
    lead, dbl, _ = V3.hook_parts(notes, voice, bpm, sw, total, None, False, seed=13, take=tk, lead=lead, pre=PRE, loop_sec=n / SR, vib=12.0)
    low_notes = [(s, l, m - 12, w) for s, l, m, w in notes]
    low = V3.sing(low_notes, bpm, sw, V3.Sing('am_onyx', speed=tk['speed'], vib=10.0, timing=0.006, tuning=4, seed=29, breaths=False, fall=False,
                                               glottal=tk['glottal']), total, pre=PRE)
    wet, dry = coven_mix(lead, dbl, low, bpm, n, int(round(PRE * SR)))
    R6.write_src('hv23', wet, dry)
    put(entry('hv23', 'sung', 'Double, double, toil and trouble (witches\' chant)', f"coven ({DESC.get(voice, voice)} lead, two doubles, US male an octave below)",
              'chant', bpm, False, f"Macbeth (1606, public domain), chanted on A minor straight 8ths; written for {CHANT_KIT} {kit['name']}, 2-bar loop",
              wet, dry, heard, key=kit['key']['label'], camelot=kit['key']['camelot'], bars=2, kit=CHANT_KIT, short='Double double toil and trouble'))
    log(f"hv23 {voice} take {tk} score {sc:.2f} | wet {V3.k_loudness(wet):.1f} LUFS | {time.time() - t0:.0f}s | heard: {heard}")


WHISPER_TEXT = 'Double, double, toil and trouble. Fire burn, and cauldron bubble.'


def render_whisper(only=None):
    """The same couplet whispered by three voices at once (each take's length matched to the first), one an octave down."""
    if only and 'whisper' not in only and 'hv24' not in only:
        return
    voices = [('bf_lily', 0, 0.45), ('af_sky', 0, -0.45), ('bf_emma', -5, 0.0)]
    takes = []
    for v, _, _ in voices:
        pick = None
        for sp in (1.0, 0.94, 1.06, 0.9, 1.1):
            x = V.tts(WHISPER_TEXT, v, sp)
            heard = V2.asr(x, V3.TTS_SR)
            w = V2.words(heard)
            ok = w.count('double') == 2 and 'toil' in w and 'cauldron' in w
            sc = V2.score(WHISPER_TEXT, heard) + (0.5 if ok else 0)
            if pick is None or sc > pick[3]:
                pick = (x, sp, heard, sc)
            if ok and sc >= 1.3:
                break
        takes.append(pick)
        log(f'  hv24 {v} take {pick[1]} score {pick[3]:.2f} heard: {pick[2]}')
    ref = len(takes[0][0])
    parts = []
    for (v, pitch, pan), (x, sp, heard, sc) in zip(voices, takes):
        if abs(len(x) - ref) / ref > 0.04:        # re-speak at a speed that matches the first voice's length (if it still says it once)
            x2 = V.tts(WHISPER_TEXT, v, sp * len(x) / ref)
            w2 = V2.words(V2.asr(x2, V3.TTS_SR))
            if w2.count('double') == 2 and 'toil' in w2:
                x = x2
        x44 = bfilt(V.up(x), 'highpass', 90, 2)
        live = np.flatnonzero(np.abs(x44) > np.abs(x44).max() * 0.05)
        x44 = x44[max(0, live[0] - int(0.02 * SR)):]
        y = V2.clean(x44, False)
        if pitch:
            y = board(y, [pb.PitchShift(semitones=pitch)])
        y = board(y, [pb.HighShelfFilter(6500, 4.0)])
        parts.append((y, pan, 1.0 if not pitch else 0.8))
    n = max(len(p[0]) for p in parts)
    mix = np.zeros((n, 2))
    lead = np.zeros(n)
    for k, (y, pan, g) in enumerate(parts):
        mix[:len(y)] += to_stereo(y * g, pan)
        if k == 0:
            lead[:len(y)] += y
    mix = board(mix, [pb.Chorus(rate_hz=0.6, depth=0.2, mix=0.25, centre_delay_ms=9)])
    ir = hall_ir(3.6, 0.04, seed=21, dark=0.5)
    wet = np.zeros((n + len(ir), 2))
    wet[:n] += mix
    m = mix.mean(axis=1)
    addconv(wet, m, ir, 0.3)
    wet, dry = V3.finish(RV.tail_trim(wet), -14.0), V3.finish(RV.tail_trim(lead, -65), -13.5)
    R6.write_src('hv24', wet, dry)
    heard = takes[0][2]
    put(entry('hv24', 'sung', "Double, double, toil and trouble (whispered coven)", 'three whispers (UK female, US female, UK female pitched down)', 'whisper',
              128, False, 'Macbeth (1606, public domain), whispered by three voices at once, chorus + long hall; no fixed pitch', wet, dry, heard,
              short='Double double toil and trouble whispered'))
    log(f'hv24 whisper coven | {len(wet) / SR:.2f}s | heard: {heard}')


# ================================================================= original sung hooks (4-bar loops on a kit)
HOOKS = [
    # id, kit, candidate voices, explicit, harmony, title, [(step16, len16, note, word | '~')]
    ('hv25', 'MTH01', ['af_nicole', 'bf_lily', 'af_bella'], False, True, "Masks on, lights low, nobody's who they say", [
        (0, 3, 'E4', 'Masks'), (3, 3, 'G4', 'on'), (8, 3, 'A4', 'lights'), (11, 4, 'G4', 'low'),
        (16, 2, 'C5', "nobody's"), (18, 2, 'B4', '~'), (20, 2, 'A4', '~'), (22, 2, 'G4', 'who'), (24, 2, 'A4', 'they'), (26, 5, 'E4', 'say'),
        (32, 3, 'E4', 'Masks'), (35, 3, 'G4', 'on'), (40, 3, 'C5', 'lights'), (43, 4, 'B4', 'low'),
        (48, 2, 'D5', "nobody's"), (50, 2, 'C5', '~'), (52, 2, 'B4', '~'), (54, 2, 'A4', 'who'), (56, 2, 'G4', 'they'), (58, 5, 'A4', 'say')]),
    ('hv26', 'CH07', ['af_nova', 'af_heart', 'af_sarah'], False, True, 'Cobwebs on the disco ball, rattle your bones', [
        (0, 2, 'Bb4', 'Cobwebs'), (2, 2, 'Bb4', '~'), (4, 2, 'Ab4', 'on'), (6, 2, 'F4', 'the'), (8, 3, 'Ab4', 'disco'), (11, 1, 'F4', '~'), (12, 4, 'Eb4', 'ball'),
        (17, 1, 'Db4', 'rattle'), (18, 2, 'F4', '~'), (20, 2, 'Ab4', 'your'), (22, 8, 'Bb4', 'bones'),
        (32, 2, 'Db5', 'Cobwebs'), (34, 2, 'Db5', '~'), (36, 2, 'Bb4', 'on'), (38, 2, 'Gb4', 'the'), (40, 3, 'Bb4', 'disco'), (43, 1, 'Gb4', '~'), (44, 4, 'F4', 'ball'),
        (49, 1, 'Eb4', 'rattle'), (50, 2, 'F4', '~'), (52, 2, 'Ab4', 'your'), (54, 8, 'C5', 'bones')]),
    ('hv27', 'UKG02', ['af_bella', 'af_kore', 'af_sarah'], True, True, 'Little bit wicked, little bit fucking sweet', [
        (0, 2, 'D5', 'Little'), (2, 2, 'C5', '~'), (4, 2, 'A4', 'bit'), (6, 2, 'C5', 'wicked'), (8, 4, 'D5', '~'),
        (14, 2, 'A4', 'little'), (16, 2, 'Bb4', '~'), (18, 2, 'A4', 'bit'), (20, 2, 'F4', 'fucking'), (22, 2, 'A4', '~'), (24, 6, 'D5', 'sweet'),
        (32, 2, 'D5', 'Little'), (34, 2, 'C5', '~'), (36, 2, 'Bb4', 'bit'), (38, 2, 'D5', 'wicked'), (40, 4, 'F5', '~'),
        (46, 2, 'E5', 'little'), (48, 2, 'C#5', '~'), (50, 2, 'E5', 'bit'), (52, 2, 'G4', 'fucking'), (54, 2, 'A4', '~'), (56, 6, 'A4', 'sweet')]),
]


def render_hooks(only=None):
    for sid, kid, voices, explicit, harm, title, mel in HOOKS:
        if only and sid not in only and 'hooks' not in only:
            continue
        t0 = time.time()
        kit = KITS[kid]
        bad = R6.check_melody(sid, kid, mel)
        if bad:
            log(f'  {sid} melody notes outside the chord: {bad}')
        bpm, sw = kit['bpm'], kit['swing']
        notes = [(s, l, N(n), w) for s, l, n, w in mel]
        n = int(4 * 16 * step_seconds(bpm) * SR)
        total = n / SR + 1.0
        best = None
        for v in voices:
            log(f'  {sid} voice {v}')
            sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=11, log=log, pre=PRE, loop_sec=n / SR)
            if best is None or sc > best[0] + 1e-6:
                best = (sc, v, y, tk, heard)
            if sc >= 0.95:
                break
        sc, voice, lead, tk, heard = best
        scale = RV.SCALE(kit['root'], kit['mode'])
        hn = V3.harmony_chords(notes, V3.chord_timeline(kit), scale, ceiling=72 if voice in V3.MALE else 81) if harm else None
        lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, harm, seed=11, take=tk, lead=lead, pre=PRE, loop_sec=n / SR, harm_notes=hn)
        wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
        R6.write_src(sid, wet, dry)
        parts = 'sung lead + 2 doubles' + (' + harmony on the chords' if harm else '')
        put(entry(sid, 'sung', title, voice, 'sung', bpm, explicit, f"{parts}, written for {kid} {kit['name']}, 4-bar loop", wet, dry, heard,
                  key=kit['key']['label'], camelot=kit['key']['camelot'], bars=4, kit=kid))
        log(f"{sid} {voice} take {tk} score {sc:.2f} | wet {V3.k_loudness(wet):.1f} LUFS dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s | heard: {heard}")


# ================================================================= Murder at Cameron Castle: drops for the user's murder-mystery party
CASTLE_SPOKEN = [
    # id, text, voice, style, bpm, pitch, explicit
    ('mc01', 'Welcome to Cameron Castle.', 'bm_daniel', 'preacher', 128, 0, False),
    ('mc02', 'Murder at Cameron Castle.', 'am_onyx', 'dark', 128, -2, False),
    ('mc03', 'Someone in this room is a killer.', 'af_sky', 'whisper', 128, 0, False),
    ('mc04', 'Nobody leaves the castle until the killer is found.', 'af_kore', 'dark', 128, -1, False),
    ('mc05', "The butler didn't do it.", 'bm_fable', 'diva', 128, 0, False),
    ('mc06', 'Who killed the DJ?', 'bm_lewis', 'mc', 132, 0, False),
    ('mc07', 'Dressed to kill. Literally.', 'bf_isabella', 'diva', 128, 0, False),
    ('mc08', 'Live from Cameron Castle, where the guests are dying to dance.', 'bm_daniel', 'radio', 132, 0, False),
    ('mc09', "Somebody's fucking dead, and the champagne's still flowing.", 'bf_isabella', 'diva', 128, 0, True),
    ('mc10', 'Every fucking guest is a suspect.', 'bm_george', 'mc', 132, 0, True),
    ('mc11', "Darling, I'd kill for this fucking song.", 'bf_emma', 'diva', 128, 0, True),
]
CASTLE_ADLIBS = [
    ('mc12', 'Murder!', 'bf_isabella', 128, False),
    ('mc13', 'Whodunit?', 'bf_emma', 128, False),
    ('mc14', 'Cameron Castle!', 'bm_george', 132, False),
    ('mc15', "Darling, you're next.", 'bf_isabella', 128, False),
    ('mc16', 'Cameron Castle, motherfuckers!', 'bm_lewis', 132, True),
]
CASTLE_HOOK = ('mc17', 'GH01', ['bf_emma', 'af_heart', 'af_bella'], False, True, "Murder at Cameron Castle, everybody's a suspect", [
    (0, 2, 'E5', 'Murder'), (2, 2, 'D5', '~'), (4, 2, 'C5', 'at'), (6, 3, 'E5', 'Cameron'), (9, 1, 'D5', '~'), (10, 2, 'C5', '~'),
    (12, 3, 'B4', 'Castle'), (15, 5, 'A4', '~'),
    (32, 2, 'D5', "everybody's"), (34, 2, 'B4', '~'), (36, 2, 'A4', '~'), (38, 2, 'G4', '~'), (40, 2, 'A4', 'a'), (42, 3, 'B4', 'suspect'),
    (45, 8, 'G4', '~')])


SUPPER_SPOKEN = [
    ('mc18', "Welcome back to Old Money. Tonight, we're live from Cameron Castle.", 'am_fenrir', 'diva', 128, 0, False),
    ('mc19', 'The Keyhole sees everything.', 'af_nicole', 'whisper', 128, 0, False),
    ('mc20', "Tonight's Keyhole exclusive: everybody's lying.", 'bf_isabella', 'diva', 128, 0, False),
    ('mc21', 'The spirits are restless tonight.', 'am_onyx', 'dark', 128, -2, False),
    ('mc22', 'The Midnight Supper is served.', 'bf_emma', 'preacher', 128, 0, False),
    ('mc23', 'House rules: no running, no snooping, no murder.', 'bf_lily', 'diva', 128, 0, False),
    ('mc24', "Smile for the camera. You're all evidence now.", 'am_michael', 'mc', 132, 0, False),
    ('mc25', 'Every family has secrets. This one has a whole library.', 'bf_lily', 'whisper', 128, 0, False),
    ('mc26', "Everybody wants to know who's in the will.", 'bf_isabella', 'dark', 128, -1, False),
    ('mc27', 'Heirs, spares and liars.', 'bm_george', 'mc', 132, 0, False),
    ('mc28', "Don't touch the art.", 'bf_isabella', 'diva', 128, 0, False),
    ('mc29', "The car's out front, but nobody's leaving.", 'bm_daniel', 'dark', 128, -1, False),
    ('mc30', 'Is anybody there? Knock once for yes.', 'am_michael', 'whisper', 128, 0, False),
    ('mc31', 'Who the fuck invited the medium?', 'bf_isabella', 'diva', 128, 0, True),
    ('mc32', 'Old money, fresh fucking scandal.', 'af_bella', 'dark', 130, -1, True),
    ('mc33', 'Everybody here is lying, and I fucking love it.', 'bf_emma', 'diva', 128, 0, True),
]
SUPPER_ADLIBS = [
    ('mc34', 'En garde!', 'bm_george', 132, False),
    ('mc35', 'Say cheese!', 'am_michael', 128, False),
    ('mc36', 'Happy birthday, Douglas!', 'af_sarah', 128, False),
    ('mc37', 'Scandalous!', 'bf_isabella', 128, False),
]
SAY = {'mc34': 'On guard!'}      # what the voice is given when the title's spelling would trip it up (en garde, said the English way)
SUPPER_HOOK = ('mc38', 'UKG02', ['af_heart', 'bf_emma', 'af_nova'], False, True, "Midnight Supper, every secret's on the menu", [
    (0, 3, 'A4', 'Midnight'), (3, 3, 'C5', '~'), (6, 2, 'D5', 'Supper'), (8, 4, 'A4', '~'),
    (14, 2, 'F4', 'every'), (16, 1, 'G4', '~'), (17, 2, 'A4', "secret's"), (19, 1, 'G4', '~'), (20, 2, 'F4', 'on'), (22, 2, 'G4', 'the'), (24, 3, 'A4', 'menu'), (27, 4, 'D4', '~'),
    (32, 3, 'Bb4', 'Midnight'), (35, 3, 'D5', '~'), (38, 2, 'F5', 'Supper'), (40, 4, 'D5', '~'),
    (46, 2, 'E5', 'every'), (48, 1, 'C#5', '~'), (49, 2, 'E5', "secret's"), (51, 1, 'D5', '~'), (52, 2, 'C#5', 'on'), (54, 2, 'Bb4', 'the'), (56, 3, 'A4', 'menu'), (59, 4, 'A4', '~')])


# Zebedee Spookington (the medium) and Flash McGillicuddy (the photographer) of the Midnight Supper: public roles only
ZF_SPOKEN = [
    ('mc39', 'Silence, please. Zebedee Spookington is reaching the other side.', 'af_nicole', 'whisper', 128, 0, False),
    ('mc40', 'Make some noise for Zebedee Spookington!', 'bm_george', 'mc', 132, 0, False),
    ('mc41', 'Zebedee? Are you there?', 'af_sky', 'whisper', 128, 0, False),
    ('mc43', 'Flash McGillicuddy, get a picture of this!', 'bm_lewis', 'mc', 132, 0, False),
    ('mc44', 'Not now, Flash! No photos!', 'bf_isabella', 'diva', 128, 0, False),
]
ZF_ADLIBS = [('mc45', 'Flash! Flash! Flash!', 'af_sarah', 128, False)]
ZF_CHOPS = [
    # id, kit, voices, title, notes (2 bars, A minor)
    ('mc42', 'MTH01', ['bm_george', 'bf_emma'], 'Zebedee, Zebedee, Zebedee Spookington (sung chop)', [     # a summoning chant (written at female pitch)
        (0, 2, 'A4', 'Zebedee'), (2, 1, 'C5', '~'), (3, 3, 'A4', '~'), (8, 2, 'A4', 'Zebedee'), (10, 1, 'C5', '~'), (11, 3, 'E5', '~'),
        (16, 2, 'D5', 'Zebedee'), (18, 1, 'C5', '~'), (19, 2, 'A4', '~'), (21, 2, 'C5', 'Spookington'), (23, 2, 'B4', '~'), (25, 5, 'A4', '~')]),
    ('mc46', 'GH01', ['af_bella', 'af_kore', 'bf_emma'], 'Flash, Flash, Flash McGillicuddy (sung chop)', [
        (0, 2, 'A4', 'Flash'), (3, 2, 'A4', 'Flash'), (6, 2, 'C5', 'Flash'), (8, 1, 'E5', 'McGillicuddy'), (9, 2, 'D5', '~'), (11, 1, 'C5', '~'),
        (12, 2, 'A4', '~'), (14, 2, 'C5', '~'),
        (16, 2, 'A4', 'Flash'), (19, 2, 'A4', 'Flash'), (22, 2, 'G4', 'Flash'), (24, 1, 'E5', 'McGillicuddy'), (25, 2, 'D5', '~'), (27, 1, 'C5', '~'),
        (28, 2, 'D5', '~'), (30, 2, 'D5', '~')]),
]


def fuzzy(want, heard):
    import difflib, re
    L = lambda t: re.sub('[^a-z]', '', t.lower())
    return difflib.SequenceMatcher(None, L(want), L(heard)).ratio()


def blend_of(v, tint):
    return V3.blend_of(v, V3.FEMALE_ALTS.get(v, [v])[0], tint) if tint else v


def render_zf(only=None):
    want = lambda sid: not only or sid in only or 'zf' in only
    render_drops(ZF_SPOKEN, ZF_ADLIBS, None, 'zf', want)
    for sid, kid, voices, title, mel in ZF_CHOPS:
        if not want(sid):
            continue
        t0 = time.time()
        kit = KITS[kid]
        bad = R6.check_melody(sid, kid, mel)
        if bad:
            log(f'  {sid} notes outside the chord: {bad}')
        bpm, sw = kit['bpm'], kit['swing']
        n = int(2 * 16 * step_seconds(bpm) * SR)
        total = n / SR + 1.0
        want_text = ' '.join(w for *_, w in mel if w != '~')
        best = None
        for v in voices:          # names aren't dictionary words, so takes are judged on spelling closeness, not exact words
            male = v in V3.MALE
            notes = [(s_, l, N(nn) - (12 if male else 0), w) for s_, l, nn, w in mel]
            for cons in (1.0, 1.3, 1.6):        # longer consonants keep the name's 'z' and 'b' through the echo
                for tk in V3.TAKES[:4]:
                    S = V3.Sing(v, seed=17, vib=8.0, consonants=cons, **tk)
                    y = V3.sing(notes, bpm, sw, S, total, pre=PRE, loop_sec=n / SR)
                    pad = np.zeros(int(0.3 * SR))
                    h_dry = V2.asr(np.concatenate([pad, V3.polish(y, male), pad]), SR)
                    w_, _ = R6.chop_mix(y, bpm, n, int(round(PRE * SR)), male)
                    h_wet = V2.asr(np.concatenate([w_.mean(axis=1)] * 2), SR)
                    sc = 0.4 * fuzzy(want_text, h_dry) + 0.6 * fuzzy(want_text + ' ' + want_text, h_wet)
                    log(f'    {v} cons {cons} {tk} {sc:.2f} dry: {h_dry} | wet: {h_wet}')
                    if best is None or sc > best[0] + 1e-6:
                        best = (sc, v, y, dict(tk, consonants=cons), h_dry + ' | wet: ' + h_wet)
                if best[0] >= 0.85:
                    break
            if best[0] >= 0.85:
                break
        sc, voice, lead, tk, heard = best
        wet, dry = R6.chop_mix(lead, bpm, n, int(round(PRE * SR)), voice in V3.MALE)
        R6.write_src(sid, wet, dry)
        put(entry(sid, 'zf', title, voice, 'chop', bpm, False, f"the name sung as a 2-bar chop loop, swung like {kid} {kit['name']}, plate + throw", wet, dry, heard,
                  key=kit['key']['label'], camelot=kit['key']['camelot'], bars=2, kit=kid))
        log(f"{sid} {voice} take {tk} score {sc:.2f} | {time.time() - t0:.0f}s | heard: {heard}")


# ================================================================= big dramatic stabs: every character announced, and comic drama
# A booming announcer over an orchestral hit (low brass, timpani, choir and cymbal on A minor), a hall and a quarter-note throw.
NAMES = [   # title, what the voice is given when the spelling trips it up
    ('Beatrix Blackthorn', None), ('Bingo Bangcroft', 'Bingo Bang-croft!'), ('Cassia Gossamer', 'Cass-ee-uh Gossamer!'), ('Cordelia Crookshank', None),
    ('Countess Valentina Fizzwick', None), ('Dr. Henrietta Hollowell', 'Doctor Henrietta Hollowell!'), ('Dorothea Dustweather', None),
    ('Douglas Cameron', None), ('Edgar Wainscot', None), ('Edmund Easelby', 'Edmund Easel-by!'), ('Evangeline Page', None),
    ('Ezra Vaultbridge', 'Ezra, Vaultbridge!'), ('Felix Moonshadow', None), ('Flash McGillicuddy', None), ('Florentine Fairweather', 'Florentine Fair-weather!'),
    ('Hamish Cameron', None), ('Imogen Vaultbridge', None), ('Ingrid Pricewell', None), ('Isadora Brushwood', None),
    ('Maestro Gideon Pricewell', None), ('Mimi Masterpiece', None), ('Mirabelle Moonshadow', 'Mira-belle Moonshadow!'), ('Poppy Puddingham', 'Poppy Puddingum!'),
    ('Professor Percival Pennyfeather', None), ('Scarlett Starlight', None), ('Sebastian Snodgrass', None), ('Seraphina Cameron', None),
    ('Tallulah Twinkleton', None), ('Thaddeus Hollowell', None), ('Tilly Tinctures', None), ('Winifred Crane', 'Winnifred Crane!'), ('Zebedee Spookington', None)]
NAME_STABS = [(f'mc{47 + k}', t, say or t + '!', 'bm_george', -2, False) for k, (t, say) in enumerate(NAMES)]
DRAMA_STABS = [
    # id, title, what the voice is given, voice, pitch, explicit
    ('mc79', 'I accuse you!', 'I accuse... you!', 'bm_george', -2, False),
    ('mc80', 'It was you!', 'It was... you!', 'bm_george', -2, False),
    ('mc81', 'Nobody move!', 'Nobody move!', 'bm_george', -2, False),
    ('mc82', 'Plot twist!', 'Plot twist!', 'bm_george', 0, False),
    ('mc83', 'Objection!', 'Objection!', 'bm_george', -2, False),
    ('mc84', 'Liar!', 'Liar!', 'am_michael', -2, False),
    ('mc85', 'How dare you!', 'How dare you!', 'bf_isabella', 0, False),
    ('mc86', 'Scandal!', 'Scandal!', 'bf_isabella', 0, False),
    ('mc87', 'Motive!', 'Motive!', 'bm_george', -2, False),
    ('mc88', 'Alibi!', 'Alibi!', 'am_michael', -2, False),
    ('mc89', 'It was the butler!', 'It was the butler!', 'bm_george', -2, False),
    ('mc90', 'Not at the dinner table!', 'Not at the dinner table!', 'bf_isabella', 0, False),
    ('mc91', 'Somebody call the police!', 'Somebody call the police!', 'bf_isabella', 0, False),
    ('mc92', 'Dun, dun, DUN!', 'Dunn... dunn... DUNN!', 'bm_george', -2, False),
    ('mc93', 'MURDER!', 'Murder!', 'bm_george', 0, False),
    ('mc94', 'Darling, no!', 'Darling, no!', 'bf_isabella', 0, False),
    ('mc95', 'Oh my fucking God!', 'Oh my fucking God!', 'bf_isabella', 0, True),
    ('mc96', 'You lying bastard!', 'You lying bastard!', 'bm_george', -2, True),
    ('mc97', "There's been a murder!", "There's been a murder!", 'bm_george', -2, False),
]
_HIT = []


def trailer_hit():
    """One orchestral hit on A minor, made once: low brass, timpani, a choir on 'ah', a cymbal. Stereo, peak 1."""
    if _HIT:
        return _HIT[0]
    import render_halloween_fx as FXM
    rng = np.random.default_rng(71)
    n = int(2.6 * SR)
    t = np.arange(n) / SR
    brass = FXM.brass([55.0, 82.41, 110.0, 130.81, 164.81], t, tau_f=0.35, tau_a=0.8, cut_hi=3000.0, cut_lo=600.0)
    timp = FXM.timp(55.0, t, rng)
    from dsp import saw
    ch = sum(saw(np.full(n, f * 2 ** (d / 1200))) for f in (220.0, 261.63, 329.63, 440.0) for d in (-7, 6))
    choir = FXM.formant_bank(ch, [(730, 90, 1.0), (1090, 110, 0.5), (2440, 160, 0.15)]) * np.clip(t / 0.012, 0, 1) * np.exp(-t / 0.7)
    cym = bfilt(rng.standard_normal(n), 'highpass', 5000, 2) * np.exp(-t / 1.1) * np.clip(t / 0.003, 0, 1)
    pk = lambda x: x / (np.abs(x).max() + 1e-9)
    out = FXM.pan2(pk(brass) * 1.0 + pk(timp) * 0.9, 0.0) / np.sqrt(2) + FXM.pan2(pk(choir) * 0.45, -0.3) / np.sqrt(2) + FXM.pan2(pk(cym) * 0.22, 0.35) / np.sqrt(2)
    out = bfilt(out, 'lowpass', 6000, 2)
    _HIT.append(out / np.abs(out).max())
    return _HIT[0]


HIT_LAST = {'mc79', 'mc80', 'mc92'}     # the hit lands on the last word ("...YOU!", "...DUN!") instead of the first


def last_onset(y, gap=0.07):
    """Sample index where the last word starts: the last rise of the level after a pause of at least `gap` seconds."""
    w = int(0.01 * SR)
    env = np.sqrt(np.convolve(y ** 2, np.ones(w) / w, 'same'))
    on = env > env.max() * 10 ** (-28 / 20)
    idx = np.flatnonzero(on)
    if not len(idx):
        return 0
    breaks = np.flatnonzero(np.diff(idx) > gap * SR)
    return int(idx[breaks[-1] + 1]) if len(breaks) else int(idx[0])


def stab_mix(x44, male, pitch, bpm=128, hit_at='first'):
    """Announcer chain: pitched down for weight, cleaned, compressed and driven; the orchestral hit on its first syllable; a hall
    and a quarter-note throw. Tails stop at -50 dB, so the stabs stay short. Returns (wet stereo, dry mono)."""
    y = board(x44, [pb.PitchShift(semitones=pitch)]) if pitch else x44
    y = V2.clean(y, male)
    live = np.flatnonzero(np.abs(y) > np.abs(y).max() * 10 ** (-45 / 20))
    y = y[max(0, live[0] - int(0.008 * SR)):].copy()
    y[:int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))
    y = RV.tail_trim(y, -60)
    dry = V3.finish(y, -12.5)
    v = board(y, [pb.Compressor(threshold_db=-24, ratio=4.0, attack_ms=2, release_ms=110)])
    v = V.parallel_dist(v, 8, 0.25)
    v = v / (np.abs(v).max() + 1e-9)
    hit = trailer_hit()
    h0 = max(0, last_onset(v) - int(0.01 * SR)) if hit_at == 'last' else 0
    n = len(v) + int(2.6 * SR)
    wet = np.zeros((n, 2))
    wet[:len(v)] += to_stereo(v)
    k = min(len(hit), n - h0)
    wet[h0:h0 + k] += hit[:k] * 0.42
    addconv(wet, v, hall_ir(3.0, 0.03, seed=73, dark=0.55), 0.17)
    dl = V3.ducked_delay(v, bpm, 1.0, 0.15, 0.07, duck_db=8.0)     # one faint quarter-note throw: the hall and the hit do the work
    wet[:min(len(dl), n)] += dl[:n]
    wet = RV.tail_trim(wet, -50)
    return V3.finish(wet, -12.0), dry


def best_take_fuzzy(say, voice, title, speeds=(1.0, 0.92, 1.08)):
    """Names aren't dictionary words: keep the take whose transcript is spelled closest to the title."""
    import vocals as V_
    best = None
    for sp in speeds:
        x = V_.tts(say, voice, sp)
        heard = V2.asr(x, V3.TTS_SR)
        f = fuzzy(title, heard) + (0.02 if sp == 1.0 else 0)
        if best is None or f > best[0]:
            best = (f, x, sp, heard)
    return best


def render_stabs(only=None):
    want = lambda sid, sec: not only or sid in only or sec in only or 'stabs' in only
    for table, sub, sec in ((NAME_STABS, 'names', 'names'), (DRAMA_STABS, 'drama', 'drama')):
        for sid, title, say, voice, pitch, explicit in table:
            if not want(sid, sec):
                continue
            sc, x, sp, heard = best_take_fuzzy(say, voice, title)
            male = voice in V3.MALE
            wet, dry = stab_mix(bfilt(V.up(x), 'highpass', 70, 2), male, pitch, hit_at='last' if sid in HIT_LAST else 'first')
            R6.write_src(sid, wet, dry)
            heard_wet = V2.asr(wet.mean(axis=1), SR)
            heard_dry = V2.asr(dry, SR)
            f = max(fuzzy(title, heard_wet), fuzzy(title, heard_dry))
            note = ('the name announced' if sub == 'names' else 'announced') + (' with an orchestral hit on the last word' if sid in HIT_LAST else ' over an orchestral hit') + \
                f" on A minor, hall + throw{', pitched ' + str(pitch) + ' st' if pitch else ''}"
            put(entry(sid, sub, title, voice, 'stab', 128, explicit, note, wet, dry, heard, key='A minor', camelot='8A'))
            log(f'{sid} {title!r} {voice} take {sp} | {len(wet) / SR:.2f}s | closeness {f:.2f} | dry: {heard_dry} | wet: {heard_wet}')


def render_castle(only=None):
    render_drops(CASTLE_SPOKEN, CASTLE_ADLIBS, CASTLE_HOOK, 'castle', lambda sid: not only or sid in only or 'castle' in only)


def render_supper(only=None):
    render_drops(SUPPER_SPOKEN, SUPPER_ADLIBS, SUPPER_HOOK, 'supper', lambda sid: not only or sid in only or 'supper' in only)


def render_drops(spoken, adlibs, hook, sub, want):
    for sid, text, voice, style, bpm, pitch, explicit in spoken:
        if not want(sid):
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        male = voice in V3.MALE
        dry = board(x44, [pb.PitchShift(semitones=pitch)]) if (style == 'dark' and pitch) else x44
        wet = V2.spoken_chain(x44, style, bpm, pitch, male)
        if style == 'dark':
            wet = haunt(wet, bpm, male)
        wet, dry = RV.tail_trim(wet), RV.tail_trim(V2.clean(dry, male), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -13.0)
        note = (f'pitched {pitch} st, grit + delay, long dark hall' if style == 'dark' else STYLE_NOTE[style]) + f' · best of 3 takes ({sp:.2f}x)'
        R6.write_src(sid, wet, dry)
        put(entry(sid, sub, text, voice, style, bpm, explicit, note, wet, dry, heard))
        log(f'{sid} {voice} {style} score {sc:.2f} heard: {heard}')
    for sid, text, voice, bpm, explicit in adlibs:
        if not want(sid):
            continue
        sc, x, sp, heard = V2.best_take(SAY.get(sid, text), voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        male = voice in V3.MALE
        wet = RV.tail_trim(V2.spoken_chain(x44, 'adlib', bpm, 0, male))
        dry = RV.tail_trim(V2.clean(x44, male), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -12.5)
        R6.write_src(sid, wet, dry)
        put(entry(sid, sub, text, voice, 'adlib', bpm, explicit, 'ad-lib, short echo', wet, dry, heard))
        log(f'{sid} {voice} adlib score {sc:.2f} heard: {heard}')
    if hook is None:
        return
    sid, kid, voices, explicit, harm, title, mel = hook
    if not want(sid):
        return
    t0 = time.time()
    kit = KITS[kid]
    bad = R6.check_melody(sid, kid, mel)
    if bad:
        log(f'  {sid} melody notes outside the chord: {bad}')
    bpm, sw = kit['bpm'], kit['swing']
    notes = [(s, l, N(n), w) for s, l, n, w in mel]
    n = int(4 * 16 * step_seconds(bpm) * SR)
    total = n / SR + 1.0
    best = None
    for v in voices:
        log(f'  {sid} voice {v}')
        sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=11, log=log, pre=PRE, loop_sec=n / SR)
        if best is None or sc > best[0] + 1e-6:
            best = (sc, v, y, tk, heard)
        if sc >= 0.95:
            break
    sc, voice, lead, tk, heard = best
    scale = RV.SCALE(kit['root'], kit['mode'])
    hn = V3.harmony_chords(notes, V3.chord_timeline(kit), scale, ceiling=81) if harm else None
    lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, harm, seed=11, take=tk, lead=lead, pre=PRE, loop_sec=n / SR, harm_notes=hn)
    wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
    R6.write_src(sid, wet, dry)
    put(entry(sid, sub, title, voice, 'sung', bpm, explicit, f"sung lead + 2 doubles + harmony on the chords, written for {kid} {kit['name']}, 4-bar loop",
              wet, dry, heard, key=kit['key']['label'], camelot=kit['key']['camelot'], bars=4, kit=kid))
    log(f"{sid} {voice} take {tk} score {sc:.2f} | {time.time() - t0:.0f}s | heard: {heard}")


def improve(ids, current, margin=0.03):
    """Wider take search (consonant length, three speeds, glottal onsets, every candidate voice); a hook is replaced only if
    its new lead is understood clearly better than the current one."""
    for sid, kid, voices, explicit, harm, title, mel in HOOKS:
        if sid not in ids:
            continue
        t0 = time.time()
        kit = KITS[kid]
        bpm, sw = kit['bpm'], kit['swing']
        notes = [(s, l, N(n), w) for s, l, n, w in mel]
        n = int(4 * 16 * step_seconds(bpm) * SR)
        total = n / SR + 1.0
        best = None
        for v in voices:
            log(f'  {sid} voice {v} (wide search; current {current[sid]:.2f})')
            sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=11, log=log, pre=PRE, loop_sec=n / SR, takes=V3.WIDE_TAKES, need=0.97, retry=0.9)
            if best is None or sc > best[0] + 0.02:
                best = (sc, v, y, tk, heard)
            if sc >= 0.97:
                break
        sc, voice, lead, tk, heard = best
        if sc < current[sid] + margin:
            log(f'{sid} kept (best new take {sc:.2f} vs {current[sid]:.2f}) | {time.time() - t0:.0f}s')
            continue
        scale = RV.SCALE(kit['root'], kit['mode'])
        hn = V3.harmony_chords(notes, V3.chord_timeline(kit), scale, ceiling=72 if voice in V3.MALE else 81) if harm else None
        lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, harm, seed=11, take=tk, lead=lead, pre=PRE, loop_sec=n / SR, harm_notes=hn)
        wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
        R6.write_src(sid, wet, dry)
        parts = 'sung lead + 2 doubles' + (' + harmony on the chords' if harm else '')
        put(entry(sid, 'sung', title, voice, 'sung', bpm, explicit, f"{parts}, written for {kid} {kit['name']}, 4-bar loop", wet, dry, heard,
                  key=kit['key']['label'], camelot=kit['key']['camelot'], bars=4, kit=kid))
        log(f"{sid} {voice} take {tk} score {sc:.2f} (was {current[sid]:.2f}) | {time.time() - t0:.0f}s | heard: {heard}")


if __name__ == '__main__':
    if sys.argv[1:2] == ['improve']:
        improve(set(sys.argv[2:]), {'hv25': 0.77, 'hv26': 0.84})
        open(f'{W}/out/render_halloween.log', 'a').write('\n'.join(LOG) + '\n')
        sys.exit(0)
    only = sys.argv[1:] or None
    t0 = time.time()
    sec = lambda name, table: not only or name in only or any(o in {r[0] for r in table} for o in only)
    if sec('spoken', SPOKEN):
        render_spoken(only)
    if sec('adlibs', ADLIBS):
        render_adlibs(only)
    if sec('laughs', LAUGHS):
        render_laughs(only)
    render_chant(only)
    render_whisper(only)
    if sec('hooks', HOOKS):
        render_hooks(only)
    if not only or any(o.startswith('castle') or o.startswith('mc') for o in only):
        render_castle(only)
    if not only or any(o == 'supper' or o.startswith('mc') for o in only):
        render_supper(only)
    if not only or any(o == 'zf' or o.startswith('mc') for o in only):
        render_zf(only)
    if not only or any(o in ('stabs', 'names', 'drama') or o.startswith('mc') for o in only):
        render_stabs(only)
    open(f'{W}/out/render_halloween.log', 'a').write('\n'.join(LOG) + '\n')
    print('done in', round(time.time() - t0), 's')
