"""Local preview of the pack page: the publish skeleton, local copies of the Google fonts, a static server."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', '..'))  # the legacy web-pack folder
import re, subprocess, time, os
SITE = PACK_ROOT + '/site'
SKEL = ('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">'
        '<style>:root{color-scheme:light;padding-top:env(safe-area-inset-top);padding-bottom:env(safe-area-inset-bottom)}body{margin:0;font:14px system-ui}img{max-width:100%}[hidden]{display:none!important}</style>'
        '<script>window.claude={use:async n=>n==="downloads"?{save:async()=>({status:"saved"})}:null}</script>')
FONTS = '''<style>
@font-face{font-family:"Instrument Sans";font-style:normal;font-weight:400 700;font-stretch:75% 100%;src:url(_fonts/instrument-sans-latin-standard-normal.woff2) format("woff2")}
@font-face{font-family:"Martian Mono";font-style:normal;font-weight:100 800;font-stretch:75% 112.5%;src:url(_fonts/martian-mono-latin-standard-normal.woff2) format("woff2")}
@font-face{font-family:"Syne";font-style:normal;font-weight:400 800;src:url(_fonts/syne-latin-wght-normal.woff2) format("woff2")}
@font-face{font-family:"Big Shoulders Display";font-style:normal;font-weight:100 900;src:url(_fonts/big-shoulders-display-latin-wght-normal.woff2) format("woff2")}
@font-face{font-family:"Michroma";font-style:normal;font-weight:400;src:url(_fonts/michroma-latin-400-normal.woff2) format("woff2")}
@font-face{font-family:"Archivo";font-style:italic;font-weight:100 900;font-stretch:62% 125%;src:url(_fonts/archivo-latin-standard-italic.woff2) format("woff2")}
@font-face{font-family:"Fraunces";font-style:italic;font-weight:100 900;src:url(_fonts/fraunces-latin-standard-italic.woff2) format("woff2")}
@font-face{font-family:"Unbounded";font-style:normal;font-weight:200 900;src:url(_fonts/unbounded-latin-wght-normal.woff2) format("woff2")}
@font-face{font-family:"Rubik Wet Paint";font-style:normal;font-weight:400;src:url(_fonts/rubik-wet-paint-latin-400-normal.woff2) format("woff2")}
</style>'''


def write_preview():
    page = open(f'{SITE}/index.html').read()
    page = re.sub(r'<link[^>]+fonts\.(googleapis|gstatic)\.com[^>]*>\s*', '', page)
    open(f'{SITE}/_preview.html', 'w').write(SKEL + FONTS + '</head><body>' + page + '</body></html>')


def serve():
    p = subprocess.Popen(['python3', '-m', 'http.server', '8765', '--bind', '127.0.0.1'], cwd=SITE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.8)
    return p
