"""Helpers for writing Decent Sampler presets (.dspreset), their skins, and matching SFZ files."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import os, re, math
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np

UIW, UIH = 812, 375
FONT_DIR = PACK_ROOT + '/inst/fonts'

# colours (the app's dark theme)
C = dict(chassis='#191917', panel='#22221f', raised='#2a2926', ink='#ece9e2', muted='#a39e94', line='#3b3a35',
         lcd='#a2b385', lcdink='#142010', lcddim='#5c6a46', accent='#d8492f',
         ukg='#c68be6', mth='#6db7da', sg='#e6a25a', ch='#7cc99c', all='#d8492f', gh='#e3c06f')


def argb(hexcol, a=255):
    h = hexcol.lstrip('#')
    return '%02X%s' % (a, h.upper())


def fmt(v):
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, float):
        s = ('%.6f' % v).rstrip('0').rstrip('.')
        return s if s not in ('', '-0') else '0'
    return str(v)


def el(tag, attrs=None, *children):
    e = ET.Element(tag, {k: fmt(v) for k, v in (attrs or {}).items() if v is not None})
    for c in children:
        if c is None:
            continue
        if isinstance(c, (list, tuple)):
            for cc in c:
                if cc is not None:
                    e.append(cc)
        else:
            e.append(c)
    return e


def binding(**a):
    return el('binding', a)


def db(lin):
    return '%.1fdB' % (20 * math.log10(max(lin, 1e-6)))


# a cutoff curve that feels even across a 0-1 knob and suits CC74 (0-127)
CUTOFF_TABLE = '0,60;0.2,160;0.35,380;0.5,900;0.65,2000;0.8,4600;0.9,9000;1.0001,22000'
TONE_TABLE = '0,300;0.3,900;0.5,2200;0.7,5200;0.85,10000;1.0001,22000'
LOWCUT_TABLE = '0,20;0.3,60;0.5,140;0.7,320;0.85,700;1.0001,1500'


# ---------------------------------------------------------------- skins
_font_cache = {}


def font(size, wght=600, wdth=100, lcd=False):
    key = (size, wght, wdth, lcd)
    if key not in _font_cache:
        if lcd:
            f = ImageFont.truetype(os.path.join(FONT_DIR, 'VT323.ttf'), size)
        else:
            f = ImageFont.truetype(os.path.join(FONT_DIR, 'Archivo.ttf'), size)
            f.set_variation_by_axes([wght, wdth])
        _font_cache[key] = f
    return _font_cache[key]


def spaced(draw, xy, text, fnt, fill, tracking=0.0, anchor='la'):
    """Letter-spaced text (PIL has no tracking)."""
    x, y = xy
    if anchor[0] == 'r':
        total = sum(draw.textlength(ch, font=fnt) + tracking for ch in text) - tracking
        x -= total
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill, anchor='l' + anchor[1])
        x += draw.textlength(ch, font=fnt) + tracking
    return x


def fit(d, text, size, maxw, lcd=False, wght=600, wdth=100):
    while size > 8:
        f = font(size, wght, wdth, lcd)
        if d.textlength(text, font=f) <= maxw:
            return f
        size -= 1
    return font(size, wght, wdth, lcd)


def skin(path, title, subtitle, lcd1, lcd2, genre, sections, legend=None, menus=(), scale=2):
    """Background image. sections: [(x, y, w, h, label)], menus: [(x, y, w, h, label)], legend: list of lines."""
    S = scale
    img = Image.new('RGB', (UIW * S, UIH * S), C['chassis'])
    grad = np.linspace(1.06, 0.94, UIH * S)[:, None, None]
    arr = np.asarray(img, dtype=float) * grad
    rng = np.random.default_rng(7)
    arr += rng.normal(0, 2.2, arr.shape[:2])[:, :, None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype('uint8'))
    d = ImageDraw.Draw(img)
    gc = C.get(genre, C['accent'])
    spaced(d, (24 * S, 18 * S), 'UKG & MINIMAL PACK', font(10 * S, 700, 125), C['muted'], 1.6 * S)
    d.text((24 * S, 30 * S), title.upper(), font=fit(d, title.upper(), 32 * S, 500 * S, wght=850, wdth=125), fill=C['ink'])
    x0, y0, x1, y1 = 540 * S, 14 * S, 788 * S, 68 * S
    d.rounded_rectangle((x0, y0, x1, y1), radius=5 * S, fill=C['lcd'], outline='#6f7d57', width=S)
    for yy in range(y0 + 2 * S, y1, 3 * S):
        d.line((x0 + 3 * S, yy, x1 - 3 * S, yy), fill='#9aab7d', width=1)
    d.text((x0 + 12 * S, y0 + 6 * S), lcd1.upper(), font=fit(d, lcd1.upper(), 24 * S, (x1 - x0) - 24 * S, lcd=True), fill=C['lcdink'])
    d.text((x0 + 12 * S, y0 + 29 * S), lcd2.upper(), font=fit(d, lcd2.upper(), 20 * S, (x1 - x0) - 24 * S, lcd=True), fill=C['lcdink'])
    d.line((24 * S, 80 * S, 788 * S, 80 * S), fill=C['line'], width=S)
    d.line((24 * S, 80 * S, 128 * S, 80 * S), fill=gc, width=3 * S)
    d.text((24 * S, 87 * S), subtitle, font=fit(d, subtitle, 12 * S, 764 * S, wght=500), fill=C['muted'])
    for (mx, my, mw, mh, ml) in menus:
        spaced(d, ((mx - 10) * S, (my + mh / 2) * S), ml.upper(), font(9 * S, 700, 125), C['muted'], 1.2 * S, anchor='rm')
    for (sx, sy, sw, sh, sl) in sections:
        d.rounded_rectangle((sx * S, sy * S, (sx + sw) * S, (sy + sh) * S), radius=7 * S, fill=C['panel'], outline=C['line'], width=S)
        if sl:
            spaced(d, ((sx + 12) * S, (sy + 11) * S), sl.upper(), font(9 * S, 700, 125), gc, 1.4 * S, anchor='lm')
    if legend:
        y = (UIH - 10 - 15 * len(legend)) * S
        for line in legend:
            d.text((24 * S, y), line, font=fit(d, line, 11 * S, 764 * S, wght=500), fill=C['muted'])
            y += 15 * S
    img = img.resize((UIW, UIH), Image.LANCZOS) if S != 1 else img
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)


def cover(path, title, sub):
    S = 2
    W, H = 512 * S, 512 * S
    img = Image.new('RGB', (W, H), C['chassis'])
    d = ImageDraw.Draw(img)
    cols = [C['ukg'], C['mth'], C['sg'], C['ch']]
    for i, c in enumerate(cols):
        d.rectangle((0, H - (i + 1) * 26 * S, W, H - i * 26 * S - 6 * S), fill=c)
    d.rounded_rectangle((36 * S, 40 * S, W - 36 * S, 150 * S), radius=10 * S, fill=C['lcd'], outline='#6f7d57', width=2 * S)
    d.text((56 * S, 58 * S), 'INSTRUMENTS', font=font(40 * S, lcd=True), fill=C['lcdink'])
    d.text((56 * S, 98 * S), '128-140 BPM', font=font(34 * S, lcd=True), fill=C['lcdink'])
    d.text((36 * S, 190 * S), title, font=font(46 * S, 900, 125), fill=C['ink'])
    d.text((36 * S, 250 * S), sub, font=font(20 * S, 500, 100), fill=C['muted'])
    img = img.resize((512, 512), Image.LANCZOS)
    img.save(path, optimize=True)


# ---------------------------------------------------------------- UI building
class UI:
    """Collects tab children in order so control positions (which count labels too) are known."""

    def __init__(self, genre):
        self.items = []
        self.gc = C.get(genre, C['accent'])
        self.knob_xy = []

    def add(self, e):
        self.items.append(e)
        return len(self.items) - 1

    def knob(self, x, y, label, minv, maxv, value, bindings, default=None, vtype='float', w=78, h=92, tip=None, name=None):
        e = el('labeled-knob', dict(x=x, y=y, width=w, height=h, label=label, parameterName=name or label, type='float' if vtype == 'float' else None,
                                     valueType=vtype, minValue=minv, maxValue=maxv, value=value, defaultValue=default if default is not None else value,
                                     textColor=argb(C['ink']), textSize=13, trackForegroundColor=argb(self.gc), trackBackgroundColor=argb(C['line']),
                                     tooltip=tip), bindings)
        return self.add(e)

    def menu(self, x, y, w, h, options, value=1, name=None):
        opts = [el('option', dict(name=n), b) for n, b in options]
        e = el('menu', dict(x=x, y=y, width=w, height=h, value=value, textColor=argb(C['ink']), backgroundColor=argb(C['raised']),
                            highlightedTextColor=argb(C['chassis']), highlightedBackgroundColor=argb(self.gc)), opts)
        return self.add(e)

    def label(self, x, y, w, h, text, size=11, color=None, halign='left'):
        return self.add(el('label', dict(x=x, y=y, width=w, height=h, text=text, textSize=size, textColor=argb(color or C['muted']), hAlign=halign)))


def knob_row(n, x0=24, x1=788, y=118, w=78):
    """Evenly spaced x positions for n knobs between x0 and x1."""
    if n == 1:
        return [int((x0 + x1 - w) / 2)]
    gap = (x1 - x0 - n * w) / (n - 1)
    return [int(round(x0 + i * (w + gap))) for i in range(n)]


def write_preset(path, root):
    ET.indent(root, space='  ')
    xml = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding='unicode') + '\n'
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(xml)
    return xml


# ---------------------------------------------------------------- SFZ
def sfz_line(**op):
    parts = []
    sample = op.pop('sample', None)
    for k, v in op.items():
        if v is None:
            continue
        parts.append('%s=%s' % (k, fmt(v)))
    if sample is not None:
        parts.append('sample=%s' % sample)       # last on the line, so spaces in names can't confuse a parser
    return ' '.join(parts)
