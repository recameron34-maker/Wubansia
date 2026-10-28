"""Real-browser check of the groove-house and tech-house kits: loop playback level and spectrum through an analyser tapped
onto the destination, song start, console errors, and screenshots of both skins on desktop and phone, light and dark."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', '..'))  # the legacy web-pack folder
import asyncio, json, sys, math
sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import harness2
from playwright.async_api import async_playwright
OUT = PACK_ROOT + '/out/shots'
_os.makedirs(OUT, exist_ok=True)
TAP = """
(() => {
  const oc = AudioNode.prototype.connect;
  AudioNode.prototype.connect = function (dst, ...a) {
    const r = oc.call(this, dst, ...a);
    try {
      if (dst instanceof AudioDestinationNode) {
        const ctx = dst.context;
        if (!ctx.__an) { ctx.__an = ctx.createAnalyser(); ctx.__an.fftSize = 8192; ctx.__an.smoothingTimeConstant = 0; window.__an = ctx.__an; window.__ctx = ctx; }
        oc.call(this, ctx.__an);
      }
    } catch (e) {}
    return r;
  };
  window.__level = () => { const an = window.__an; if (!an) return null; const b = new Float32Array(an.fftSize); an.getFloatTimeDomainData(b); let s = 0, pk = 0; for (const x of b) { s += x * x; pk = Math.max(pk, Math.abs(x)); } return [Math.sqrt(s / b.length), pk]; };
  window.__bands = () => { const an = window.__an; if (!an) return null; const b = new Float32Array(an.frequencyBinCount); an.getFloatFrequencyData(b);
    const hz = i => i * an.context.sampleRate / an.fftSize, e = [0, 0, 0, 0];
    b.forEach((db, i) => { const f = hz(i), p = Math.pow(10, db / 10); if (f < 120) e[0] += p; else if (f < 1000) e[1] += p; else if (f < 6000) e[2] += p; else e[3] += p; });
    return e.map(x => 10 * Math.log10(x + 1e-20)); };
})();
"""


async def measure(pg, secs=4.0, step=0.25):
    lv, bands = [], []
    n = int(secs / step)
    for _ in range(n):
        await pg.wait_for_timeout(int(step * 1000))
        l = await pg.evaluate('window.__level && window.__level()')
        b = await pg.evaluate('window.__bands && window.__bands()')
        if l: lv.append(l)
        if b: bands.append(b)
    if not lv:
        return None
    rms = math.sqrt(sum(x[0] ** 2 for x in lv) / len(lv))
    pk = max(x[1] for x in lv)
    bm = [10 * math.log10(sum(10 ** (b[k] / 10) for b in bands) / len(bands)) for k in range(4)] if bands else None
    return rms, pk, bm


async def main():
    harness2.write_preview(); srv = harness2.serve(8771)
    errs = []
    try:
        async with async_playwright() as p:
            b = await p.chromium.launch(args=['--autoplay-policy=no-user-gesture-required'])
            ctx = await b.new_context(viewport={'width': 1280, 'height': 900})
            await ctx.add_init_script(TAP)
            pg = await ctx.new_page()
            pg.on('pageerror', lambda e: errs.append('pageerror: ' + str(e)))
            pg.on('console', lambda m: errs.append(m.type + ': ' + m.text) if m.type in ('error', 'warning') else None)
            await pg.goto('http://127.0.0.1:8771/_preview.html'); await pg.wait_for_timeout(1500)
            await pg.click('.mode[data-view="kits"]'); await pg.wait_for_timeout(200)
            res = {}
            import os
            for kid in os.environ.get('KITS', 'CH01,MTH01,GH01,GV01,GV02,GV03,TH01,TH02,TH03,TH04').split(','):
                await pg.click('#genres [data-genre="all"]'); await pg.wait_for_timeout(150)
                await pg.click(f'[data-kit="{kid}"]'); await pg.wait_for_timeout(250)
                await pg.click('#playBtn'); await pg.wait_for_timeout(1200)   # let the samples load and the loop settle
                m = await measure(pg, 4.0)
                res[kid] = m
                if kid in ('GV01', 'TH03'):
                    await pg.screenshot(path=f'{OUT}/desk_{kid}_playing.png')
                await pg.click('#playBtn'); await pg.wait_for_timeout(300)
                if m:
                    rms, pk, bm = m
                    print(f'{kid}: rms {20 * math.log10(rms + 1e-9):6.1f} dBFS  peak {pk:.2f}  bands(sub/mid/pres/air) ' + ' '.join(f'{x:6.1f}' for x in bm), flush=True)
                else:
                    print(kid, 'no level', flush=True)
            # a full song and an extended mix start and sound
            for kid, v in (('TH01', 'ext'), ('GV03', 'full')):
                await pg.click('#genres [data-genre="all"]'); await pg.wait_for_timeout(150)
                await pg.click(f'[data-kit="{kid}"]'); await pg.wait_for_timeout(250)
                await pg.click(f'[data-songv="{v}"]'); await pg.wait_for_timeout(200)
                await pg.click('#songPlay'); await pg.wait_for_timeout(1500)
                m = await measure(pg, 3.0)
                print(f'{kid} {v} song: ' + (f'rms {20 * math.log10(m[0] + 1e-9):.1f} dBFS peak {m[1]:.2f}' if m else 'silent'), flush=True)
                await pg.click('#songPlay'); await pg.wait_for_timeout(300)
            if os.environ.get('NOSHOTS'):
                await b.close(); return
            # skins: desktop light
            for g in ('gv', 'th'):
                await pg.click(f'#genres [data-genre="{g}"]'); await pg.wait_for_timeout(500)
                await pg.evaluate('window.scrollTo(0,0)')
                await pg.screenshot(path=f'{OUT}/desk_{g}_light.png')
            await ctx.close()
            # dark desktop + phone light/dark
            for scheme, vw, tag in (('dark', 1280, 'desk'), ('light', 360, 'm360'), ('dark', 360, 'm360')):
                c2 = await b.new_context(viewport={'width': vw, 'height': 900 if vw > 400 else 760}, color_scheme=scheme, device_scale_factor=2 if vw < 400 else 1)
                await c2.add_init_script(TAP)
                p2 = await c2.new_page()
                p2.on('pageerror', lambda e: errs.append('pageerror: ' + str(e)))
                await p2.goto('http://127.0.0.1:8771/_preview.html'); await p2.wait_for_timeout(1500)
                await p2.click('.mode[data-view="kits"]'); await p2.wait_for_timeout(200)
                for g in ('gv', 'th'):
                    await p2.click(f'#genres [data-genre="{g}"]'); await p2.wait_for_timeout(500)
                    await p2.evaluate('window.scrollTo(0,0)')
                    await p2.screenshot(path=f'{OUT}/{tag}_{g}_{scheme}.png')
                    if vw < 400 and scheme == 'light':
                        await p2.click('[data-kit="' + ('GV01' if g == 'gv' else 'TH01') + '"]'); await p2.wait_for_timeout(300)
                        await p2.click('#playBtn'); await p2.wait_for_timeout(1500)
                        await p2.screenshot(path=f'{OUT}/{tag}_{g}_playing.png')
                        await p2.click('#playBtn'); await p2.wait_for_timeout(200)
                        sw = await p2.evaluate('document.documentElement.scrollWidth')
                        print(f'{vw}px {g}: scrollWidth {sw}', flush=True)
                if vw < 400 and scheme == 'light':
                    for view in ('bass', 'fx', 'hits'):
                        await p2.click(f'.mode[data-view="{view}"]'); await p2.wait_for_timeout(400)
                        await p2.evaluate('window.scrollTo(0,0)')
                        sw = await p2.evaluate('document.documentElement.scrollWidth')
                        print(f'{vw}px view {view} (th skin): scrollWidth {sw}', flush=True)
                        await p2.screenshot(path=f'{OUT}/{tag}_{view}_th.png')
                await c2.close()
            await b.close()
    finally:
        srv.terminate()
    print('errors:', errs[:8] if errs else 'none')
asyncio.run(main())
