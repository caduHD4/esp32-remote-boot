#!/usr/bin/env python3
"""Run builder with local Git source and a build-boundary stand-in.

The stand-in avoids compiling upstream per test; real builds/QEMU cover that
boundary separately. Source mutations are observable through actual Git state.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
PIN = '7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1'
class Builder(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.d = Path(self.tmp.name)
        self.root = self.d/'project'; self.root.mkdir()
        shutil.copytree(ROOT/'ipxe', self.root/'ipxe')
        shutil.copytree(ROOT/'scripts', self.root/'scripts')
        (self.root/'uefi/remote-boot').mkdir(parents=True)
        (self.root/'uefi/remote-boot/Makefile').write_text('all:\n\ttrue\n')
        (self.root/'uefi/remote-boot/remote_boot.c').write_text('fixture')
        (self.root/'uefi/remote-boot/load_option.h').write_text('fixture')
        self.src = self.d/'upstream'; self.src.mkdir()
        self.git = shutil.which('git')
        def git(*args): return subprocess.check_output([self.git,'-C',str(self.src),*args],stderr=subprocess.DEVNULL).decode().strip()
        self.gitrun = git
        git('init'); (self.src/'src/config').mkdir(parents=True)
        (self.src/'src/config/config.c').write_text('upstream\n')
        for name in ['usr','hci/commands','include/usr']:
            path=self.src/'src'/name;path.mkdir(parents=True,exist_ok=True);(path/'.keep').touch()
        git('add','.'); git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','-m','test')
        rev = git('rev-parse','HEAD')
        (self.src/'src/config/config.c').write_text('LOCAL DIRTY MUST SURVIVE\n')
        (self.src/'untracked').write_text('preserve')
        self.before = git('status','--porcelain')
        unrelated = self.root/'.pio/ipxe-rbdiscover'; unrelated.mkdir(parents=True)
        (unrelated/'sentinel').write_text('MUST SURVIVE')
        self.bin = self.d/'bin'; self.bin.mkdir()
        (self.bin/'git').write_text('#!/usr/bin/env python3\nimport os,sys\na=["'+rev+'" if x.startswith("'+PIN+'") else x for x in sys.argv[1:]]\nos.execv('+repr(self.git)+',['+repr(self.git)+']+a)\n')
        (self.bin/'make').write_text('''#!/usr/bin/env python3
import pathlib,sys,os,json
args=sys.argv[1:]; d=pathlib.Path(args[args.index('-C')+1])
if (d/'remote_boot.c').exists():
 (d/'RemoteBoot.efi').write_bytes(b'loader-fixture')
 if os.environ.get('RB_TEST_MUTATE_PROFILE'):
  p=pathlib.Path(os.environ['RB_TEST_MUTATE_PROFILE']);v=json.loads(p.read_text());v['mode_revision']=2;p.write_text(json.dumps(v))
else:
 p=d/'bin-x86_64-efi/ipxe.efi'; p.parent.mkdir(parents=True,exist_ok=True)
 embed=next(x[6:] for x in args if x.startswith('EMBED='))
 p.write_bytes(b'EFI-fixture'+b''.join(pathlib.Path(x).read_bytes() for x in embed.split(',')))
''')
        for f in self.bin.iterdir(): f.chmod(0o755)
        self.env = dict(os.environ,PATH=str(self.bin)+':'+os.environ['PATH'],IPXE_SOURCE=str(self.src),TMPDIR=str(self.d))
        self.profile = json.loads((ROOT/'tests/fixtures/discovery-v1.json').read_text())['profile']
    def tearDown(self): self.tmp.cleanup()
    def run_build(self, profile=None):
        out = self.d/'output with spaces'
        args = ['bash',str(self.root/'ipxe/build.sh'),'192.168.50.80',self.profile['pc_id'],str(out),'1000']
        if profile is not None:
            p=self.d/'profile.json';p.write_text(json.dumps(profile));args.append(str(p))
        run = subprocess.run(args,env=self.env,text=True,capture_output=True)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        self.assertEqual(self.gitrun('status','--porcelain'),self.before)
        self.assertEqual((self.root/'.pio/ipxe-rbdiscover/sentinel').read_text(),'MUST SURVIVE')
        return out,json.loads((out/'manifest.json').read_text())
    def test_legacy_four_arguments_build_without_source_mutation(self):
        out, manifest = self.run_build()
        self.assertEqual(set(manifest),{'esp_ipv4','pc_id','ipxe_sha256','loader_sha256'})
        self.assertNotIn('rbdiscover', (out/'remote-boot.ipxe').read_text())
    def test_blind_manifest_and_pins_without_source_mutation(self):
        out, manifest = self.run_build(self.profile)
        self.assertTrue(manifest['blind_mode'])
        self.assertEqual(manifest['device_id'],self.profile['device_id'])
        self.assertIn('rbdiscover '+self.profile['device_id'],(out/'remote-boot.ipxe').read_text())
        import hashlib
        for filename,key in [('ipxe.efi','ipxe_sha256'),('RemoteBoot.efi','loader_sha256')]:
            self.assertEqual(hashlib.sha256((out/filename).read_bytes()).hexdigest(),manifest[key])
    def test_profile_is_snapshotted_before_build(self):
        self.env['RB_TEST_MUTATE_PROFILE']=str(self.d/'profile.json')
        out,manifest=self.run_build(self.profile)
        self.assertEqual(manifest['mode_revision'],1)
        self.assertEqual(json.loads((self.d/'profile.json').read_text())['mode_revision'],2)
    def test_normal_profile_keeps_static_mode(self):
        self.profile['blind_mode']=False
        out,manifest=self.run_build(self.profile)
        self.assertFalse(manifest['blind_mode'])
        self.assertNotIn('rbdiscover', (out/'remote-boot.ipxe').read_text())
if __name__ == '__main__': unittest.main()
