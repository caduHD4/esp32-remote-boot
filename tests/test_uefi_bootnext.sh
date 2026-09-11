#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_file="$root/uefi/remote-boot/remote_boot.c"

grep -q 'L"BootNext"' "$source_file"
grep -q 'EFI_VARIABLE_NON_VOLATILE' "$source_file"
grep -q 'ResetSystem.*EfiResetCold' "$source_file"
if grep -q 'BS->LoadImage' "$source_file"; then
    echo 'RemoteBoot must delegate Boot#### execution to firmware through BootNext' >&2
    exit 1
fi

printf 'PASS: RemoteBoot schedules BootNext and resets through firmware\n'
