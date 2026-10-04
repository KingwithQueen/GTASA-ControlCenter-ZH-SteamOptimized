"""Verify the recorded backup file hashes; no compilation or game access."""
from pathlib import Path
import hashlib,sys
root=Path(__file__).resolve().parents[1]
failures=[];count=0
for line in (root/'FILES.sha256').read_text(encoding='utf8').splitlines():
    expected,relative=line.split('  ',1)
    target=(root/relative).resolve()
    if root not in target.parents:
        failures.append('Unsafe manifest path: '+relative);continue
    if not target.is_file():
        failures.append('Missing: '+relative);continue
    if hashlib.sha256(target.read_bytes()).hexdigest()!=expected:
        failures.append('Changed: '+relative)
    count+=1
if failures:
    print('\n'.join(failures));sys.exit(1)
print(f'PASS: {count} recorded files verified. FILES.sha256 does not hash itself.')
