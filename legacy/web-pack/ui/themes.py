"""Genre skins for the pack page: one palette per genre and colour scheme. emit_css() writes the token blocks
(bare :root = UK garage light; [data-genre] variants; the dark scheme by media query and by data-theme)."""
import re

T = {
  # UK garage: chrome and champagne on aubergine (Y2K rave flyer)
  'ukg': {
    'light': dict(bg='#EFEDF5', surface='#FBFAFE', surface2='#E5E1EF', line='#D2CDE0', ink='#1A1530', muted='#5B5575',
                  accent='#6A35D0', accentInk='#FFFFFF', accentText='#5B2BBE', accent2='#9A6F12', soft='rgba(106,53,208,.12)',
                  stage='#1C1236', stage2='#3A2470', stageInk='#F6F2FF', stageMuted='#C8BEE8', stageLine='rgba(255,255,255,.16)',
                  viz1='#F1F3FA', viz2='#9AA3C7', viz3='#E7C66F', stageHi='#E7C66F', stageHiInk='#1C1236'),
    'dark': dict(bg='#100D1A', surface='#191527', surface2='#231E36', line='#2F2946', ink='#EEEAF8', muted='#A59EBF',
                 accent='#B69BFF', accentInk='#1B1236', accentText='#C4AEFF', accent2='#E7C66F', soft='rgba(182,155,255,.16)',
                 stage='#150D2A', stage2='#2E1C58', stageInk='#F6F2FF', stageMuted='#C2B6E6', stageLine='rgba(255,255,255,.13)',
                 viz1='#F1F3FA', viz2='#9AA3C7', viz3='#E7C66F', stageHi='#E7C66F', stageHiInk='#150D2A'),
  },
  # Garage house: lagoon teal and flamingo
  'gh': {
    'light': dict(bg='#E8F4F1', surface='#FAFEFD', surface2='#D9EDE8', line='#BFDCD5', ink='#0A2925', muted='#476660',
                  accent='#C42464', accentInk='#FFFFFF', accentText='#B51F5B', accent2='#08806F', soft='rgba(196,36,100,.11)',
                  stage='#0A4542', stage2='#11706A', stageInk='#F1FFFB', stageMuted='#B3E2D8', stageLine='rgba(255,255,255,.17)',
                  viz1='#FF8FBE', viz2='#5BE7CF', viz3='#FFD39A', stageHi='#FF8FBE', stageHiInk='#3A0820'),
    'dark': dict(bg='#061816', surface='#0B2320', surface2='#112F2B', line='#1B3D38', ink='#E5F6F2', muted='#8DB2AB',
                 accent='#FF7BB0', accentInk='#3A0820', accentText='#FF8FBC', accent2='#4FDDC5', soft='rgba(255,123,176,.15)',
                 stage='#06302D', stage2='#0D5550', stageInk='#F1FFFB', stageMuted='#A9DCD1', stageLine='rgba(255,255,255,.14)',
                 viz1='#FF8FBE', viz2='#5BE7CF', viz3='#FFD39A', stageHi='#FF8FBE', stageHiInk='#3A0820'),
  },
  # Minimal tech house: concrete and one signal colour
  'mth': {
    'light': dict(bg='#E4E4E0', surface='#F3F3F0', surface2='#D7D7D2', line='#C1C1BB', ink='#121312', muted='#5A5C58',
                  accent='#2B3CFF', accentInk='#FFFFFF', accentText='#2433E0', accent2='#121312', soft='rgba(43,60,255,.10)',
                  stage='#D3D3CE', stage2='#C6C6C0', stageInk='#121312', stageMuted='#50524E', stageLine='rgba(18,19,18,.16)',
                  viz1='#121312', viz2='#2B3CFF', viz3='#8E908B', stageHi='#2B3CFF', stageHiInk='#FFFFFF'),
    'dark': dict(bg='#111212', surface='#1A1B1B', surface2='#232525', line='#303232', ink='#E5E6E2', muted='#8F928D',
                 accent='#7D8CFF', accentInk='#0B1040', accentText='#97A3FF', accent2='#E5E6E2', soft='rgba(125,140,255,.15)',
                 stage='#191A1A', stage2='#242626', stageInk='#E5E6E2', stageMuted='#9A9D98', stageLine='rgba(255,255,255,.11)',
                 viz1='#E5E6E2', viz2='#7D8CFF', viz3='#5E615D', stageHi='#7D8CFF', stageHiInk='#0B1040'),
  },
  # Speed garage: navy, hazard yellow, siren red
  'sg': {
    'light': dict(bg='#ECEEF6', surface='#FBFCFF', surface2='#DEE2EF', line='#C8CEE0', ink='#0C1230', muted='#525A78',
                  accent='#FFCC1A', accentInk='#0C1230', accentText='#8C5E00', accent2='#D3122F', soft='rgba(255,204,26,.22)',
                  stage='#0B1236', stage2='#18255F', stageInk='#F4F6FF', stageMuted='#AEB8E2', stageLine='rgba(255,255,255,.15)',
                  viz1='#FFD21F', viz2='#FF3B4E', viz3='#7FB2FF', stageHi='#FFD21F', stageHiInk='#0C1230'),
    'dark': dict(bg='#080C1D', surface='#0F152C', surface2='#161E3C', line='#222B4F', ink='#ECF0FF', muted='#98A2C4',
                 accent='#FFD21F', accentInk='#0C1230', accentText='#FFDA4D', accent2='#FF4A5C', soft='rgba(255,210,31,.16)',
                 stage='#070C26', stage2='#121C4C', stageInk='#F4F6FF', stageMuted='#A6B0DA', stageLine='rgba(255,255,255,.13)',
                 viz1='#FFD21F', viz2='#FF3B4E', viz3='#7FB2FF', stageHi='#FFD21F', stageHiInk='#0C1230'),
  },
  # Classic house: Chicago sky blue, white and star red
  'ch': {
    'light': dict(bg='#E8F2F8', surface='#FFFFFF', surface2='#D9E9F3', line='#C1D8E7', ink='#0C1A25', muted='#476073',
                  accent='#1C79B8', accentInk='#FFFFFF', accentText='#16679E', accent2='#D0021B', soft='rgba(28,121,184,.12)',
                  stage='#A6D7EF', stage2='#FFFFFF', stageInk='#0C1A25', stageMuted='#2C4A5E', stageLine='rgba(12,26,37,.16)',
                  viz1='#0C1A25', viz2='#E4002B', viz3='#FFFFFF', stageHi='#D0021B', stageHiInk='#FFFFFF'),
    'dark': dict(bg='#0A131B', surface='#111D27', surface2='#182836', line='#233747', ink='#E7F1F8', muted='#90A7B8',
                 accent='#6CC4F0', accentInk='#04263A', accentText='#86D0F4', accent2='#FF4A62', soft='rgba(108,196,240,.15)',
                 stage='#0E2231', stage2='#163A52', stageInk='#EAF5FC', stageMuted='#9CC3DA', stageLine='rgba(255,255,255,.13)',
                 viz1='#05090D', viz2='#FF3150', viz3='#9ED8F5', stageHi='#FF4A62', stageHiInk='#2C0008'),
  },
  # Groove house (Chris Stussy-style): lamp light on a canal at night - espresso, sodium amber, cream, a little canal green
  'gv': {
    'light': dict(bg='#F3ECE2', surface='#FFFBF5', surface2='#E9DFD2', line='#D9CCBB', ink='#24180F', muted='#6A5848',
                  accent='#A84B0B', accentInk='#FFFFFF', accentText='#9A4409', accent2='#3F6B3A', soft='rgba(168,75,11,.12)',
                  stage='#2A1A10', stage2='#4A2E1A', stageInk='#FFF4E6', stageMuted='#D9C2A6', stageLine='rgba(255,255,255,.15)',
                  viz1='#FFB347', viz2='#F1DFC2', viz3='#7FBF86', stageHi='#FFB347', stageHiInk='#2A1A10'),
    'dark': dict(bg='#120C08', surface='#1B130D', surface2='#251A12', line='#35271B', ink='#F5ECE1', muted='#B39E88',
                 accent='#FFAA45', accentInk='#2A1606', accentText='#FFB45C', accent2='#8FCB8A', soft='rgba(255,170,69,.15)',
                 stage='#1A100A', stage2='#3A2414', stageInk='#FFF4E6', stageMuted='#D4BC9E', stageLine='rgba(255,255,255,.12)',
                 viz1='#FFB347', viz2='#F1DFC2', viz3='#7FBF86', stageHi='#FFB347', stageHiInk='#2A1606'),
  },
  # Tech house (Chris Lake & Riordan-style): black booth, signal red lasers, white light
  'th': {
    'light': dict(bg='#EDEDED', surface='#FFFFFF', surface2='#E2E2E2', line='#CFCFCF', ink='#0B0B0B', muted='#555555',
                  accent='#D10018', accentInk='#FFFFFF', accentText='#C20016', accent2='#0B0B0B', soft='rgba(209,0,24,.10)',
                  stage='#0B0B0B', stage2='#1C1C1C', stageInk='#FFFFFF', stageMuted='#B5B5B5', stageLine='rgba(255,255,255,.14)',
                  viz1='#FF2A3D', viz2='#FFFFFF', viz3='#6E6E6E', stageHi='#FF2A3D', stageHiInk='#0B0B0B'),
    'dark': dict(bg='#080808', surface='#121212', surface2='#1B1B1B', line='#2A2A2A', ink='#F2F2F2', muted='#9C9C9C',
                 accent='#FF3347', accentInk='#1A0003', accentText='#FF5C6C', accent2='#F2F2F2', soft='rgba(255,51,71,.16)',
                 stage='#000000', stage2='#161616', stageInk='#FFFFFF', stageMuted='#ABABAB', stageLine='rgba(255,255,255,.12)',
                 viz1='#FF2A3D', viz2='#FFFFFF', viz3='#5A5A5A', stageHi='#FF2A3D', stageHiInk='#0B0B0B'),
  },
  # Halloween (a seasonal skin, worn while the Halloween view is open): pumpkin orange and witch purple on a night sky, bone white moon
  'hw': {
    'light': dict(bg='#F2ECE3', surface='#FFFBF5', surface2='#E9E0D2', line='#D8CBB8', ink='#1D1326', muted='#5D4F66',
                  accent='#B0420A', accentInk='#FFFFFF', accentText='#A13C08', accent2='#6A2EA0', soft='rgba(176,66,10,.11)',
                  stage='#150C22', stage2='#2C1744', stageInk='#FFF3E4', stageMuted='#CDB9DA', stageLine='rgba(255,255,255,.14)',
                  viz1='#FF8A1F', viz2='#9BE15D', viz3='#F4EBDA', stageHi='#FF8A1F', stageHiInk='#1A0E04'),
    'dark': dict(bg='#0D0912', surface='#16101C', surface2='#201828', line='#2E2338', ink='#F3EBF6', muted='#A898B4',
                 accent='#FF8A1F', accentInk='#1A0E04', accentText='#FFA14A', accent2='#B78EFF', soft='rgba(255,138,31,.15)',
                 stage='#0C0615', stage2='#25123B', stageInk='#FFF3E4', stageMuted='#C8B4D8', stageLine='rgba(255,255,255,.12)',
                 viz1='#FF8A1F', viz2='#9BE15D', viz3='#F4EBDA', stageHi='#FF8A1F', stageHiInk='#1A0E04'),
  },
}
# the seven families' signature colours, for swatches, dots and tags shown side by side whatever skin is on
SIG = {'light': dict(ukg='#6A35D0', gh='#C42464', mth='#2B3CFF', sg='#8A5D00', ch='#1C79B8', gv='#A84B0B', th='#D10018', ok='#1E7A43'),
       'dark': dict(ukg='#B69BFF', gh='#FF7BB0', mth='#7D8CFF', sg='#FFD21F', ch='#6CC4F0', gv='#FFAA45', th='#FF3347', ok='#6BD394')}
