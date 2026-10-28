"""Speed garage + classic house expansion: hits, Reese basses, organ bass, house piano, strings, loops, FX."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, re, time
import numpy as np
from dsp import *
import hits as H
import fx as F
import reese as R
import keys as Kz

SITE = PACK_ROOT + '/site/audio'
K = json.load(open(PACK_ROOT + '/out/kits.json'))
OUT = []
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
PCN = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}


def nname(m, flats=True):
    sh = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    return (NAMES if flats else sh)[m % 12] + str(m // 12 - 1)


def save(cat, sid, title, x, bpm=None, key=None, cam=None, bars=None, note=None, pack=None, genre=None, sset=None, noteName=None, peak=-1.0, hidden=False):
    os.makedirs(f'{SITE}/{cat}', exist_ok=True)
    y = write(f'{SITE}/{cat}/{sid}.wav', x, peak)
    e = {'id': sid, 'cat': cat, 'title': title, 'bpm': bpm, 'key': key, 'camelot': cam, 'bars': bars, 'note': note,
         'dur': round(len(y) / SR, 3), 'file': f'audio/{cat}/{sid}.wav', 'pack': pack + '.wav' if pack else None,
         'peaks': peaks(y, 72), 'stereo': y.ndim == 2, 'genre': genre}
    if sset:
        e['set'] = sset; e['noteName'] = noteName
    if hidden:
        e['hidden'] = True; e['pack'] = None
    OUT.append({k: v for k, v in e.items() if v is not None})
    print(f'  {sid:24s} {len(y)/SR:6.2f}s {bpm or "":>4} {key or "":10s} {title}', flush=True)


def kit_notes(kid, part, bars=None):
    bpm = K[kid]['bpm']
    sec = lambda t: t / 480 * 60 / bpm
    ev = K[kid]['parts'][part]
    if bars:
        ev = [e for e in ev if e[0] < bars * 1920]
    return [(sec(t), sec(d), n, v, bool(l), ch) for t, d, n, v, l, ch in ev]


def kkey(kid):
    k = K[kid]['key']
    return k['label'], k['camelot']


def root_of(chord):
    m = re.match(r'^([A-G])(#|b)?', chord)
    return (PCN[m.group(1)] + (1 if m.group(2) == '#' else -1 if m.group(2) == 'b' else 0)) % 12


def main():
    t0 = time.time()
    # ---------------- hits ----------------
    print('hits')
    for name, fn, label, grp, key in H.ONE_SHOTS_MORE:
        kname = label.replace(' ', '-') + (f'_{key}' if key else '')
        save('hits', name, label, normalize(fn(), -1.0), key=key, note=('tuned to ' + key) if key else None,
             pack=f'Drum One-Shots/Hit_{kname}', genre={'sg': 'sg', 'house': 'ch'}.get(grp))

    # ---------------- Reese ----------------
    print('reese one-shots')
    for m in range(24, 36):
        nm = nname(m)
        save('bass', f'reese_{nm.replace("#", "s")}', f'Reese Classic {nm}', R.one_shot(m, 'classic'), key=nm[:-1],
             note='chromatic one-shot, 2 s', pack=f'Speed Garage/Reese Bass/One-Shots Classic/SG_Reese-Classic_{nm}', genre='sg',
             sset='reese_classic', noteName=nm)
    chars = [('reese_wobble', 'Reese Wobble', 31, 'wobble', None, 'G', '1/8 filter wobble at 132 BPM', 132),
             ('reese_growl', 'Reese Growl', 29, 'growl', None, 'F', 'vowel growl, 1/4 sweep at 132 BPM', 132),
             ('reese_dark', 'Reese Dark', 28, 'dark', None, 'E', 'low-passed, sub-heavy', 132),
             ('reese_dive', 'Reese Pitch Dive', 33, 'classic', (0.55, -12, 0.9), 'A', 'drops an octave (speed garage drop)', 132),
             ('reese_stab', 'Reese Stab', 30, 'stab', None, 'F#', 'short, plucky filter', 132)]
    for sid, title, m, patch, dive, keyn, note, bpm in chars:
        hold = 0.35 if patch == 'stab' else 1.75
        x = R.one_shot(m, patch, 2.0 if patch != 'stab' else 0.8, hold, bpm, dive)
        save('bass', sid, title, x, bpm=bpm if patch in ('wobble', 'growl') else None, key=keyn, note=note,
             pack=f'Speed Garage/Reese Bass/One-Shots Character/SG_{title.replace(" ", "-")}_{keyn}' + (f'_{bpm}bpm' if patch in ('wobble', 'growl') else ''), genre='sg')
    print('reese loops')
    patch_for = {'SG01': 'classic', 'SG02': 'classic', 'SG03': 'dark', 'SG04': 'classic', 'SG05': 'dark', 'SG06': 'classic'}
    for kid in ['SG01', 'SG02', 'SG03', 'SG04', 'SG05', 'SG06']:
        bpm = K[kid]['bpm']; kl, cam = kkey(kid)
        ns = [(a, b, c, d, e) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
        x = R.render(ns, 16 * 60 / bpm, patch_for[kid], bpm)
        short = kl.replace(' minor', 'min').replace(' major', 'maj').replace('#', 's')
        save('bass', f'reese_loop_{kid.lower()}', f'Reese Bassline · {K[kid]["name"]}', x, bpm, kl, cam, 4, f'{patch_for[kid]} patch, plays {kid} bass A',
             f'Speed Garage/Reese Bass/Loops/SG_ReeseLoop_{kid}_{short}-{cam}_{bpm}bpm', 'sg')
        ns = [(a, b, c, d, e) for a, b, c, d, e, _ in kit_notes(kid, 'bassB', 2)]
        x = R.render(ns, 8 * 60 / bpm, 'wobble', bpm)
        save('bass', f'reese_wob_{kid.lower()}', f'Reese Wobble · {K[kid]["name"]}', x, bpm, kl, cam, 2, f'1/8 wobble, plays {kid} bass B',
             f'Speed Garage/Reese Bass/Wobble Loops/SG_ReeseWobble_{kid}_{short}-{cam}_{bpm}bpm', 'sg')
    print('reese preview instrument')
    for m in range(28, 53, 3):
        save('inst', f'reese_pv_{m}', f'reese preview {m}', R.one_shot(m, 'classic', 2.0, 1.85).mean(axis=1), hidden=True)

    # ---------------- organ bass ----------------
    print('organ bass')
    for m in range(24, 48):
        nm = nname(m)
        save('bass', f'organ_{nm.replace("#", "s")}', f'Organ Bass {nm}', Kz.organ_bass_note(m, 1.1, 0.9), key=nm[:-1],
             note='M1-style organ bass one-shot', pack=f'Classic House/Organ Bass/One-Shots/CH_OrganBass_{nm}', genre='ch',
             sset='organ_bass', noteName=nm)
    for kid in ['CH01', 'CH03', 'CH04', 'CH06']:
        bpm = K[kid]['bpm']; kl, cam = kkey(kid)
        L = int(16 * 60 / bpm * SR)
        buf = np.zeros(L + SR)
        for st, du, m, v, leg, _ in kit_notes(kid, 'bassA'):
            place(buf, Kz.organ_bass_note(m, du + 0.02, du * 0.92, v), int(st * SR))
        short = kl.replace(' minor', 'min').replace(' major', 'maj')
        save('bass', f'organ_loop_{kid.lower()}', f'Organ Bassline · {K[kid]["name"]}', buf[:L], bpm, kl, cam, 4, f'plays {kid} bass A',
             f'Classic House/Organ Bass/Loops/CH_OrganBassLoop_{kid}_{short}-{cam}_{bpm}bpm', 'ch')

    # ---------------- piano ----------------
    print('piano multisample')
    for m in range(36, 85, 3):
        nm = nname(m)
        x = Kz.piano_note(m, 1.6, 104, tail=0.6)
        save('keys', f'piano_{m}', f'House Piano {nm}', x, key=nm[:-1], note='dry multisample note', genre='ch',
             pack=f'Classic House/Piano/Multisample/CH_HousePiano_{nm}', sset='piano_ms', noteName=nm)
    print('piano stabs')
    stabs = [('CH01', 'Am7'), ('CH01', 'Dm7'), ('CH01', 'G7'), ('CH01', 'Cmaj7'), ('CH04', 'Fm9'), ('CH04', 'Bbm9'),
             ('CH04', 'Eb9sus4'), ('CH04', 'Dbmaj9'), ('CH06', 'Cm7'), ('CH06', 'Bbm7'), ('CH02', 'Dm9'), ('CH02', 'Gm9')]
    from dsp import to_stereo
    for kid, ch in stabs:
        kl, cam = kkey(kid)
        notes = [12 * (int(n[-1]) + 1) + PCN[n[:-1]] for n in K[kid]['chords'][ch]]
        r = root_of(ch); rm = 36 + ((r - 36) % 12)
        x = sum(Kz.piano_note(m, 0.32, 112, tail=1.3) for m in notes + [rm, rm + 12]) / 4
        x = Kz.piano_post(x, 0.35, 0.16)
        save('keys', f'pstab_{ch.lower().replace("#", "s")}_{kid.lower()}', f'Piano Stab {ch}', x, key=kl, cam=cam,
             note=f'{ch} voicing from {K[kid]["name"]}', genre='ch',
             pack=f'Classic House/Piano/Stabs/CH_PianoStab_{ch}_{kl.replace(" minor", "min").replace(" major", "maj")}-{cam}')
    print('piano loops')
    for kid in ['CH01', 'CH04', 'CH06']:
        bpm = K[kid]['bpm']; kl, cam = kkey(kid)
        L = int(16 * 60 / bpm * SR)
        buf = np.zeros(L + 2 * SR)
        groups = {}
        for st, du, m, v, leg, ch in kit_notes(kid, 'chordsA'):
            groups.setdefault((round(st, 4), round(du, 4), ch, v), []).append(m)
        for (st, du, ch, v), ms in groups.items():
            r = root_of(ch); rm = 36 + ((r - 36) % 12)
            for m in ms + [rm]:
                place(buf, Kz.piano_note(m, du, v, tail=0.8) / 3.2, int(st * SR))
        y = Kz.piano_post(buf, 0.3, 0.12)
        loop = y[:L].copy(); tail = y[L:L + L]; loop[:len(tail)] += tail[:len(loop)]
        short = kl.replace(' minor', 'min').replace(' major', 'maj')
        save('keys', f'ploop_{kid.lower()}', f'Piano Loop · {K[kid]["name"]}', loop, bpm, kl, cam, 4, f'{kid} chords A with left-hand roots',
             f'Classic House/Piano/Loops/CH_PianoLoop_{kid}_{short}-{cam}_{bpm}bpm', 'ch')

    # ---------------- strings ----------------
    print('strings')
    sch = [('CH01', 'Am7', 0), ('CH06', 'Dbmaj9', 0), ('CH02', 'Gm9', 12), ('CH04', 'Bbm9', 12), ('SG05', 'Cmaj9', 12), ('CH05', 'Gbmaj7', 0)]
    for kid, ch, up in sch:
        kl, cam = kkey(kid)
        notes = [12 * (int(n[-1]) + 1) + PCN[n[:-1]] + up for n in K[kid]['chords'][ch]]
        x = Kz.string_chord(notes + [notes[-1] + 12], 2.6, 2.2)
        save('keys', f'strings_{ch.lower()}', f'Strings {ch}', x, key=kl, cam=cam, note=f'ensemble pad, {ch}', genre='ch' if kid != 'SG05' else 'sg',
             pack=f'Classic House/Strings/CH_Strings_{ch}_{kl.replace(" minor", "min").replace(" major", "maj")}-{cam}')
    kid = 'CH05'; bpm = K[kid]['bpm']; kl, cam = kkey(kid)
    L = int(16 * 60 / bpm * SR); buf = np.zeros((L + 3 * SR, 2))
    for st, du, m, v, leg, _ in kit_notes(kid, 'leadA'):
        place(buf, Kz.string_chord([m, m + 12], du, du * 0.95, attack=0.04) * (v / 127), int(st * SR))
    loop = buf[:L].copy(); tail = buf[L:2 * L]; loop[:len(tail)] += tail[:len(loop)]
    save('keys', 'strings_riff_ch05', 'String Riff · Filter Disco', loop, bpm, kl, cam, 4, 'CH05 melody A on octave strings',
         f'Classic House/Strings/CH_StringRiff_CH05_Bbmin-3A_{bpm}bpm', 'ch')
    # filtered disco chops
    buf = np.zeros(L + SR)
    for st, du, m, v, leg, _ in kit_notes(kid, 'chordsA'):
        place(buf, F.saw_chord([m], du + 0.05, du * 0.9, (3400, 900), 1.2) * (v / 127) * 1.6, int(st * SR))
    for st, du, m, v, leg, _ in kit_notes(kid, 'bassA'):
        place(buf, F.pluck_bass(m, du + 0.03, du * 0.85) * 0.55, int(st * SR))
    sweep = svf(buf[:L], 260 * (9000 / 260) ** (np.linspace(0, 1, L) ** 1.2), 1.1, 'lp')
    x = board(to_stereo(sweep), [pb.Phaser(rate_hz=0.25, depth=0.5, mix=0.25)])
    save('keys', 'disco_filter_ch05', 'Filter Disco Loop', x, bpm, kl, cam, 4, 'chops + octave bass through an opening low-pass',
         f'Classic House/Loops/CH_FilterDiscoLoop_Bbmin-3A_{bpm}bpm', 'ch')

    # ---------------- drum loops ----------------
    print('drum loops')
    for kid in ['CH01', 'CH02', 'CH03', 'CH04', 'CH05', 'CH06', 'SG01', 'SG06']:
        bpm = K[kid]['bpm']; kit = 'house' if kid.startswith('CH') else 'sg'
        x = F.render_drums(K[kid]['drums']['main'], 2, bpm, K[kid]['swing'], kit)
        g = 'Classic House' if kit == 'house' else 'Speed Garage'
        save('loops', f'drums_{kid.lower()}', f'Drum Loop · {K[kid]["name"]}', x, bpm, None, None, 2, f'{kid} main groove, {round(K[kid]["swing"]*100)}% swing',
             f'{g}/Drum Loops/{kid}_DrumLoop_{bpm}bpm', 'ch' if kit == 'house' else 'sg')

    # ---------------- speed garage FX ----------------
    print('sg fx')
    bpm = 132; n = int(16 * 60 / bpm * SR); t = np.arange(n) / SR
    rate = (bpm / 60) * (0.5 * 2 ** (4 * (t / t[-1]) ** 1.5))  # 1/2-note wobble accelerating to 1/32
    ph = np.cumsum(rate / SR)
    lfo = 0.5 - 0.5 * np.cos(2 * np.pi * ph)
    f = float(midi_hz(31))
    Ls = R.stack(np.full(n, f), R.DETUNE_L, n); Rs = R.stack(np.full(n, f), R.DETUNE_R, n)
    fc = (200 + 900 * (t / t[-1])) * 2 ** (2.6 * lfo)
    mids = np.stack([svf(svf(np.tanh(1.6 * Ls), fc, 1.2, 'lp'), fc, 0.7, 'lp'), svf(svf(np.tanh(1.6 * Rs), fc, 1.2, 'lp'), fc, 0.7, 'lp')], 1)
    mids = np.tanh(3 * mids) * (0.3 + 0.7 * (t / t[-1]) ** 1.2)[:, None]
    sub = np.sin(2 * np.pi * f * t) * 0.6 * (t / t[-1])
    x = mids + sub[:, None] + F.noise_riser(4, bpm) * 0.25
    save('fx', 'sg_fx_wobbleriser', 'Wobble Riser', fade(x, 0.01, 0.004), bpm, 'G', None, 4, 'Reese wobble accelerating 1/2 to 1/32',
         f'Speed Garage/FX/SG_FX_WobbleRiser-4bars_G_{bpm}bpm', 'sg')
    x = R.render([(0.0, 3.2, 30, 115, False)], 8 * 60 / bpm, 'classic', bpm, dive=(0.2, -24, 3.0))
    n2 = len(x); env = np.linspace(1, 0, n2) ** 0.7
    save('fx', 'sg_fx_reesedown', 'Reese Downlifter', x * env[:, None], bpm, 'F#', None, 2, 'two-octave dive from F#1',
         f'Speed Garage/FX/SG_FX_ReeseDownlifter-2bars_Fs_{bpm}bpm', 'sg')
    bpm = 134; st = step_seconds(bpm); n = int(16 * st * SR * 2); t = np.arange(n) / SR
    ph = (t / (4 * st)) % 1.0
    semis = 12 * ph ** 0.6
    f = midi_hz(69 + semis)
    x = 0.5 * square(f, pw=0.4) + 0.5 * sine(f)
    x = svf(x, 3000, 0.8, 'lp') * (t < n / SR * 0.75)
    x = F.pingpong(x, 3 * st, 0.4, 0.4, 3500, 8)
    save('fx', 'sg_fx_siren', 'Siren Whoops', fade(trim_to(reverb(x, 0.45, 0.15, 0.5, 1.0, 1.0), n), 0.005, 0.05), bpm, 'A', None, 2, 'quarter-note whoops from A4',
         f'Speed Garage/FX/SG_FX_SirenWhoops_A_{bpm}bpm', 'sg')

    with open(PACK_ROOT + '/out/manifest_more.json', 'w') as f:
        json.dump(OUT, f)
    print('done', len(OUT), 'items in', round(time.time() - t0, 1), 's')


if __name__ == '__main__':
    main()
