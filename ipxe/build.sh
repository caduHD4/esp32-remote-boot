#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
esp=${1:?Usage: build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms] [profile.json]}
pc_id=${2:?Usage: build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms] [profile.json]}
out=${3:-$root/build/$pc_id}; timeout=${4:-10000}; profile=${5:-}
[[ $esp =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || { echo 'Expected IPv4 address' >&2; exit 1; }
[[ $pc_id =~ ^[0-9a-f]{32}$ ]] || { echo 'Expected 32 lowercase hexadecimal PC ID' >&2; exit 1; }
python3 -c 'import ipaddress,sys; a=ipaddress.IPv4Address(sys.argv[1]); assert 0 < int(a)>>24 < 224 and not a.is_loopback' "$esp" || { echo "Expected unicast IPv4 address" >&2; exit 1; }
IFS=. read -r a b c d <<< "$esp"
for octet in "$a" "$b" "$c" "$d"; do ((10#$octet<=255)) || exit 1; done
[[ $timeout =~ ^[1-9][0-9]{3,4}$ ]] && ((timeout>=1000&&timeout<=60000)) || exit 1
mkdir -p "$out"; out=$(cd -- "$out" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/remote-boot-ipxe.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT
profile_blind=false
if [[ -n $profile ]]; then
    # One validated public snapshot feeds the script and manifest, even if the
    # dashboard export is replaced while compilation is in progress.
    cp -- "$profile" "$work/profile.json"
    profile=$work/profile.json
    python3 "$root/scripts/discovery_profile.py" check "$profile" >/dev/null
    [[ $(python3 "$root/scripts/discovery_profile.py" field "$profile" pc_id) == "$pc_id" ]] || { echo 'Discovery profile PC ID mismatch' >&2; exit 1; }
    profile_blind=$(python3 "$root/scripts/discovery_profile.py" field "$profile" blind_mode)
    profile_ip=$(python3 "$root/scripts/discovery_profile.py" field "$profile" last_ipv4)
    if [[ $profile_blind == true ]]; then esp=$profile_ip; fi
fi
revision=7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1
# Never checkout, clean, force-remove a worktree, or compile in a user cache.
# Export only the pinned commit to a fresh directory owned by this invocation.
source_dir=${IPXE_SOURCE:-$root/.cache/ipxe}
if ! git -C "$source_dir" cat-file -e "$revision^{commit}" 2>/dev/null; then
    source_dir=$work/upstream
    git clone --no-checkout https://github.com/ipxe/ipxe.git "$source_dir"
fi
mkdir "$work/ipxe"
git -C "$source_dir" archive --format=tar "$revision" | tar -xf - -C "$work/ipxe"
build_src=$work/ipxe/src
if [[ $profile_blind == true ]]; then
    cp "$root/ipxe/rbdiscover/rbdiscover.c" "$build_src/usr/rbdiscover.c"
    cp "$root/ipxe/rbdiscover/rbdiscover_cmd.c" "$build_src/hci/commands/rbdiscover_cmd.c"
    cp "$root/ipxe/rbdiscover/"*.h "$build_src/include/usr/"
    printf '\n/* Remote Boot signed discovery */\nREQUIRE_OBJECT ( rbdiscover_cmd );\n' >> "$build_src/config/config.c"
fi
# Build the loader privately too; no races or stale timestamps in the workspace.
mkdir "$work/loader"
cp "$root/uefi/remote-boot/"*.c "$root/uefi/remote-boot/"*.h "$root/uefi/remote-boot/Makefile" "$work/loader/"
make -C "$work/loader" -B
if [[ $profile_blind == true ]]; then
    sed -e "s/@DEVICE_ID@/$(python3 "$root/scripts/discovery_profile.py" field "$profile" device_id)/g" \
        -e "s/@PUBLIC_KEY@/$(python3 "$root/scripts/discovery_profile.py" field "$profile" public_key)/g" \
        -e "s/@LAST_IPV4@/$esp/g" -e "s/@PC_ID@/$pc_id/g" -e "s/@TIMEOUT@/$timeout/g" \
        "$root/ipxe/remote-boot-blind.ipxe.in" > "$out/remote-boot.ipxe"
else
    sed -e "s/@ESP@/$esp/g" -e "s/@PC_ID@/$pc_id/g" -e "s/@TIMEOUT@/$timeout/g" "$root/ipxe/remote-boot.ipxe.in" > "$out/remote-boot.ipxe"
fi
cp "$work/loader/RemoteBoot.efi" "$out/RemoteBoot.efi"
make -C "$build_src" -j"${JOBS:-4}" bin-x86_64-efi/ipxe.efi "EMBED=$out/remote-boot.ipxe,$out/RemoteBoot.efi" NO_WERROR=1
cp "$build_src/bin-x86_64-efi/ipxe.efi" "$out/ipxe.efi"
sha256sum "$out/RemoteBoot.efi" "$out/ipxe.efi" > "$out/SHA256SUMS"
loader_sha=$(sha256sum "$out/RemoteBoot.efi" | cut -d' ' -f1)
ipxe_sha=$(sha256sum "$out/ipxe.efi" | cut -d' ' -f1)
if [[ -n $profile ]]; then
    profile_version=$(python3 "$root/scripts/discovery_profile.py" field "$profile" profile_version)
    protocol=$(python3 "$root/scripts/discovery_profile.py" field "$profile" protocol)
    discovery_version=$(python3 "$root/scripts/discovery_profile.py" field "$profile" discovery_version)
    mode_revision=$(python3 "$root/scripts/discovery_profile.py" field "$profile" mode_revision)
    device_id=$(python3 "$root/scripts/discovery_profile.py" field "$profile" device_id)
    public_key=$(python3 "$root/scripts/discovery_profile.py" field "$profile" public_key)
    fingerprint=$(python3 "$root/scripts/discovery_profile.py" field "$profile" fingerprint)
    printf '{"esp_ipv4":"%s","pc_id":"%s","ipxe_sha256":"%s","loader_sha256":"%s","profile_version":%s,"protocol":%s,"discovery_version":%s,"blind_mode":%s,"mode_revision":%s,"device_id":"%s","public_key":"%s","fingerprint":"%s"}\n' \
        "$esp" "$pc_id" "$ipxe_sha" "$loader_sha" "$profile_version" "$protocol" "$discovery_version" "$profile_blind" "$mode_revision" "$device_id" "$public_key" "$fingerprint" > "$out/manifest.json"
else
    printf '{"esp_ipv4":"%s","pc_id":"%s","ipxe_sha256":"%s","loader_sha256":"%s"}\n' \
        "$esp" "$pc_id" "$ipxe_sha" "$loader_sha" > "$out/manifest.json"
fi
printf 'Built for ESP32 %s PC %s%s: %s/ipxe.efi\n' "$esp" "$pc_id" "$([[ $profile_blind == true ]] && echo ' (blind)')" "$out"
