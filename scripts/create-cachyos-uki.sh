#!/usr/bin/env bash
set -euo pipefail
[[ $EUID == 0 && -f /etc/cachyos-release && -d /sys/firmware/efi ]] || { echo 'CachyOS UEFI only; optional helper' >&2; exit 1; }
command -v mkinitcpio >/dev/null
read -r -p 'Mounted ESP directory: ' esp
[[ $(findmnt -rn -M "$esp" -o FSTYPE) == vfat ]] || exit 1
[[ -f /etc/kernel/cmdline && -s /etc/kernel/cmdline ]] || { echo 'Create /etc/kernel/cmdline for YOUR root filesystem; exclude BOOT_IMAGE. See docs/uki.md.' >&2; exit 1; }
if [[ $(cat /etc/kernel/cmdline) == *BOOT_IMAGE=* ]]; then echo 'Remove BOOT_IMAGE from /etc/kernel/cmdline' >&2; exit 1; fi
[[ -f /boot/vmlinuz-linux-cachyos ]] || exit 1
read -r -p 'Type UKI to generate an optional CachyOS UKI (no BootOrder changes): ' answer
[[ $answer == UKI ]] || exit 0
mkdir -p "$esp/EFI/Linux"
destination="$esp/EFI/Linux/cachyos-remote.efi"
if [[ -f $destination ]]; then cp "$destination" "$destination.backup-$(date +%s)"; fi
mkinitcpio --kernel /boot/vmlinuz-linux-cachyos --uki "$destination.new" --cmdline /etc/kernel/cmdline
objdump -h "$destination.new" | awk '/\.(linux|initrd|cmdline|osrel)/ {print; n++} END {exit n<4}'
mv "$destination.new" "$destination"
printf 'Generated %s. Register/test separately with efibootmgr. Regenerate after kernel updates.\n' "$destination"
