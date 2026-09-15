#!/usr/bin/env bash
set -euo pipefail
umask 077
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
# shellcheck source=agent/linux/common.sh
source "$root/agent/linux/common.sh"
[[ $EUID == 0 && -d /sys/firmware/efi ]] || { echo 'Run as root on UEFI Linux' >&2; exit 1; }
rb_require jq curl efibootmgr systemctl
case $(uname -m) in
    x86_64) rid=linux-x64 ;;
    aarch64) rid=linux-arm64 ;;
    *) echo 'Unsupported agent architecture' >&2; exit 1 ;;
esac
agent_binary=${2:-$root/build/agent/$rid/remote-boot-agent}
[[ -f $agent_binary && -x $agent_binary ]] || { echo "Build or download the native agent first: $agent_binary" >&2; exit 1; }
"$agent_binary" --self-test
esp=${1:-}
if [[ -z $esp ]]; then read -r -p 'ESP32 IPv4 address: ' esp; fi
RB_URL="http://$esp"
read -r -s -p 'Senha do agent (8-128 caracteres): ' RB_TOKEN; printf '\n'
rb_api GET systems >/dev/null
read -r -p 'Allow confirmed remote reboot? Type REBOOT: ' reboot_allow
read -r -p 'Allow dashboard/Sinric shutdown? Type SHUTDOWN: ' shutdown_allow
reboot_json=false; [[ $reboot_allow != REBOOT ]] || reboot_json=true
shutdown_json=false; [[ $shutdown_allow != SHUTDOWN ]] || shutdown_json=true
systemctl stop remote-boot.service 2>/dev/null || true
mkdir -p /opt/remote-boot /etc/remote-boot /var/lib/remote-boot
chmod 700 /etc/remote-boot /var/lib/remote-boot
install -m 755 "$agent_binary" /opt/remote-boot/remote-boot-agent.new
mv /opt/remote-boot/remote-boot-agent.new /opt/remote-boot/remote-boot-agent
jq -n --arg url "$RB_URL" --arg token "$RB_TOKEN" --argjson reboot "$reboot_json" --argjson shutdown "$shutdown_json" \
    '{url:$url,token:$token,allow_reboot:$reboot,allow_shutdown:$shutdown}' > /etc/remote-boot/agent.json
chmod 600 /etc/remote-boot/agent.json
install -m 644 "$root/agent/linux/remote-boot.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable remote-boot.service
systemctl restart remote-boot.service
echo 'Agent installed. No EFI files or boot order changed.'
