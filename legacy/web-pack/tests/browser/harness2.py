"""Local preview of the pack page outside the site folder: the publish skeleton, local copies of the Google fonts, the site's
audio folder linked in, a static server. Nothing is written into the site, so nothing needs cleaning up before a publish."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', '..'))  # the legacy web-pack folder
import re, subprocess, time, os
SITE = PACK_ROOT + '/site'
PREVIEW = PACK_ROOT + '/out/preview'
import harness


def write_preview():
    os.makedirs(PREVIEW, exist_ok=True)
    for name, target in (('audio', f'{SITE}/audio'), ('_fonts', os.path.join(os.path.dirname(os.path.abspath(__file__)), '_fonts'))):
        if not os.path.lexists(os.path.join(PREVIEW, name)):
            os.symlink(target, os.path.join(PREVIEW, name))
    page = open(f'{SITE}/index.html').read()
    page = re.sub(r'<link[^>]+fonts\.(googleapis|gstatic)\.com[^>]*>\s*', '', page)
    open(f'{PREVIEW}/_preview.html', 'w').write(harness.SKEL + harness.FONTS + '</head><body>' + page + '</body></html>')


def serve(port=8765):
    p = subprocess.Popen(['python3', '-m', 'http.server', str(port), '--bind', '127.0.0.1'], cwd=PREVIEW, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.8)
    return p
