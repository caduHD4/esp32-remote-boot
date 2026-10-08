#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
json_field() { node -e 'process.stdout.write(String(require(process.argv[1])[process.argv[2]]))' "$1" "$2"; }
manifest_matches() {
    node -e 'const fs=require("fs"),crypto=require("crypto"),m=require(process.argv[1]); const hash=f=>crypto.createHash("sha256").update(fs.readFileSync(f)).digest("hex"); process.exit(m.esp_ipv4===process.argv[2]&&m.pc_id===process.argv[3]&&m.ipxe_sha256===hash(process.argv[4])&&m.loader_sha256===hash(process.argv[5])?0:1)' "$1" 192.0.2.1 "$2" "$3" "$4"
}
pc_a=0123456789abcdef0123456789abcdef
pc_b=fedcba9876543210fedcba9876543210

if bash "$root/ipxe/build.sh" 192.0.2.1 >/dev/null 2>&1; then fail 'build accepted missing PC ID'; fi
if bash "$root/ipxe/build.sh" 192.0.2.1 ABCD "$tmp/invalid" >/dev/null 2>&1; then fail 'build accepted non-lowercase/non-hex PC ID'; fi

# Keep fake build artifacts out of the real source tree.
fixture_root="$tmp/project"
mkdir -p "$fixture_root/ipxe" "$fixture_root/uefi/remote-boot"
cp "$root/ipxe/build.sh" "$root/ipxe/remote-boot.ipxe.in" "$fixture_root/ipxe/"

mkdir -p "$tmp/bin" "$tmp/ipxe/src/bin-x86_64-efi" "$tmp/ipxe/.git"
cat > "$tmp/bin/git" <<'EOF'
#!/usr/bin/env bash
if [[ $1 == -C && $3 == rev-parse ]]; then echo 7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1; fi
EOF
cat > "$tmp/bin/make" <<'EOF'
#!/usr/bin/env bash
dir=''; [[ $1 != -C ]] || dir=$2
embed=''; for arg in "$@"; do [[ $arg != EMBED=* ]] || embed=${arg#EMBED=}; done
if [[ $dir == */uefi/remote-boot ]]; then
    [[ " $* " == *" -B "* ]] || { echo 'Loader build must force regeneration' >&2; exit 1; }
    printf 'MZloader-a' > "$dir/RemoteBoot.efi"
elif [[ $dir == */ipxe/src ]]; then printf 'MZ' > "$dir/bin-x86_64-efi/ipxe.efi"; cat "${embed%%,*}" >> "$dir/bin-x86_64-efi/ipxe.efi"
fi
EOF
chmod +x "$tmp/bin/git" "$tmp/bin/make"
export PATH="$tmp/bin:$PATH" IPXE_SOURCE="$tmp/ipxe"
bash "$fixture_root/ipxe/build.sh" 192.0.2.1 "$pc_a" "$tmp/out-a" 5000 >/dev/null
bash "$fixture_root/ipxe/build.sh" 192.0.2.1 "$pc_b" "$tmp/out-b" 5000 >/dev/null
grep -q "http://192.0.2.1/boot/$pc_a.ipxe" "$tmp/out-a/remote-boot.ipxe"
grep -q "http://192.0.2.1/boot/$pc_b.ipxe" "$tmp/out-b/remote-boot.ipxe"
[[ $(json_field "$tmp/out-a/manifest.json" pc_id) == "$pc_a" ]] || fail 'manifest PC A mismatch'
[[ $(json_field "$tmp/out-a/manifest.json" esp_ipv4) == 192.0.2.1 ]] || fail 'manifest ESP mismatch'
[[ $(json_field "$tmp/out-a/manifest.json" ipxe_sha256) == "$(sha256sum "$tmp/out-a/ipxe.efi" | cut -d' ' -f1)" ]] || fail 'iPXE hash mismatch'
[[ $(json_field "$tmp/out-a/manifest.json" loader_sha256) == "$(sha256sum "$tmp/out-a/RemoteBoot.efi" | cut -d' ' -f1)" ]] || fail 'loader hash mismatch'
[[ $(json_field "$tmp/out-b/manifest.json" pc_id) == "$pc_b" ]] || fail 'manifest PC B mismatch'
[[ $(json_field "$tmp/out-a/manifest.json" ipxe_sha256) != $(json_field "$tmp/out-b/manifest.json" ipxe_sha256) ]] || fail 'PC-specific image hashes are not distinct'
manifest_matches "$tmp/out-a/manifest.json" "$pc_a" "$tmp/out-a/ipxe.efi" "$tmp/out-a/RemoteBoot.efi" || fail 'valid build manifest rejected'
if manifest_matches "$tmp/out-a/manifest.json" "$pc_b" "$tmp/out-a/ipxe.efi" "$tmp/out-a/RemoteBoot.efi"; then fail 'manifest for another PC accepted'; fi
cp "$tmp/out-a/manifest.json" "$tmp/tampered.json"
node -e 'const fs=require("fs"),p=process.argv[1],m=require(p);m.ipxe_sha256="0".repeat(64);fs.writeFileSync(p,JSON.stringify(m))' "$tmp/tampered.json"
if manifest_matches "$tmp/tampered.json" "$pc_a" "$tmp/out-a/ipxe.efi" "$tmp/out-a/RemoteBoot.efi"; then fail 'tampered artifact hash accepted'; fi

if [[ -f "$root/installer/windows/install.ps1" ]]; then
    ! rg -q 'Administrative token|Invoke-RemoteBootApi|systems/sync' "$root/installer/windows/install.ps1" || fail 'Windows EFI installer still uses global admin catalog API'
    rg -q 'manifest\.json|pc_id|protocol' "$root/installer/windows/install.ps1" || fail 'Windows installer lacks PC manifest/config guard'
fi
if [[ -f "$root/installer/linux/install.sh" ]]; then
    ! rg -q 'RB_TOKEN|rb_api|systems/sync' "$root/installer/linux/install.sh" || fail 'Linux EFI installer still uses global admin catalog API'
    rg -q 'manifest\.json|pc_id|protocol' "$root/installer/linux/install.sh" || fail 'Linux installer lacks PC manifest/config guard'
fi
rg -q 'boot/@PC_ID@\.ipxe' "$root/ipxe/remote-boot.ipxe.in" || fail 'iPXE template lacks PC-specific boot URL'
echo 'PASS: PC-bound iPXE builds and installer guards'
