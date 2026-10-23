#!/usr/bin/env python3
"""Validate the shipped single executable, without source tree or sidecars."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--runner');args=p.parse_args()
source=args.binary.resolve(strict=True)
with tempfile.TemporaryDirectory(prefix='rb-standalone-') as temporary:
    target=Path(temporary)/source.name;shutil.copy2(source,target)
    command=([args.runner] if args.runner else [])+[str(target),'--self-test']
    result=subprocess.run(command,cwd=temporary,text=True,capture_output=True,timeout=90)
    if result.returncode:
        print(result.stdout);print(result.stderr);raise SystemExit(result.returncode)
    assert 'PASS: shared signed discovery fixture' in result.stdout,result.stdout
    print('PASS: standalone executable self-test without source tree or fixture sidecar')
