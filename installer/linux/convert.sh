#!/usr/bin/env bash
# Local assisted conversion. Never pair, enable services, promote BootOrder or reboot.
set -euo pipefail
umask 077
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
source "$root/agent/linux/common.sh"
source "$root/installer/linux/efi_helpers.sh"
[[ $EUID == 0 && -d /sys/firmware/efi/efivars ]] || { echo 'Run as root on UEFI Linux' >&2; exit 1; }
rb_require efibootmgr findmnt lsblk jq python3 sha256sum systemctl
config=${RB_AGENT_CONFIG:-/etc/remote-boot/agent.json}
binary=${RB_AGENT_BINARY:-/opt/remote-boot/remote-boot-agent}
profile='' candidate='' restore='' requested=''
while (($#)); do
    case $1 in
        --discovery-profile|--agent-file|--rollback)
            (($# >= 2)) || { echo "Missing value for $1" >&2; exit 1; }
            case $1 in --discovery-profile) profile=$2;; --agent-file) candidate=$2;; --rollback) restore=$2;; esac; shift 2;;
        --*) echo "Unknown option: $1" >&2; exit 1;;
        *) [[ -z $requested ]] || { echo 'Only one optional ESP IPv4 is accepted' >&2; exit 1; }; requested=$1; shift;;
    esac
done
# Only the selected files are restored; firmware BootOrder/BootNext and other entries
# are never imported from a snapshot. Snapshot files are for inspection only.
restore_files() {
    local b=$1 c bin target item mount uuid
    c=$(jq -er '.config' "$b/state.json"); bin=$(jq -er '.binary' "$b/state.json")
    mount=$(jq -er '.esp_mount' "$b/state.json"); uuid=$(jq -er '.partuuid' "$b/state.json")
    [[ $(findmnt -rn -M "$mount" -o FSTYPE) == vfat && $(lsblk -dnro PARTUUID "$(findmnt -rn -M "$mount" -o SOURCE)") == "$uuid" ]] || { echo 'Original ESP identity unavailable; refusing rollback.' >&2; return 1; }
    target=$(jq -er '.target' "$b/state.json")
    [[ -d $(dirname -- "$target") && ! -L $c && ! -L $bin ]] || { echo 'Rollback paths unavailable or symlinked; mount the original ESP first.' >&2; return 1; }
    cp -p -- "$b/agent.json" "$c.restore"; chmod "$(jq -r '.config_mode' "$b/state.json")" "$c.restore"; mv -f -- "$c.restore" "$c"
    cp -p -- "$b/agent.bin" "$bin.restore"; chmod "$(jq -r '.binary_mode' "$b/state.json")" "$bin.restore"; mv -f -- "$bin.restore" "$bin"
    for item in ipxe.efi manifest.json; do
        if [[ -f $b/$item ]]; then cp -p -- "$b/$item" "$target/$item.restore"; mv -f -- "$target/$item.restore" "$target/$item";
        else rm -f -- "$target/$item"; fi
        rm -f -- "$target/$item.new"
    done
    # A newly created entry can only be removed if it is unchanged and unreferenced.
    if [[ -f $b/created-entry ]]; then
        local id current order next
        id=$(jq -er '.created_id' "$b/state.json")
        current=$(efibootmgr -v | sed -n "/^Boot$id/p")
        next=$(efibootmgr | sed -n 's/^BootNext: //p')
        order=$(efibootmgr | sed -n 's/^BootOrder: //p')
        if [[ $current == "$(cat "$b/created-entry")" && ,${order^^}, != *,${id^^},* && ${next^^} != "${id^^}" ]]; then efibootmgr --bootnum "$id" --delete-bootnum;
        else echo "Boot$id changed or is referenced by BootOrder/BootNext; left untouched. Resolve it manually." >&2; fi
    fi
    sync
}
if [[ -n $restore ]]; then
    [[ -f $restore/state.json && -f $restore/agent.json && -f $restore/agent.bin ]] || { echo 'Incomplete conversion backup' >&2; exit 1; }
    echo "Restore only this OS agent and selected EFI files from $restore. BootOrder/BootNext remain untouched."
    read -r -p 'Type RESTORE to roll back: ' answer
    [[ $answer == RESTORE ]] || { echo 'Rollback cancelled.'; exit 0; }
    active=false; if systemctl is-active --quiet remote-boot.service; then active=true; systemctl stop remote-boot.service; fi
    trap 'rc=$?; if $active; then systemctl start remote-boot.service || rc=1; fi; exit "$rc"' EXIT
    restore_files "$restore"
    echo 'Local files restored. Server preparation may now be stale; set the desired mode and rerun conversion to report it. No physical boot was tested.'
    exit 0
