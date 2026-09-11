#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=installer/linux/efi_helpers.sh
source "$root/installer/linux/efi_helpers.sh"

sample=$(cat <<'EOF'
BootCurrent: 0007
BootOrder: 0007,0004,0001
Boot0004* Grub2Win EFI - 64 Bit HD(1,GPT,aaaa,0x800,0x32000)/\efi\grub2win\boot.efi
Boot0007* Remote Boot iPXE      HD(3,GPT,bbbb,0x87c288,0xa00578)/\EFI\iPXE\ipxe.efi
Boot0009* Remote Boot iPXE      HD(3,GPT,bbbb,0x87c288,0xa00578)/\EFI\iPXE\ipxe.efi
EOF
)

mapfile -t ids < <(rb_remote_boot_ids <<< "$sample")
[[ ${#ids[@]} == 2 ]]
[[ ${ids[0]} == 0007 ]]
[[ ${ids[1]} == 0009 ]]

plain='Boot000A* Remote Boot iPXE'
[[ $(rb_remote_boot_ids <<< "$plain") == 000A ]]

similar='Boot000B* Remote Boot iPXE Backup HD(3,GPT,bbbb,0x87c288,0xa00578)/\EFI\iPXE\old.efi'
[[ -z $(rb_remote_boot_ids <<< "$similar") ]]

echo 'PASS: efibootmgr Remote Boot entry parsing'
