"""Copy a render's FLAC masters to WAV for listening, in plain-named folders.

    python3 tools/sound/listen.py build/s1a-listen

Reads <out>/qa_report.json and writes each file as a WAV with the same 48 kHz / 24-bit samples (bit for bit) into
Drums/, Bass/, Music/, Vox/, FX/ or Loops/, named in plain words: FUT01_Drums_KickSplit_v5_rr1.flac becomes
Drums/Kick Split v5 rr1.wav, FUT01_Bass_Reese-Fs1_v2_rr1.flac becomes Bass/Reese F#1 v2 rr1.wav; the layer or round
robin is left out when the instrument has only one. The folders are rebuilt on every run. Renders live under build/,
never in git.
"""
import json
import re
import shutil
import sys
from pathlib import Path

import soundfile as sf

FOLDERS = {'Drums': 'Drums', 'Perc': 'Drums', 'Bass': 'Bass', 'Stabs': 'Music', 'Pad': 'Music', 'Lead': 'Music',
           'Vox': 'Vox', 'FX': 'FX', 'Textures': 'FX', 'Loops': 'Loops'}
WORD = re.compile(r'[A-G]s?m?\d+|[A-Z]{2,}(?=[A-Z][a-z]|\d|$)|[A-Z][a-z]+|[A-Z]|\d+')
NOTE = re.compile(r'^([A-G])(s?)(m?)(-?\d+)$')
FILE = re.compile(r'^[A-Z0-9]+_[A-Za-z]+_(?P<sound>[A-Za-z0-9]+)(-(?P<note>[A-G]s?-?\d+))?'
                  r'_v(?P<v>\d+)_rr(?P<rr>\d+)\.flac$')


def note(token):
    """Fs1 -> F#1, Fsm9 -> F#m9; anything else unchanged."""
    m = NOTE.match(token)
    return f"{m[1]}{'#' if m[2] else ''}{m[3]}{m[4]}" if m else token


def parse(file):
    m = FILE.match(file)
    if not m:
        raise ValueError(f'not a master file name: {file}')
    return m


def plain(file, layers=True, rrs=True):
    """The plain name of a master: its sound split into words, its zone as a note, then the layer and the round robin
    (each left out when the instrument has only one)."""
    m = parse(file)
    words = [note(w) for w in WORD.findall(m['sound'])]
    if m['note']:
        words.append(note(m['note']))
    words += [f"v{m['v']}"] * layers + [f"rr{m['rr']}"] * rrs
    return ' '.join(words) + '.wav'


def build(out_dir):
    """Rebuild the WAV folders from out_dir's report; returns the paths written."""
    out_dir = Path(out_dir)
    report = json.loads((out_dir / 'qa_report.json').read_text(encoding='utf-8'))
    for f in set(FOLDERS.values()):
        shutil.rmtree(out_dir / f, ignore_errors=True)
    written = []
    for item in report['items']:
        folder = out_dir / FOLDERS[item['group']]
        folder.mkdir(exist_ok=True)
        ms = [parse(f['file']) for f in item['files']]
        layers, rrs = len({m['v'] for m in ms}) > 1, len({m['rr'] for m in ms}) > 1
        for f in item['files']:
            data, sr = sf.read(out_dir / f['file'], dtype='int32')
            dest = folder / plain(f['file'], layers, rrs)
            if dest.exists():
                raise ValueError(f'two masters map to {dest}')
            sf.write(dest, data, sr, subtype='PCM_24', format='WAV')
            written.append(dest)
    return written


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print(__doc__.strip().split('\n\n')[1], file=sys.stderr)
        return 2
    written = build(argv[0])
    print(f'{len(written)} WAV files in {", ".join(sorted({p.parent.name for p in written}))}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