fi
[[ -n $profile ]] || { echo '--discovery-profile is required for assisted conversion/agent upgrade.' >&2; exit 1; }
python3 "$root/scripts/discovery_profile.py" check "$profile" >/dev/null
fingerprint=$(jq -r '.fingerprint' "$profile")
printf 'ESP fingerprint: %s\nPC: %s; desired blind_mode=%s; mode revision=%s; normal IPv4=%s\n' "$fingerprint" "$(jq -r '.pc_id' "$profile")" "$(jq -r '.blind_mode' "$profile")" "$(jq -r '.mode_revision' "$profile")" "$(jq -r '.last_ipv4' "$profile")"
echo 'Compare with a previously trusted fingerprint (or physical USB). Each OS agent in dual boot must be converted separately.'
read -r -p 'Type TRUST to confirm this fingerprint and desired mode: ' answer
[[ $answer == TRUST ]] || { echo 'Conversion cancelled; no files changed.'; exit 0; }
[[ -f $config && ! -L $config && -f $binary && ! -L $binary ]] || { echo 'Existing paired config and installed agent binary are required (no symlinks).' >&2; exit 1; }
jq -e --slurpfile p "$profile" '. as $c | .protocol==2 and .pc_id==$p[0].pc_id and (["device_id","public_key","fingerprint"] | all(. as $k | ($c[$k]//"")=="" or $c[$k]==$p[0][$k]))' "$config" >/dev/null || { echo 'Profile does not match paired PC or pinned ESP identity.' >&2; exit 1; }
esp=$(jq -r '.last_ipv4' "$profile")
[[ -z $requested || $requested == "$esp" ]] || { echo 'Requested IPv4 differs from explicitly confirmed profile.' >&2; exit 1; }
runner=${candidate:-$binary}
[[ -f $runner && -x $runner ]] || { echo "Agent not executable: $runner" >&2; exit 1; }
"$runner" --self-test || { echo 'Agent self-test failed. Supply a current native binary with --agent-file; no re-pairing is needed.' >&2; exit 1; }
pc=$(jq -r '.pc_id' "$profile")
build_dir=${RB_BUILD_DIR:-$root/build/$pc}
bash "$root/ipxe/build.sh" "$esp" "$pc" "$build_dir" 10000 "$profile"
manifest="$build_dir/manifest.json"; ipxe="$build_dir/ipxe.efi"; loader="$build_dir/RemoteBoot.efi"
[[ -f $manifest && -f $ipxe && -f $loader ]] || { echo 'Build manifest or EFI artifacts missing.' >&2; exit 1; }
jq -e --slurpfile p "$profile" --arg ipxe "$(sha256sum "$ipxe" | cut -d' ' -f1)" --arg loader "$(sha256sum "$loader" | cut -d' ' -f1)" '
 . as $m | $p[0] as $p | .pc_id==$p.pc_id and .esp_ipv4==$p.last_ipv4 and .ipxe_sha256==$ipxe and .loader_sha256==$loader and
 (["profile_version","blind_mode","mode_revision","device_id","public_key","fingerprint","discovery_version"] | all(. as $key | $m[$key]==$p[$key]))' "$manifest" >/dev/null || { echo 'Manifest PC/ESP/mode/revision/identity or actual image hash mismatch.' >&2; exit 1; }
catalog=$(rb_catalog); (( $(jq '.systems|length' <<< "$catalog") <= 24 )) || { echo 'Catalog exceeds 24 entries' >&2; exit 1; }
esp_mount=''
for path in /boot/efi /efi /boot; do
    if [[ $(findmnt -rn -M "$path" -o FSTYPE || true) == vfat ]]; then esp_mount=$path; break; fi
