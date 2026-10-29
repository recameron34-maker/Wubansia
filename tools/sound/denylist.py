"""Content denylists, stored as SHA-256 digests so the repo never spells out an artist name or a slur.

A text is split into words (camelCase and digits split too, accents folded, lowercase); every run of 1-4 words is
checked, spaced and run together ("Kick Name" -> "kick name", "kickname"). An entry is added the same way, so "Some
Name" also catches "SomeName", "some-name" and "some_name".

    python3 tools/sound/denylist.py add artists < names.txt     add entries (one per line) from stdin
    python3 tools/sound/denylist.py check "FUT01_Drums_KickAlloy"  print the lists a text hits

Lists: artists (style references only, never in names or strings), slurs (never, R-rated or not), profanity (allowed
in R-rated lines only, so a clean line must have none).
"""
import hashlib
import re
import sys
import unicodedata
from pathlib import Path

LISTS = ('artists', 'slurs', 'profanity')
HERE = Path(__file__).resolve().parent / 'denylists'
SALT = 'wubansia-denylist-v1:'
MAX_WORDS = 4


def split_words(text):
    t = unicodedata.normalize('NFKD', str(text))
    t = ''.join(c for c in t if not unicodedata.combining(c))
    t = re.sub(r'(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', t)  # camelCase, ABCWord
    t = re.sub(r'(?<=[A-Za-z])(?=[0-9])|(?<=[0-9])(?=[A-Za-z])', ' ', t)
    return re.sub(r'[^a-z0-9]+', ' ', t.lower().replace("'", '')).split()


def grams(text):
    """Every run of 1..MAX_WORDS words, spaced and run together."""
    w = split_words(text)
    out = set()
    for i in range(len(w)):
        for j in range(i + 1, min(len(w), i + MAX_WORDS) + 1):
            out.add(' '.join(w[i:j]))
            out.add(''.join(w[i:j]))
    return out


def digest(gram):
    return hashlib.sha256((SALT + gram).encode('utf-8')).hexdigest()


def entry_digests(entry):
    w = split_words(entry)
    return {digest(' '.join(w)), digest(''.join(w))} if w else set()


def path(name):
    return HERE / f'{name}.sha256'


def load(name):
    """The digests of one list (lines of 64 hex characters; '#' starts a comment)."""
    lines = path(name).read_text(encoding='utf-8').splitlines()
    return frozenset(s for s in (ln.split('#')[0].strip() for ln in lines) if s)


def hits(text, digests):
    """The word runs of `text` whose digest is in the list (longest first)."""
    return sorted((g for g in grams(text) if digest(g) in digests), key=lambda g: (-len(g), g))


def add(name, entries):
    """Add entries to a list file, keeping its header comment; returns how many digests were new."""
    p = path(name)
    old = p.read_text(encoding='utf-8').splitlines() if p.exists() else []
    head = [ln for ln in old if ln.startswith('#')]
    have = set(load(name)) if p.exists() else set()
    new = set().union(*(entry_digests(e) for e in entries if e.strip())) - have
    p.write_text('\n'.join(head + sorted(have | new)) + '\n', encoding='utf-8')
    return len(new)


def main(argv):
    if len(argv) == 2 and argv[0] == 'add' and argv[1] in LISTS:
        print(f'{add(argv[1], sys.stdin.read().splitlines())} new digests in {path(argv[1])}')
        return 0
    if len(argv) >= 2 and argv[0] == 'check':
        found = 0
        for name in LISTS:
            h = hits(' '.join(argv[1:]), load(name))
            if h:
                found += 1
                print(f'{name}: {len(h)} hit(s)')
        return 1 if found else 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
