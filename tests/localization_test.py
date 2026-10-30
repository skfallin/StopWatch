"""Verify generated catalogs, argument parity and untranslated protocol boundaries."""
from pathlib import Path
import json
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root/'tools/generate_localizations.py'), '--check'], check=True)
pairs = json.loads((root/'locales/en.json').read_text())
formatting = re.compile(r'%(?:\d+\$)?[-+0 #]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|h|l|z|t|j)?[diuoxXfFeEgGaAcsp@]')
for key, english in pairs.items():
    assert english, key
    assert formatting.findall(key) == formatting.findall(english), key
    assert re.findall(r'\\\([^)]*\)', key) == re.findall(r'\\\([^)]*\)', english), key
    assert sorted(re.findall(r'\{\w+\}', key)) == sorted(re.findall(r'\{\w+\}', english)), key
for folder in ['src', 'include']:
    for path in (root/folder).glob('*'):
        if path.suffix not in ['.cpp', '.h'] or path.name=='i18n.h': continue
        for key in re.findall(r'i18n::text\("([^"\n]*)"\)', path.read_text()):
            assert key in pairs, (path, key)
            assert key not in ['language', 'time_zone', 'home_apps', 'ssid', 'ha_token', 'Reset chiave'], key
portal = (root/'web/portal.html').read_text()
generated = (root/'src/portal_page.h').read_text()
catalog = json.loads(re.search(r'const translations = (.*);', generated)[1])
assert catalog['Nome visualizzato'] == 'Display name'
assert catalog['Dimensione del file inattesa'] == 'Unexpected file size'
assert catalog['Audio scaricato. La copia sullo StopWatch è ancora presente.'] == 'Audio downloaded. The copy on StopWatch is still available.'
assert not re.search(r"(?:new Error|message)\('[^']+" , portal), 'Portal feedback must pass through tr()'
print(f'OK: {len(pairs)} translation pairs, format arguments, portal feedback and stable protocol identifiers')