done
read -r -p "EFI System Partition mount [$esp_mount]: " selected; esp_mount=${selected:-$esp_mount}
source_dev=$(findmnt -rn -M "$esp_mount" -o SOURCE)
[[ $(findmnt -rn -M "$esp_mount" -o FSTYPE) == vfat && $(lsblk -dnro PARTTYPE "$source_dev" | tr '[:upper:]' '[:lower:]') == c12a7328-f81f-11d2-ba4b-00a0c93ec93b ]] || { echo 'Selected partition must be a mounted GPT FAT ESP' >&2; exit 1; }
partuuid=$(lsblk -dnro PARTUUID "$source_dev")
[[ $partuuid =~ ^[0-9a-fA-F-]{36}$ ]] || { echo 'Cannot identify selected GPT partition UUID' >&2; exit 1; }
disk="/dev/$(lsblk -dnro PKNAME "$source_dev")"; part=$(lsblk -dnro PARTN "$source_dev")
mapfile -t ids < <(efibootmgr | rb_remote_boot_ids)
(( ${#ids[@]} <= 1 )) || { echo 'Multiple Remote Boot entries; resolve explicitly first.' >&2; exit 1; }
existing=${ids[0]:-}
target="$esp_mount/EFI/iPXE"
printf 'Will update paired agent %s and %s on %s partition %s.\n' "$config" "$target" "$disk" "$part"
read -r -p 'Type INSTALL to write agent configuration and EFI: ' answer
[[ $answer == INSTALL ]] || { echo 'Conversion cancelled; no files changed.'; exit 0; }
if [[ -n $existing ]]; then
    efibootmgr -v | sed -n "/^Boot$existing/p"
    read -r -p 'Confirm existing entry targets the selected ESP; type REUSE: ' answer
    [[ $answer == REUSE ]] || { echo 'Conversion cancelled; no files changed.'; exit 0; }
fi
backup="${RB_BACKUP_ROOT:-/var/backups/remote-boot}/$(date -u +%Y%m%dT%H%M%S)-$$"
mkdir -p "$backup"; chmod 700 "$backup"
cp -p -- "$config" "$backup/agent.json"; chmod 600 "$backup/agent.json"
cp -p -- "$binary" "$backup/agent.bin"; chmod 700 "$backup/agent.bin"
cp -- "$profile" "$backup/profile.json"; cp -- "$manifest" "$backup/new-manifest.json"
for item in ipxe.efi manifest.json; do [[ ! -f $target/$item ]] || cp -p -- "$target/$item" "$backup/$item"; done
efibootmgr -v > "$backup/efibootmgr.txt"
old_order=$(efibootmgr | sed -n 's/^BootOrder: //p'); [[ $old_order =~ ^[0-9A-Fa-f,]+$ ]] || { echo 'Cannot read BootOrder' >&2; exit 1; }
jq -n --arg config "$config" --arg binary "$binary" --arg target "$target" --arg mount "$esp_mount" --arg uuid "$partuuid" --arg cm "$(stat -c %a "$config")" --arg bm "$(stat -c %a "$binary")" '{config:$config,binary:$binary,target:$target,esp_mount:$mount,partuuid:$uuid,config_mode:$cm,binary_mode:$bm}' > "$backup/state.json"
phase='agent configuration'; active=false
finish() {
    local rc=$?
    trap - EXIT
    if $active; then systemctl start remote-boot.service || rc=1; fi
    if ((rc)); then printf 'Conversion incomplete at phase: %s. Local preparation pending. Backup: %s\nRollback: bash %q --rollback %q\n' "$phase" "$backup" "$root/installer/linux/install.sh" "$backup" >&2; fi
    exit "$rc"
}
trap finish EXIT
printf 'Backup: %s\n' "$backup"
if systemctl is-active --quiet remote-boot.service; then active=true; systemctl stop remote-boot.service; fi
# Use the owned installed config, preserving PC/agent IDs, token and permissions.
"$runner" --apply-profile "$profile" --config "$config" --confirm-fingerprint "$fingerprint" || { echo 'Agent profile application failed. Older agents require --agent-file with a current native binary.' >&2; exit 1; }
if [[ -n $candidate ]]; then
    phase='agent binary upgrade'; cp -- "$candidate" "$binary.new"; chmod "$(jq -r '.binary_mode' "$backup/state.json")" "$binary.new"; mv -f -- "$binary.new" "$binary"
fi
phase='EFI install'
mkdir -p "$target"
cp -- "$ipxe" "$target/ipxe.efi.new"; cp -- "$manifest" "$target/manifest.json.new"; sync
mv -f -- "$target/ipxe.efi.new" "$target/ipxe.efi"; mv -f -- "$target/manifest.json.new" "$target/manifest.json"
cmp -s -- "$manifest" "$target/manifest.json" || { echo 'Installed manifest mismatch' >&2; exit 1; }
[[ $(sha256sum "$target/ipxe.efi" | cut -d' ' -f1) == "$(jq -r '.ipxe_sha256' "$manifest")" ]] || { echo 'Installed EFI hash mismatch' >&2; exit 1; }
if [[ -z $existing ]]; then
    efibootmgr --create-only --disk "$disk" --part "$part" --label 'Remote Boot iPXE' --loader '\EFI\iPXE\ipxe.efi'
    mapfile -t ids < <(efibootmgr | rb_remote_boot_ids); (( ${#ids[@]} == 1 )) || { echo 'Cannot identify created entry; inspect retained backup.' >&2; exit 1; }
    existing=${ids[0]}
    jq --arg id "$existing" '.created_id=$id' "$backup/state.json" > "$backup/state.new"; mv "$backup/state.new" "$backup/state.json"
    efibootmgr -v | sed -n "/^Boot$existing/p" > "$backup/created-entry"
fi
[[ $(efibootmgr | sed -n 's/^BootOrder: //p') == "$old_order" ]] || { echo 'Unexpected BootOrder change. Retained snapshot; no global restore was attempted.' >&2; exit 1; }
if $active; then systemctl start remote-boot.service; active=false; fi
phase='server preparation report'
"$binary" --report-preparation "$target/manifest.json" --config "$config" || { echo 'EFI installed, but server preparation is pending (possibly stale desired mode/revision). Export a fresh profile and rerun, or explicitly roll back.' >&2; exit 1; }
echo 'Local preparation reported; this is not proof of physical boot. Other OS agents may still need conversion.'
phase='optional BootNext test'
read -r -p 'Set BootNext for one-time test? Type TEST: ' answer || answer=''
if [[ $answer == TEST ]]; then efibootmgr --bootnext "$existing"; fi
echo "Installed Boot$existing; BootOrder preserved. No reboot performed. Backup: $backup"
