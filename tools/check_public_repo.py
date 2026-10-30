"""Check working files and reachable Git history for high-confidence secret patterns.

Reports locations only. This is a release guard, not a complete security audit.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PATTERN = re.compile(rb'(?:-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{60,}|AKIA[A-Z0-9]{16})')

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)

def main():
    findings = []
    paths = git('ls-files', '-z', '--cached', '--others', '--exclude-standard').split(b'\0')
    for raw in set(paths):
        if not raw: continue
        name = raw.decode('utf-8')
        path = ROOT / name
        if not path.is_file(): continue
        if PATTERN.search(path.read_bytes()): findings.append('working tree: ' + name)
    # Include old/deleted blobs: deleting a secret in the latest tree is insufficient.
    objects = git('rev-list', '--objects', '--all').splitlines()
    for item in objects:
        fields = item.split(b' ', 1)
        if len(fields) < 2: continue
        oid, name = fields
        if git('cat-file', '-t', oid.decode()).strip() != b'blob': continue
        if PATTERN.search(git('cat-file', 'blob', oid.decode())):
            findings.append('history: ' + oid.decode()[:12] + ' ' + name.decode('utf-8', errors='replace'))
    if findings:
        raise SystemExit('Potential secrets found (values redacted):\n' + '\n'.join(findings))
    print('No high-confidence secrets found in working files or reachable Git blobs. Review personal data and service-specific tokens separately.')

if __name__ == '__main__':
    main()
