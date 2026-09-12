import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
patterns=[r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',r'gh[pousr]_[A-Za-z0-9]{30,}',r'AKIA[A-Z0-9]{16}']
failed=[]
for p in root.rglob('*'):
    if not p.is_file() or any(x in p.parts for x in ('.git','.pio','.cache','build','dist','__pycache__','managed_components')):continue
    if p.suffix in ('.efi','.bin','.o','.so','.zip'):continue
    text=p.read_text(errors='ignore')
    for pattern in patterns:
        if re.search(pattern,text):failed.append(str(p.relative_to(root)))
if failed:raise SystemExit('Potential secrets: '+', '.join(failed))
print('PASS: simple secret pattern scan (not a guarantee)')