ORDER = ['ukg', 'gh', 'mth', 'sg', 'ch', 'gv', 'th']
SKINS = ORDER + ['hw']


def var(k):
    k = re.sub(r'([A-Z])', r'-\1', k).lower()
    return '--' + re.sub(r'([a-z])(\d)', r'\1-\2', k)


def block(sel, d, scheme=None):
    body = ' '.join(f'{var(k)}: {v};' for k, v in d.items())
    return f'{sel} {{ {body}' + (f' color-scheme: {scheme};' if scheme else '') + ' }'


def emit_css():
    out = []
    out.append(block(':root', {**T['ukg']['light'], **{'g-' + k: v for k, v in SIG['light'].items()}}))
    for g in SKINS[1:]:
        out.append(block(f':root[data-genre="{g}"]', T[g]['light']))
    dark = ['@media (prefers-color-scheme: dark) {']
    dark.append('  ' + block(':root:not([data-theme="light"])', {**T['ukg']['dark'], **{'g-' + k: v for k, v in SIG['dark'].items()}}, 'dark'))
    for g in SKINS[1:]:
        dark.append('  ' + block(f':root[data-genre="{g}"]:not([data-theme="light"])', T[g]['dark']))
    dark.append('}')
    out += dark
    out.append(block(':root[data-theme="dark"]', {**T['ukg']['dark'], **{'g-' + k: v for k, v in SIG['dark'].items()}}, 'dark'))
    for g in SKINS[1:]:
        out.append(block(f':root[data-genre="{g}"][data-theme="dark"]', T[g]['dark']))
    return '\n'.join(out)


