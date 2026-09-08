#!/usr/bin/env bash
set -euo pipefail
umask 077
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
# shellcheck source=agent/linux/common.sh
source "$root/agent/linux/common.sh"
[[ $EUID == 0 && -d /sys/firmware/efi/efivars ]] || { echo 'Run as root on UEFI Linux' >&2; exit 1; }
rb_require efibootmgr findmnt lsblk jq curl make gcc objcopy git sha256sum
read -r -p 'ESP32 IPv4 address: ' esp
RB_URL="http://$esp"
read -r -s -p 'Administrative token (24+ letters/digits/_/-): ' RB_TOKEN; printf '\n'
rb_api GET status >/dev/null
catalog=$(rb_catalog); jq . <<< "$catalog"
if (( $(jq '.systems|length' <<< "$catalog")>24 )); then echo 'More than 24 entries. Reduce firmware entries before installing.' >&2; exit 1; fi
rb_api POST systems/sync "$catalog"
printf '\nSelect default and fallback in the ESP32 dashboard before testing.\n'
bash "$root/ipxe/build.sh" "$esp"
esp_mount=''
for candidate in /boot/efi /efi /boot; do
    if [[ $(findmnt -rn -M "$candidate" -o FSTYPE || true) == vfat ]]; then
        source_dev=$(findmnt -rn -M "$candidate" -o SOURCE)
        type=$(lsblk -dnro PARTTYPE "$source_dev")
        if [[ ${type,,} == c12a7328-f81f-11d2-ba4b-00a0c93ec93b ]]; then esp_mount=$candidate; break; fi
    fi
done
read -r -p "EFI System Partition mount [$esp_mount]: " selected
esp_mount=${selected:-$esp_mount}
source_dev=$(findmnt -rn -M "$esp_mount" -o SOURCE)
[[ $(findmnt -rn -M "$esp_mount" -o FSTYPE) == vfat ]] || { echo 'ESP must be a mounted FAT filesystem' >&2; exit 1; }
[[ $(lsblk -dnro PARTTYPE "$source_dev" | tr '[:upper:]' '[:lower:]') == c12a7328-f81f-11d2-ba4b-00a0c93ec93b ]] || { echo 'Partition is not a GPT ESP' >&2; exit 1; }
disk="/dev/$(lsblk -dnro PKNAME "$source_dev")"; part=$(lsblk -dnro PARTN "$source_dev")
backup="/var/backups/remote-boot/$(date -u +%Y%m%dT%H%M%S)-$$"
mkdir -p "$backup"
efibootmgr -v > "$backup/efibootmgr.txt"
mkdir -p "$backup/efivars"
for variable in /sys/firmware/efi/efivars/Boot*-8be4df61-93ca-11d2-aa0d-00e098032b8c; do
    [[ ! -f $variable ]] || cp "$variable" "$backup/efivars/"
done
old_order=$(efibootmgr | sed -n 's/^BootOrder: //p'); [[ $old_order =~ ^[0-9A-Fa-f,]+$ ]] || exit 1
printf '%s\n' "$old_order" > "$backup/BootOrder"
if [[ -f $esp_mount/EFI/iPXE/ipxe.efi ]]; then cp "$esp_mount/EFI/iPXE/ipxe.efi" "$backup/ipxe.efi"; fi
printf 'Will install %s/EFI/iPXE/ipxe.efi on %s partition %s. Backup: %s\n' "$esp_mount" "$disk" "$part" "$backup"
read -r -p 'Type INSTALL to write the EFI file and create/reuse the boot entry: ' answer
[[ $answer == INSTALL ]] || exit 0
existing=$(efibootmgr | sed -n 's/^Boot\([0-9A-Fa-f]\{4\}\)\*\? Remote Boot iPXE$/\1/p')
if [[ $(wc -w <<< "$existing") -gt 1 ]]; then echo 'Multiple Remote Boot entries; resolve explicitly before rerun' >&2; exit 1; fi
if [[ -n $existing ]]; then
    printf 'Existing entry Boot%s:\n' "$existing"
    efibootmgr -v | sed -n "/^Boot$existing/p"
    read -r -p 'Confirm this entry points to the selected ESP (type REUSE): ' answer
    [[ $answer == REUSE ]] || exit 0
fi
mkdir -p "$esp_mount/EFI/iPXE"
cp "$root/build/ipxe.efi" "$esp_mount/EFI/iPXE/ipxe.efi.new"
sync
mv "$esp_mount/EFI/iPXE/ipxe.efi.new" "$esp_mount/EFI/iPXE/ipxe.efi"
if [[ -z $existing ]]; then
    # --create-only does not append to BootOrder.
    efibootmgr --create-only --disk "$disk" --part "$part" --label 'Remote Boot iPXE' --loader '\EFI\iPXE\ipxe.efi'
    existing=$(efibootmgr | sed -n 's/^Boot\([0-9A-Fa-f]\{4\}\)\*\? Remote Boot iPXE$/\1/p')
fi
[[ $existing =~ ^[0-9A-Fa-f]{4}$ ]] || { echo 'Cannot identify created entry' >&2; exit 1; }
new_order=$(efibootmgr | sed -n 's/^BootOrder: //p')
[[ $old_order == "$new_order" ]] || { efibootmgr --bootorder "$old_order"; echo 'Unexpected BootOrder change restored' >&2; exit 1; }
printf '%s\n' "$existing" > "$backup/RemoteBootId"
rb_api POST systems/sync "$(rb_catalog)"
printf '\n'
read -r -p 'Set BootNext for a one-time test? Type TEST: ' answer
if [[ $answer == TEST ]]; then efibootmgr --bootnext "$existing"; fi
read -r -p 'Install heartbeat agent? Type AGENT: ' answer
if [[ $answer == AGENT ]]; then
    read -r -s -p 'Agent token from dashboard: ' agent_token; printf '\n'
    read -r -p 'Allow dashboard-confirmed reboot commands? Type REBOOT to enable: ' allow
    allow_json=false; [[ $allow != REBOOT ]] || allow_json=true
    mkdir -p /opt/remote-boot /etc/remote-boot /var/lib/remote-boot
    install -m 755 "$root/agent/linux/agent.sh" "$root/agent/linux/common.sh" /opt/remote-boot/
    jq -n --arg url "$RB_URL" --arg token "$agent_token" --argjson allow "$allow_json" '{url:$url,token:$token,allow_reboot:$allow}' > /etc/remote-boot/agent.json
    install -m 644 "$root/agent/linux/remote-boot.service" /etc/systemd/system/
    systemctl daemon-reload; systemctl enable --now remote-boot.service
fi
printf 'No reboot performed. Test manually, then run installer/linux/promote.sh %s after confirming successful boot.\n' "$existing"
