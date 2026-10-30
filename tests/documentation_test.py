"""Catch broken repository-relative documentation links before a public release."""
from pathlib import Path
import re
from urllib.parse import unquote

root = Path(__file__).resolve().parents[1]
files = [*root.glob('*.md'), *(root/'docs').glob('*.md'), *(root/'ios').glob('README*.md'), *(root/'home_assistant').glob('README*.md')]
for path in files:
    for target in re.findall(r'\]\(([^\s)]+)', path.read_text()):
        if target.startswith(('http:', 'https:', 'mailto:', '#')): continue
        relative = unquote(target.split('#', 1)[0])
        assert (path.parent / relative).exists(), (path.relative_to(root), target)
print(f'OK: repository-relative links in {len(files)} documents')