if __name__ == '__main__':
    # contrast check of the pairs the page relies on
    def lum(h):
        h = h.lstrip('#')
        r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
        f = lambda c: c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
        return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b)

    def cr(a, b):
        la, lb = sorted((lum(a), lum(b)), reverse=True)
        return (la + 0.05) / (lb + 0.05)
    pairs = [('ink', 'bg', 7), ('muted', 'bg', 4.5), ('muted', 'surface', 4.5), ('muted', 'surface2', 4.0), ('accentText', 'bg', 4.5), ('accentText', 'surface', 4.5),
             ('accentInk', 'accent', 4.5), ('stageInk', 'stage', 7), ('stageMuted', 'stage', 4.5), ('stageInk', 'stage2', 4.5), ('stageHiInk', 'stageHi', 4.5)]
    for g in SKINS:
        for m in ('light', 'dark'):
            d = T[g][m]
            bad = [f'{a}/{b} {cr(d[a], d[b]):.2f}' for a, b, need in pairs if cr(d[a], d[b]) < need]
            print(g, m, 'OK' if not bad else 'LOW: ' + ', '.join(bad))
    for m in ('light', 'dark'):
        bg = T['ukg'][m]['surface']
        print('signature', m, {k: round(cr(v, bg), 2) for k, v in SIG[m].items()})
    print(len(emit_css()), 'bytes of theme CSS')
