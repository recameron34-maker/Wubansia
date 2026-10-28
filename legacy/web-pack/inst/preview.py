"""Rough composite of a preset's skin with its controls drawn where Decent Sampler would put them (for layout checks)."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import sys, os
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw
sys.path.insert(0, PACK_ROOT + '/inst')
from dslib import font, C

def preview(dspreset, out):
    root = ET.parse(dspreset).getroot()
    ui = root.find('ui')
    base = os.path.dirname(dspreset)
    img = Image.open(os.path.join(base, ui.get('bgImage'))).convert('RGB').resize((812 * 2, 375 * 2))
    d = ImageDraw.Draw(img)
    S = 2
    for e in ui.find('tab'):
        x, y, w, h = [float(e.get(k, 0)) * S for k in ('x', 'y', 'width', 'height')]
        if e.tag == 'labeled-knob':
            lab = e.get('label')
            d.text((x + w / 2, y + 2 * S), lab, font=font(13 * S, 600), fill=C['ink'], anchor='mt')
            r = min(w, h - 34 * S) / 2
            cx, cy = x + w / 2, y + 20 * S + r + 4 * S
            d.ellipse((cx - r, cy - r, cx + r, cy + r), outline='#3b3a35', width=3 * S)
            d.arc((cx - r, cy - r, cx + r, cy + r), 135, 135 + 270 * 0.6, fill=e.get('trackForegroundColor', 'FFD8492F')[2:] and '#' + e.get('trackForegroundColor')[2:], width=3 * S)
            d.text((cx, y + h - 4 * S), e.get('value'), font=font(10 * S, 500), fill=C['muted'], anchor='mb')
        elif e.tag == 'menu':
            d.rounded_rectangle((x, y, x + w, y + h), radius=4 * S, fill='#2a2926', outline='#3b3a35', width=S)
            opts = e.findall('option')
            v = int(e.get('value', 1))
            d.text((x + 8 * S, y + h / 2), opts[v - 1].get('name') + '  ▾', font=font(12 * S, 600), fill=C['ink'], anchor='lm')
        elif e.tag == 'label':
            d.text((x, y), e.get('text'), font=font(int(float(e.get('textSize', 12))) * S, 500), fill=C['muted'])
    # keyboard strip (what DS shows under the UI)
    kb = Image.new('RGB', (812 * 2, 60 * 2), '#111110')
    img2 = Image.new('RGB', (812 * 2, (375 + 60) * 2))
    img2.paste(img, (0, 0)); img2.paste(kb, (0, 375 * 2))
    img2.resize((812, 435), Image.LANCZOS).save(out)

if __name__ == '__main__':
    import glob
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    for p in sorted(glob.glob(PACK_ROOT + '/inst/lib/*.dspreset')):
        preview(p, os.path.join(outdir, os.path.basename(p).replace('.dspreset', '.png')))
    print('ok')
