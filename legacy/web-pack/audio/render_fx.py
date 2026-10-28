"""Render one-shots, fills and FX into site/audio and write their manifest."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
from dsp import *
import hits as H
import fx as F

SITE = PACK_ROOT + '/site/audio'
OUT = []

KEYS_MIN = {'C': '5A', 'C#': '12A', 'D': '7A', 'Eb': '2A', 'E': '9A', 'F': '4A', 'F#': '11A', 'G': '6A', 'G#': '1A', 'A': '8A', 'Bb': '3A', 'B': '10A'}


def keyinfo(k):
    """'Fm' -> label, camelot ; 'F' (note) -> label only."""
    if k is None:
        return None, None
    if k.endswith('m'):
        r = k[:-1]
        return r + ' minor', KEYS_MIN[r]
    return k, None


def save(cat, sid, title, x, bpm=None, key=None, bars=None, note=None, pack_dir='', pack_name=None, peak=-1.0):
    os.makedirs(f'{SITE}/{cat}', exist_ok=True)
    path = f'{SITE}/{cat}/{sid}.wav'
    y = write(path, x, peak)
    label, cam = keyinfo(key)
    OUT.append({
        'id': sid, 'cat': cat, 'title': title, 'bpm': bpm, 'key': label, 'camelot': cam, 'bars': bars,
        'note': note, 'dur': round(len(y) / SR, 3), 'file': f'audio/{cat}/{sid}.wav',
        'pack': f'{pack_dir}/{pack_name}.wav', 'peaks': peaks(y, 72), 'stereo': y.ndim == 2
    })
    print(f'  {sid:28s} {len(y)/SR:6.2f}s  {bpm or "":>4} {label or "":10s}', flush=True)


def main():
    t0 = time.time()
    hs = F.hits()
    # ---------- one-shots ----------
    print('one-shots')
    for name, fn, label, grp, key in H.ONE_SHOTS:
        kname = label.replace(' ', '-') + (f'_{key}' if key else '')
        save('hits', name, label, hs[name], key=key, note=('tuned to ' + key) if key else None,
             pack_dir='Drum One-Shots', pack_name=f'Hit_{kname}')

    # ---------- UKG fills ----------
    print('ukg fills')
    U = 'UK Garage/Fills'
    x = F.render_drums({'K': ['9..........6..6.'], 'CL': ['....9...........'], 'SN': ['........6.......5...5.6.67788999'],
                        'CH': ['5374537453745...'], 'RIM': ['...5..4.........']}, 1, 134, 0.60, 'ukg',
                       lane_fx={'SN': lambda y: reverb(y, 0.3, 0.12, 0.5, 1.0, 1.0)})
    save('fills', 'ukg_fill_01', 'Shuffle Snare Roll', x, 134, None, 1, None, U, 'UKG_Fill_01_ShuffleSnareRoll_134bpm')

    x = F.render_drums({'K': ['9.....6...8.....'], 'RIM': ['...5..5.4..5.456'], 'CL': ['....9.......9...'],
                        'CH': ['5.6.5.6.5.6.....'], 'SH': ['2423242324232423']}, 1, 132, 0.58, 'ukg')
    save('fills', 'ukg_fill_02', 'Rim Skip', x, 132, None, 1, None, U, 'UKG_Fill_02_RimSkip_132bpm')

    x = F.render_drums({'K': ['9.........8....8'], 'CL': ['....9...........'], 'HT': ['........76......'],
                        'MT': ['..........76....'], 'LT': ['............776.'], 'CH': ['53745374........']}, 1, 134, 0.60, 'ukg',
                       lane_fx={k: (lambda y: reverb(y, 0.35, 0.15, 0.5, 1.0, 1.0)) for k in ('HT', 'MT', 'LT')})
    save('fills', 'ukg_fill_03', 'Tom Cascade', x, 134, 'Fm', 1, 'toms tuned C3 / Ab2 / F2', U, 'UKG_Fill_03_TomCascade_Fmin-4A_134bpm')

    pat = {'K': ['9.........8.....', '9...............'], 'CL': ['....9.......9.5.', '4...4...5...5...5.5.6.6.77778899'],
           'CH': ['5373537353735373', '53735373........']}
    x = F.render_drums(pat, 2, 136, 0.62, 'ukg')
    x += F.noise_riser(2, 136, 400, 8000) * 0.35
    save('fills', 'ukg_fill_04', 'Clap Build', x, 136, None, 2, None, U, 'UKG_Fill_04_ClapBuild-2bars_136bpm')

    x = F.render_drums({'K': ['9.........8.....'], 'SN': ['....9...........'], 'CH': ['5..4..5..4..545454565656'],
                        'RIM': ['...5.....5......']}, 1, 138, 0.57, 'ukg')
    save('fills', 'ukg_fill_05', 'Triplet Hat Roll', x, 138, None, 1, None, U, 'UKG_Fill_05_TripletHatRoll_138bpm')

    x = F.render_drums({'K': ['9..6..9...9.6.66'], 'CL': ['....9.......9...'], 'CH': ['53.453.453.453.4'],
                        'OH': ['..7...7...7...7.']}, 1, 130, 0.56, 'ukg')
    save('fills', 'ukg_fill_06', 'Kick Skip Stutter', x, 130, None, 1, None, U, 'UKG_Fill_06_KickSkipStutter_130bpm')

    x = F.reverse_snare_swell(133, 'ukg')
    save('fills', 'ukg_fill_07', 'Reverse Snare Swell', x, 133, None, 1, None, U, 'UKG_Fill_07_ReverseSnareSwell_133bpm')

    x = F.render_drums({'SN': ['....9.......9..4'], 'RIM': ['..45..4...45..4.'], 'CH': ['5373537353735373'],
                        'SH': ['2423242324232423']}, 1, 136, 0.62, 'ukg')
    x = bfilt(x, 'highpass', 180, 2)
    save('fills', 'ukg_fill_08', 'Two-Step Break', x, 136, None, 1, 'kick drops out', U, 'UKG_Fill_08_TwoStepBreak_136bpm')

    # ---------- minimal fills ----------
    print('minimal fills')
    M = 'Minimal Tech House/Fills'
    x = F.render_drums({'K': ['9...9...9.6.9696'], 'CL': ['....8.......8...'], 'OH': ['..6...6...6.....'],
                        'SH': ['3253325332533253']}, 1, 128, 0.54, 'tech')
    save('fills', 'mth_fill_01', 'Kick Roll', x, 128, None, 1, None, M, 'MTH_Fill_01_KickRoll_128bpm')

    pat = {'K': ['9...9...9...9...'], 'SN': ['4.......4.......5.......5.......', '5...5...6...6...6.6.7.7.88889999'],
           'OH': ['..6...6...6...6.']}
    x = F.render_drums(pat, 2, 128, 0.52, 'tech', lane_fx={'SN': lambda y: reverb(y, 0.45, 0.18, 0.5, 1.0, 1.0)})
    x += F.noise_riser(2, 128, 300, 9000) * 0.3
    save('fills', 'mth_fill_02', '32nd Snare Build', x, 128, None, 2, None, M, 'MTH_Fill_02_SnareBuild32nds-2bars_128bpm')

    x = F.render_drums({'K': ['9...9...9...9...'], 'CL': ['....8.......8...'], 'WB': ['4...4...4.4.4...5.5.5.5.66667777'],
                        'CLV': ['...4...4...4...4'], 'RIM': ['..5..5..5..5..5.']}, 1, 130, 0.54, 'tech')
    save('fills', 'mth_fill_03', 'Perc Click Burst', x, 130, None, 1, None, M, 'MTH_Fill_03_PercClickBurst_130bpm')

    x = F.render_drums({'K': ['9...9...9.......'], 'CL': ['....8...........'], 'COH': ['........5.5.5.5.'],
                        'CLO': ['.........5.5.5.5'], 'BH': ['...5..4.........'], 'SH': ['32433243........']}, 1, 128, 0.56, 'tech')
    save('fills', 'mth_fill_04', 'Conga Roll', x, 128, None, 1, 'congas E4 / A3', M, 'MTH_Fill_04_CongaRoll_128bpm')

    x = F.render_drums({'K': ['9...9...9...9...'], 'CH': ['5.3.5.3.5.3.5.3.5555666677778888'], 'OH': ['..6...6.........'],
                        'CL': ['....8.......8...']}, 1, 132, 0.52, 'tech')
    save('fills', 'mth_fill_05', 'Hat Stutter', x, 132, None, 1, None, M, 'MTH_Fill_05_HatStutter_132bpm')

    st = step_seconds(129)
    x = F.render_drums({'K': ['9...9...9...9...'], 'CL': ['....8.......8.64'], 'SH': ['3233323332333233']}, 1, 129, 0.55, 'tech',
                       lane_fx={'CL': lambda y: F.pingpong(y, 3 * st, 0.4, 0.4, 3000, 6)})
    save('fills', 'mth_fill_06', 'Clap Echo', x, 129, None, 1, None, M, 'MTH_Fill_06_ClapEcho_129bpm')

    x = F.render_drums({'K': ['9...9...9...9...'], 'RIM': ['5..5..5..5..5..5'], 'CLV': ['..4...4...4...4.'],
                        'SH': ['2323232323232323']}, 1, 131, 0.53, 'tech')
    save('fills', 'mth_fill_07', 'Rim Polyrhythm', x, 131, None, 1, None, M, 'MTH_Fill_07_RimPolyrhythm_131bpm')

    x = F.render_drums({'CL': ['....8.......8...'], 'OH': ['..6...6...6...6.'], 'CH': ['43.343.343.343.3'],
                        'RIM': ['...5...5.....5..']}, 1, 130, 0.54, 'tech')
    n = len(x)
    x = svf_stereo(x, np.linspace(150, 2200, n) ** 1.0, 0.9, 'hp')
    save('fills', 'mth_fill_08', 'Drop-out Break', x, 130, None, 1, 'no kick, rising high-pass', M, 'MTH_Fill_08_DropoutBreak_130bpm')

    # ---------- UKG FX ----------
    print('ukg fx')
    UX = 'UK Garage/FX'
    save('fx', 'ukg_fx_rewind', 'Rewind Spinback', F.rewind_fx(134), 134, 'Fm', 1, 'groove then pull-up', UX, 'UKG_FX_RewindSpinback_Fmin-4A_134bpm')
    save('fx', 'ukg_fx_siren', 'Dub Siren', F.dub_siren(130, 67), 130, 'G', 2, 'whoops start on G4', UX, 'UKG_FX_DubSiren_G_130bpm')
    save('fx', 'ukg_fx_laser', 'Laser Zaps', F.laser_zaps(134, 53), 134, 'F', 1, 'zaps land on F3', UX, 'UKG_FX_LaserZaps_F_134bpm')
    save('fx', 'ukg_fx_subdrop', 'Sub Drop', F.sub_drop(134, 41), 134, 'F', 1, 'falls from F2', UX, 'UKG_FX_SubDrop_F_134bpm')
    save('fx', 'ukg_fx_revcrash', 'Reverse Crash', F.reverse_crash(134), 134, None, 1, None, UX, 'UKG_FX_ReverseCrash_134bpm')
    save('fx', 'ukg_fx_riser', 'White Noise Riser', F.noise_riser(4, 134), 134, None, 4, None, UX, 'UKG_FX_NoiseRiser-4bars_134bpm')
    save('fx', 'ukg_fx_organecho', 'Organ Stab Echo', F.stab_echo(134, [41, 56, 60, 63, 67], 'organ'), 134, 'Fm', 2, 'Fm9 stab, dotted-8th echo', UX, 'UKG_FX_OrganStabEcho_Fm9_Fmin-4A_134bpm')
    g = F.groove_simple('ukg', {'K': ['9.........8..5..'], 'CL': ['....9.......9...'], 'CH': ['5373537353735373'], 'RIM': ['..4...4....4..4.']},
                        ([60, 64, 67, 71], [(0, 1), (3, 1), (6, 2), (9, 1), (12, 2)]), F.organ,
                        [(0, 2, 33), (3, 1, 45), (6, 2, 33), (10, 2, 36), (13, 1, 33)], 136, 0.62, 2)
    save('fx', 'ukg_fx_vinylstop', 'Vinyl Stop', F.stop_fx(g, 136, 0.06, 2.6), 136, 'Am', 1, 'groove then platter stop', UX, 'UKG_FX_VinylStop_Amin-8A_136bpm')

    # ---------- minimal FX ----------
    print('minimal fx')
    MX = 'Minimal Tech House/FX'
    save('fx', 'mth_fx_riser', 'Noise Riser', F.noise_riser(8, 128, 250, 11000, 1.4), 128, None, 8, None, MX, 'MTH_FX_NoiseRiser-8bars_128bpm')
    save('fx', 'mth_fx_downsweep', 'Downsweep', F.downsweep(2, 128), 128, None, 2, None, MX, 'MTH_FX_Downsweep-2bars_128bpm')
    save('fx', 'mth_fx_impact', 'Sub Impact', F.impact(128, 33), 128, 'A', 2, 'sub on A1', MX, 'MTH_FX_SubImpact_A_128bpm')
    save('fx', 'mth_fx_revcrash', 'Reverse Crash', F.reverse_crash(130), 130, None, 1, None, MX, 'MTH_FX_ReverseCrash_130bpm')
    save('fx', 'mth_fx_dubecho', 'Dub Stab Echo', F.stab_echo(131, [50, 53, 57, 60, 64], 'saw', 2, 0.55), 131, 'Dm', 2, 'Dm9 stab, dotted-8th echo', MX, 'MTH_FX_DubStabEcho_Dm9_Dmin-7A_131bpm')
    g = F.groove_simple('tech', {'K': ['9...9...9...9...'], 'CL': ['....8.......8...'], 'OH': ['..6...6...6...6.'], 'SH': ['3253325332533253']},
                        None, None, [(2, 2, 33), (6, 1, 33), (7, 1, 45), (10, 2, 33), (14, 1, 36), (15, 1, 33)], 128, 0.54, 2)
    save('fx', 'mth_fx_tapestop', 'Tape Stop', F.stop_fx(g, 128, 0.0, 1.8), 128, 'Am', 1, 'groove then tape stop', MX, 'MTH_FX_TapeStop_Amin-8A_128bpm')
    g = F.groove_simple('tech', {'K': ['9...9...9...9...'], 'CL': ['....8.......8...'], 'CH': ['53.353.353.353.3'], 'OH': ['..6...6...6...6.']},
                        None, None, [(2, 1, 35), (3, 1, 35), (6, 1, 35), (7, 1, 47), (10, 1, 35), (11, 1, 35), (13, 1, 38), (15, 1, 35)], 132, 0.52, 1)
    save('fx', 'mth_fx_glitch', 'Glitch Stutter', F.glitch_stutter(g, 132), 132, 'Bm', 1, 'beat repeat 1/4 to 1/32', MX, 'MTH_FX_GlitchStutter_Bmin-10A_132bpm')
    save('fx', 'mth_fx_pitchriser', 'Pitch Riser', F.pitch_riser(4, 132, 47, 2), 132, 'Bm', 4, 'rises B2 to B4', MX, 'MTH_FX_PitchRiser-4bars_Bmin-10A_132bpm')

    with open(PACK_ROOT + '/out/manifest_fx.json', 'w') as f:
        json.dump(OUT, f)
    print('done', len(OUT), 'items in', round(time.time() - t0, 1), 's')


if __name__ == '__main__':
    main()
