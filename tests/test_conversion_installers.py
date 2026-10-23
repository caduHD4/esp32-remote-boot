"""Installer control flow; every firmware/service/network command is replaced in a temp tree.

Catches writes before confirmation/validation, lost credentials on upgrade, false
success on stale preparation reports, and rollback touching unrelated boot state.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
KEY = '04' + '6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296' + '4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5'
PROFILE = dict(profile_version=1, protocol=2, discovery_version=1, pc_id='a'*32,
               blind_mode=True, mode_revision=2, device_id='b'*32, public_key=KEY,
               fingerprint=hashlib.sha256(bytes.fromhex(KEY)).hexdigest(), last_ipv4='192.168.1.30',
               http_port=80, ws_port=81, hostname='remote-boot-bbbbbbbbbbbb.local')


def run(scenario):
    with tempfile.TemporaryDirectory(prefix='rb-convert-') as temporary:
        box = Path(temporary)
        root = box / 'repo'
        for folder in ('installer', 'agent/linux', 'scripts'):
            shutil.copytree(REPO / folder, root / folder)
        for file in (root / 'installer/linux').glob('*.sh'):
            code = file.read_text().replace('[[ $EUID == 0 && -d /sys/firmware/efi/efivars ]]', '[[ 1 == 1 ]]')
            file.write_text(code)
        tools = box / 'tools'; tools.mkdir()
        efi = box / 'esp/EFI/iPXE'; efi.mkdir(parents=True)
        (efi / 'ipxe.efi').write_bytes(b'old EFI')
        (efi / 'manifest.json').write_bytes(b'old manifest')
        config = box / 'agent.json'
        original = dict(protocol=2, pc_id='a'*32, agent_id='c'*32, token='fixture-token-not-real',
                        url='http://192.168.1.20', allow_reboot=True, allow_shutdown=False)
        config.write_text(json.dumps(original)); config.chmod(0o600)
        if scenario == 'wrong-pin': original.update(device_id='e'*32)
        if scenario == 'wrong-key-only': original.update(public_key='04'+'0'*128)
        if scenario == 'normal': original.update(blind_mode=True, mode_revision=2, device_id=PROFILE['device_id'], public_key=KEY, fingerprint=PROFILE['fingerprint'], discovery_version=1)
        config.write_text(json.dumps(original))
        old = config.read_bytes()
        profile = PROFILE.copy()
        if scenario == 'normal': profile.update(blind_mode=False, mode_revision=3)
        if scenario == 'wrong-pc': profile['pc_id'] = 'd'*32
        profile_file = box / 'profile.json'; profile_file.write_text(json.dumps(profile))
        out = box / 'build'; out.mkdir()
        (out / 'ipxe.efi').write_bytes(b'new EFI'); (out / 'RemoteBoot.efi').write_bytes(b'new loader')
        manifest = {k: profile[k] for k in ('profile_version', 'blind_mode', 'mode_revision', 'device_id', 'public_key', 'fingerprint', 'discovery_version')}
        manifest.update(esp_ipv4=profile['last_ipv4'], pc_id=profile['pc_id'],
                        ipxe_sha256=hashlib.sha256(b'new EFI').hexdigest(), loader_sha256=hashlib.sha256(b'new loader').hexdigest())
        if scenario == 'wrong-mode': manifest['blind_mode'] = False
        if scenario == 'wrong-revision': manifest['mode_revision'] = 55
        if scenario == 'wrong-device': manifest['device_id'] = 'e'*32
        if scenario == 'bad-hash': manifest['ipxe_sha256'] = '0'*64
        (out / 'manifest.json').write_text(json.dumps(manifest))
        (root / 'ipxe').mkdir()
        (root / 'ipxe/build.sh').write_text('#!/bin/bash\nexit 0\n')
        def exe(name, body):
            p = tools / name; p.write_text('#!/bin/bash\nset -eu\n' + body); p.chmod(0o755); return p
        exe('findmnt', 'if [[ "$*" == *FSTYPE* ]]; then echo vfat; else echo /dev/fixture1; fi\n')
        exe('lsblk', 'case "$*" in *PARTTYPE*) echo c12a7328-f81f-11d2-ba4b-00a0c93ec93b;; *PKNAME*) echo fixture;; *PARTN*) echo 1;; *PARTUUID*) echo aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa;; esac\n')
        exe('efibootmgr', '''if [[ "$*" == *--* ]]; then echo "efi $*" >> "$FIXTURE_ROOT/events"; exit 0; fi
printf 'BootOrder: 0001,0002\nBoot0001* Linux\nBoot0002* Remote Boot iPXE\tHD(fixture)\n'
''')
        exe('systemctl', '''echo "service $*" >> "$FIXTURE_ROOT/events"
if [[ ${1:-} == is-active && $FIXTURE_SCENARIO == inactive ]]; then exit 3; fi
''')
        exe('mv', '''if [[ $FIXTURE_SCENARIO == efi-fail && $* == *ipxe.efi.new* ]]; then exit 8; fi
exec /usr/bin/mv "$@"
''')
        agent_body = '''echo "agent $*" >> "$FIXTURE_ROOT/events"
if [[ $* == *--self-test* ]]; then exit 0; fi
if [[ $* == *--apply-profile* ]]; then
  [[ $FIXTURE_SCENARIO != apply-fail ]] || exit 4
  python3 - "$@" <<'AGENT'
import sys,json
args=sys.argv[1:]; p=json.load(open(args[args.index('--apply-profile')+1])); c=args[args.index('--config')+1]
d=json.load(open(c));d.update({k:p[k] for k in ('blind_mode','mode_revision','device_id','public_key','fingerprint','discovery_version')});d['url']='http://'+p['last_ipv4'];open(c,'w').write(json.dumps(d))
AGENT
fi
if [[ $* == *--report-preparation* && $FIXTURE_SCENARIO == stale ]]; then exit 5; fi
'''
        candidate = exe('candidate', agent_body)
        installed = box / 'installed-agent'; installed.write_text('#!/bin/bash\nexit 9\n'); installed.chmod(0o755)
        old_binary = installed.read_bytes()
        env = dict(os.environ, PATH=str(tools)+':'+os.environ['PATH'], FIXTURE_ROOT=str(box), FIXTURE_SCENARIO=scenario,
                   RB_AGENT_CONFIG=str(config), RB_AGENT_BINARY=str(installed), RB_BACKUP_ROOT=str(box / 'backups'), RB_BUILD_DIR=str(out))
        answers = 'TRUST\n' + str(box / 'esp') + '\nINSTALL\nREUSE\n\n'
        if scenario == 'cancel-trust': answers = 'NO\n'
        if scenario == 'cancel-install': answers = 'TRUST\n' + str(box / 'esp') + '\nNO\n'
        command = ['bash', str(root / 'installer/linux/install.sh'), '--discovery-profile', str(profile_file), '--agent-file', str(candidate)]
        result = subprocess.run(command, input=answers, text=True, capture_output=True, env=env, timeout=25)
        trace = (box / 'events').read_text() if (box / 'events').exists() else ''
        detail = (scenario, result.returncode, result.stdout, result.stderr, trace)
        safe = scenario.startswith('wrong-') or scenario in ('bad-hash', 'cancel-trust', 'cancel-install')
        if safe:
            assert config.read_bytes() == old and installed.read_bytes() == old_binary and (efi / 'ipxe.efi').read_bytes() == b'old EFI', detail
            assert '--apply-profile' not in trace and 'service stop' not in trace and 'efi --' not in trace, detail
            assert (result.returncode == 0) == scenario.startswith('cancel-'), detail
        else:
            assert (result.returncode != 0) == (scenario in ('stale', 'apply-fail', 'efi-fail')), detail
            backup = next((box / 'backups').iterdir())
            assert (backup / 'agent.json').read_bytes() == old, detail
            assert (backup.stat().st_mode & 0o077) == 0, detail
            if scenario not in ('apply-fail', 'efi-fail'):
                assert (config.stat().st_mode & 0o777) == 0o600, detail
                current = json.loads(config.read_text())
                for k in ('pc_id', 'agent_id', 'token', 'allow_reboot', 'allow_shutdown'): assert current[k] == original[k], detail
                assert current['blind_mode'] is profile['blind_mode'], detail
                assert (efi / 'ipxe.efi').read_bytes() == b'new EFI', detail
                assert json.loads((efi / 'manifest.json').read_text()) == manifest, detail
                assert '--report-preparation' in trace and '--pair' not in trace, detail
                if scenario == 'stale': assert 'pending' in result.stdout.lower()+result.stderr.lower(), detail
            if scenario == 'rerun':
                again = subprocess.run(command, input=answers, text=True, capture_output=True, env=env, timeout=25)
                assert again.returncode == 0, (again.stdout, again.stderr)
            if scenario == 'inactive': assert 'service stop' not in trace and 'service start' not in trace, detail
            if scenario == 'success':
                state_file = backup / 'state.json'; state = json.loads(state_file.read_text()); saved = state.copy(); state['partuuid'] = 'ffffffff-ffff-ffff-ffff-ffffffffffff'; state_file.write_text(json.dumps(state))
                refused = subprocess.run(['bash', str(root / 'installer/linux/install.sh'), '--rollback', str(backup)], input='RESTORE\n', text=True, capture_output=True, env=env, timeout=20)
                assert refused.returncode != 0 and (efi / 'ipxe.efi').read_bytes() == b'new EFI', (refused.stdout, refused.stderr)
                state_file.write_text(json.dumps(saved))
            rolled = subprocess.run(['bash', str(root / 'installer/linux/install.sh'), '--rollback', str(backup)], input='RESTORE\n', text=True, capture_output=True, env=env, timeout=20)
            assert rolled.returncode == 0, (detail, rolled.stdout, rolled.stderr)
            assert config.read_bytes() == old and installed.read_bytes() == old_binary and (efi / 'ipxe.efi').read_bytes() == b'old EFI' and (efi / 'manifest.json').read_bytes() == b'old manifest', detail
            trace = (box / 'events').read_text()
            assert 'efi --bootorder' not in trace and 'efi --bootnext' not in trace, detail
        print('PASS Linux conversion:', scenario)


if __name__ == '__main__':
    for case in ('success', 'normal', 'rerun', 'inactive', 'wrong-pc', 'wrong-mode', 'wrong-revision', 'wrong-device', 'wrong-pin', 'wrong-key-only', 'bad-hash', 'cancel-trust', 'cancel-install', 'apply-fail', 'efi-fail', 'stale'):
        run(case)
