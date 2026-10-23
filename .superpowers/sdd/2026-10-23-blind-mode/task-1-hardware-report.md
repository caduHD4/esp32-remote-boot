# Task 1 hardware report — ESP32-C3 backup, upload and discovery smoke

Date: 2026-10-23. Board: ESP32-C3 AZ (QFN32) rev v1.1, 4 MB XMC flash, MAC `44:b1:76:18:8b:90`.

## Backup before any write

- Full 4 MB backup: `/home/caduhd4/.local/state/esp32-remote-boot/blind-mode-backup/2026-10-23/esp32c3-full-4mb.bin`
- SHA-256: `2954f57cfeace2020c895523ed2d71efddd7f51f6dd22fa0b0693f8f35d1c1d0`
- Partition table: `nvs 0x9000/0x10000`, `phy_init 0x19000/0x1000`, `factory app 0x20000/0x3e0000`.
- Pre-upload firmware: `v3.0.0-alpha.1-5-g7e58e15-dirty`, project `worktree`, IDF 5.5.5 (MicroLink variant).

## Upload

- Built `esp32c3_4mb_microlink` with Task 1 discovery + Task 5 dashboard: 1,544,224-byte application image (37.99% of factory partition).
- Wrote **only** `firmware.bin` at `0x20000`; bootloader, partition table and NVS were not written.
- esptool reported `Hash of data verified`.
- Readback of the first 1,544,224 bytes matched the image (the `.bin` file also carries a 32-byte self-hash trailer that is not part of the flashed image).
- Wi-Fi reconnected to the same network with the same address (`10.0.1.10`), and MicroLink loaded the same NVS peer keys/configuration, confirming configuration preservation. The NVS bytes changed because the new `rb-discovery` identity and NVS allocations were created.
- Discovery identity generated on the ESP: device ID `df72cc31d9d380922d26833acadf6957`, public fingerprint `ee2120e4d5c9f6cabf711e42cae312c27ae7a4aea35ac4a622f683c41ef49a7d`.

## Discovery smoke (real hardware)

- Pinned `remote-boot-agent --discover --profile`: returned the ESP with valid P-256/SHA256 signature, pinned device/key/fingerprint, source IPv4 `10.0.1.10` and ports 80/81.
- `--resolve --profile`: printed `10.0.1.10`.
- Stale-cache test: profile `last_ipv4` changed to `10.0.1.99`; discovery recovered the real address through the mDNS hostname `remote-boot-833acadf6957.local`, then verified the signed reply.
- `--apply-profile`: persisted `url=http://10.0.1.10/`, `blind_mode=true`, revision/pin/discovery metadata, and preserved the agent token.
- Unbound discovery (`--discover` without profile) returned `[]` on this host; unicast and mDNS pinned discovery worked. Firmware sets `SO_BROADCAST`. No broadcast reply was observed; an AP/bridge cause has not been established and host firewall/network behavior remains possible.

## HTTP/dashboard smoke

- Small unauthenticated endpoints work (`/api/v2/status` returns `401 AUTH_REQUIRED` JSON).
- The large gzip dashboard asset repeatedly stalled partway (4,320 of 22,381 bytes) under the MicroLink build; the full browser/dashboard smoke could not be completed over this link. The dashboard UI/profile export were verified by the JS structure/value tests and the embedded-asset test, not by a full browser session against this firmware.
- No EFI write, OS power action, reboot or router change was performed. Physical EFI/iPXE acceptance remains pending.

## Final integration update

The final MicroLink application is1,544,992 bytes. Application-only upload at
0x20000 passed its hash check; complete4MiB readback verified the application,
unchanged bootloader/partition/PHY, identical full decoded configuration and
identical149-byte discovery identity. Credentials were preserved.

The user confirmed normal dashboard access from the phone. This PC still shows
intermittent TCP transfer loss, including a temporary no-Tailscale diagnostic;
no causal firmware/AP conclusion is justified. Final firmware retains all
integrations and normal radio configuration. Authenticated profile export and
signed stale-cache/mDNS resolution both passed after the final flash. Actual
UI suite:108 Chromium tests passed; see docs/blind-mode-validation.md for limits.
