#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=agent/linux/common.sh
source "$base/common.sh"
rb_require jq curl efibootmgr systemctl
cfg=${1:-/etc/remote-boot/agent.json}
[[ -r $cfg && -d /sys/firmware/efi ]] || exit 1
RB_URL=$(jq -er .url "$cfg"); RB_TOKEN=$(jq -er .token "$cfg")
allow_reboot=$(jq -r '.allow_reboot//false' "$cfg")
generation=0; last_sync=0; ack=''; pending_ack=''
if [[ -r /var/lib/remote-boot/ack ]]; then ack=$(cat /var/lib/remote-boot/ack); fi
while true; do
    os_name=$(sed -n 's/^PRETTY_NAME=//p' /etc/os-release | tr -d '"')
    uptime_s=$(cut -d. -f1 /proc/uptime)
    boot_id=$(efibootmgr | sed -n 's/^BootCurrent: //p')
    if ! rb_target_exists "$boot_id"; then boot_id=''; fi
    payload=$(jq -cn --arg host "$(hostname)" --arg os "$os_name" --arg boot "$boot_id" --arg ack "$ack" --argjson uptime "$uptime_s" --argjson reboot "$allow_reboot" '{hostname:$host,os:$os,boot_id:$boot,uptime:$uptime,ack:$ack,reboot_enabled:$reboot}')
    if reply=$(rb_api POST heartbeat "$payload"); then
        new_generation=$(jq -r '.discovery_generation//0' <<< "$reply")
        if [[ $generation != "$new_generation" ]] || ((SECONDS-last_sync>=300)); then
            if rb_api POST systems/sync "$(rb_catalog)" >/dev/null; then generation=$new_generation; last_sync=$SECONDS; fi
        fi
        command_id=$(jq -r '.command.id//empty' <<< "$reply")
        target=$(jq -r '.command.boot_id//empty' <<< "$reply")
        action=$(jq -r '.command.action//empty' <<< "$reply")
        if [[ $allow_reboot == true && $action == reboot && -n $command_id && $command_id != "$ack" && $command_id != "$pending_ack" ]] && rb_target_exists "$target"; then
            if efibootmgr --bootnext "$target"; then
                pending_ack=$command_id
                mkdir -p /var/lib/remote-boot
                printf '%s' "$command_id" > /var/lib/remote-boot/ack
                ack=$command_id
                if rb_api POST heartbeat "$(jq -c --arg ack "$ack" '.ack=$ack' <<< "$payload")" >/dev/null; then
                    systemctl reboot
                else
                    efibootmgr --delete-bootnext
                    echo 'Reboot canceled: acknowledgment failed' >&2
                fi
            fi
        fi
    fi
    sleep 12
done
