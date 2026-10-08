"""Run installer with every OS action replaced and all writes inside a temporary tree."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--bash', default='bash')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
source = (repo / 'installer/linux/install-agent.sh').read_text()


def shell_path(path):
    value = str(path).replace('\\', '/')
    return '/' + value[0].lower() + value[2:] if len(value) > 1 and value[1] == ':' else value


for scenario in ('approved', 'cancel', 'check-failure'):
    with tempfile.TemporaryDirectory(prefix='rb-installer-') as temporary:
        sandbox = Path(temporary)
        root = shell_path(sandbox)
        tools = sandbox / 'tools'
        tools.mkdir()
        events = sandbox / 'events'
        environment = dict(os.environ, FIXTURE_ROOT=root, FIXTURE_SCENARIO=scenario)
        def executable(name, text):
            path = tools / name
            path.write_bytes(('#!/usr/bin/env bash\nset -euo pipefail\n' + text).encode())
            path.chmod(0o755)
            return path
        executable('efibootmgr', 'exit 0\n')
        executable('chown', 'echo protect >> "$FIXTURE_ROOT/events"\n')
        executable('systemctl', 'echo "systemctl $*" >> "$FIXTURE_ROOT/events"\nif [[ ${1:-} == enable ]]; then test -f "$FIXTURE_ROOT/checked"; fi\n')
        fake_agent = executable('agent', '''echo "agent $*" >> "$FIXTURE_ROOT/events"
if [[ $* == *--pair* ]]; then
    grep -q protect "$FIXTURE_ROOT/events"
    [[ $FIXTURE_SCENARIO != cancel ]] || exit 1
    while [[ $1 != --config ]]; do shift; done
    printf '{"fixture":true}\\n' > "$2"
    [[ $FIXTURE_SCENARIO != check-failure ]] || exit 1
    touch "$FIXTURE_ROOT/checked"
elif [[ $* == *--check* ]]; then
    [[ $FIXTURE_SCENARIO != check-failure ]] || exit 1
    touch "$FIXTURE_ROOT/checked"
fi
''')
        environment['PATH'] = shell_path(tools) + ':' + environment['PATH']
        # Replace only root requirements and external system paths; production control flow stays intact.
        code = source.replace('umask 077', 'umask 077\nexport PATH=' + shlex.quote(shell_path(tools)) + ':$PATH')
        code = code.replace('[[ $EUID == 0 && -d /sys/firmware/efi ]]', '[[ 1 == 1 ]]')
        code = code.replace('root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)', 'root=' + shlex.quote(shell_path(repo)))
        for system_path in ('/opt/remote-boot', '/etc/remote-boot', '/var/lib/remote-boot', '/etc/systemd/system'):
            code = code.replace(system_path, root + system_path)
        (sandbox / 'etc/systemd/system').mkdir(parents=True)
        fixture = sandbox / 'installer.sh'
        fixture.write_bytes(code.encode())
        result = subprocess.run([args.bash, shell_path(fixture), '127.0.0.1', shell_path(fake_agent)],
                                input='\n\n', text=True, capture_output=True, env=environment, timeout=20)
        trace = events.read_text() if events.exists() else ''
        installed = (sandbox / 'etc/remote-boot/agent.json').exists()
        assert (result.returncode == 0) == (scenario == 'approved'), (scenario, result.stdout, result.stderr)
        assert installed == (scenario == 'approved'), scenario
        assert ('systemctl enable' in trace) == (scenario == 'approved'), (scenario, trace)
        assert not (sandbox / 'etc/remote-boot/agent.pending.json').exists(), scenario
        print('PASS: Linux installer fixture ' + scenario)
