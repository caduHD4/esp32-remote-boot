#!/usr/bin/env bash
set -euo pipefail
umask 077
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
[[ $EUID == 0 && -d /sys/firmware/efi ]] || { echo 'Run as root on UEFI Linux' >&2; exit 1; }
for tool in efibootmgr systemctl install; do command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 1; }; done
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
read -r -p 'Allow confirmed remote reboot? Type REBOOT: ' reboot_allow
read -r -p 'Allow dashboard/Sinric shutdown? Type SHUTDOWN: ' shutdown_allow
mkdir -p /opt/remote-boot /etc/remote-boot /var/lib/remote-boot
chown root:root /opt/remote-boot /etc/remote-boot /var/lib/remote-boot
chmod 755 /opt/remote-boot
chmod 700 /etc/remote-boot /var/lib/remote-boot
pending=/etc/remote-boot/agent.pending.json
staged_binary=/opt/remote-boot/remote-boot-agent.pending
trap 'rm -f -- "$pending" "$staged_binary"' EXIT
install -m 755 "$agent_binary" "$staged_binary"
systemctl stop remote-boot.service 2>/dev/null || true
pair_args=(--pair --url "http://$esp" --config "$pending")
[[ $reboot_allow != REBOOT ]] || pair_args+=(--allow-reboot)
[[ $shutdown_allow != SHUTDOWN ]] || pair_args+=(--allow-shutdown)
echo 'Open the dashboard pairing window, then approve the short code for the intended PC.'
"$staged_binary" "${pair_args[@]}"
chmod 600 "$pending"
mv -f -- "$pending" /etc/remote-boot/agent.json
mv -f -- "$staged_binary" /opt/remote-boot/remote-boot-agent
install -m 644 "$root/agent/linux/remote-boot.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable remote-boot.service
systemctl restart remote-boot.service
echo 'Agent paired and installed. No EFI files or boot order changed.'
