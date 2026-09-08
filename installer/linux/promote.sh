#!/usr/bin/env bash
set -euo pipefail
id=${1:?Provide Remote Boot Boot#### ID without prefix}
[[ $EUID == 0 && $id =~ ^[0-9A-Fa-f]{4}$ ]] || exit 1
[[ $(efibootmgr | sed -n "s/^Boot${id^^}\\*\\? //p") == 'Remote Boot iPXE' ]] || { echo 'Not a Remote Boot entry' >&2; exit 1; }
efibootmgr -v | sed -n "/^Boot${id^^}/p"
read -r -p 'After successful BootNext test, type PROMOTE to place this entry first: ' answer
[[ $answer == PROMOTE ]] || exit 0
order=$(efibootmgr | sed -n 's/^BootOrder: //p')
new=${id^^}
IFS=, read -ra ids <<< "$order"
for item in "${ids[@]}"; do [[ ${item^^} == "${id^^}" ]] || new+=",$item"; done
install -d -m 700 /var/backups/remote-boot
printf '%s\n' "$order" > "/var/backups/remote-boot/BootOrder-before-promote-$(date +%s)"
efibootmgr --bootorder "$new"
