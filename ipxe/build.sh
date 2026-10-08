#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
esp=${1:?Usage: build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms]}
pc_id=${2:?Usage: build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms]}
out=${3:-$root/build/$pc_id}; timeout=${4:-10000}
[[ $esp =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || { echo 'Expected IPv4 address' >&2; exit 1; }
[[ $pc_id =~ ^[0-9a-f]{32}$ ]] || { echo 'Expected 32 lowercase hexadecimal PC ID' >&2; exit 1; }
IFS=. read -r a b c d <<< "$esp"
for octet in "$a" "$b" "$c" "$d"; do ((10#$octet<=255)) || exit 1; done
[[ $timeout =~ ^[0-9]+$ ]] && ((timeout>=1000&&timeout<=60000)) || exit 1
mkdir -p "$out"; out=$(cd -- "$out" && pwd)
revision=7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1
source_dir=${IPXE_SOURCE:-$root/.cache/ipxe}
if [[ ! -d $source_dir/.git ]]; then
    git clone https://github.com/ipxe/ipxe.git "$source_dir"
fi
if [[ $(git -C "$source_dir" rev-parse HEAD) != "$revision" ]]; then
    [[ -z $(git -C "$source_dir" status --porcelain) ]] || { echo 'Dirty iPXE source; use a fresh checkout' >&2; exit 1; }
    git -C "$source_dir" fetch origin "$revision"
    git -C "$source_dir" checkout --detach "$revision"
fi
# Rebuild every loader stage: copied artifacts may have future timestamps.
make -C "$root/uefi/remote-boot" -B
sed -e "s/@ESP@/$esp/g" -e "s/@PC_ID@/$pc_id/g" -e "s/@TIMEOUT@/$timeout/g" "$root/ipxe/remote-boot.ipxe.in" > "$out/remote-boot.ipxe"
cp "$root/uefi/remote-boot/RemoteBoot.efi" "$out/RemoteBoot.efi"
make -C "$source_dir/src" -j"${JOBS:-4}" bin-x86_64-efi/ipxe.efi "EMBED=$out/remote-boot.ipxe,$out/RemoteBoot.efi" NO_WERROR=1
cp "$source_dir/src/bin-x86_64-efi/ipxe.efi" "$out/ipxe.efi"
sha256sum "$out/RemoteBoot.efi" "$out/ipxe.efi" > "$out/SHA256SUMS"
loader_sha=$(sha256sum "$out/RemoteBoot.efi" | cut -d' ' -f1)
ipxe_sha=$(sha256sum "$out/ipxe.efi" | cut -d' ' -f1)
printf '{"esp_ipv4":"%s","pc_id":"%s","ipxe_sha256":"%s","loader_sha256":"%s"}\n' \
    "$esp" "$pc_id" "$ipxe_sha" "$loader_sha" > "$out/manifest.json"
printf 'Built for ESP32 %s PC %s: %s/ipxe.efi\n' "$esp" "$pc_id" "$out"
