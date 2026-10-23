# Task 2 report — native agent and local finder

Status: DONE_WITH_CONCERNS (software/synthetic verification plus real ESP discovery on the bench; no Windows runtime or EFI validation).

## Implemented

- `agent/native/Discovery.cs`: canonical query40/reply177, P-256/SHA256/P1363 verification, nonce/device/key/fingerprint/source-IPv4/source-port/HTTP/WS binding, strict length/magic/type/flags, bounded 10 s attempt with 1 s retransmission, cache-IP and mDNS hostname first, then limited and per-interface directed broadcast (loopback/tunnel/VPN excluded; one socket bound per interface), one valid reply per pinned attempt, no deadline extension or key substitution.
- `Config`: optional `blind_mode`, `mode_revision`, `device_id`, `public_key`, `fingerprint`, `discovery_version`; legacy files remain valid; blind requires a complete pin; fingerprint must equal SHA256(public key); atomic save preserves token/IDs/permissions; no secret in profiles/logs.
- `Agent.ConnectOnce` resolves and verifies a blind endpoint before opening WebSocket or sending catalog HTTP/bearer; verified endpoint is persisted atomically. Non-blind behavior unchanged; absence of ESP fails that connection without losing pairing.
- CLI: `--discover [--profile FILE] [--timeout-ms N]`, `--resolve --profile FILE`, `--apply-profile FILE --config PATH --confirm-fingerprint HEX64`, `--report-preparation MANIFEST --config PATH`; old `--pair/--config/--check/--self-test` remain.
- `--apply-profile` requires exact fingerprint confirmation, matching PC ID and existing pin, preserves credentials/permissions, verifies blind devices before saving, and saves explicitly confirmed `last_ipv4` for normal mode while retaining pin metadata.
- `--report-preparation` validates manifest PC/ESP/mode/revision/discovery/hashes, resolves a blind ESP before bearer, and POSTs the authenticated preparation body; it does not claim physical boot.
- Pairing persists an optional `discovery_profile` pin but never auto-enables blind mode from a server wish.
- `DiscoverySelfTest` uses the shared `tests/fixtures/discovery-v1.json` (copied to output) plus loopback and real LAN-interface UDP responders: good fixture, every invalid mutation, wrong nonce/source/key/signature, strict profile/unknown field, legacy/blind config round-trip and corrupt pin, pairing profile retention, blind endpoint persistence, wrong-key timeout, normal profile apply and authenticated preparation HTTP.

## Evidence

- `dotnet run --project agent/native/RemoteBoot.Agent.csproj -- --self-test`: PASS, including all new discovery tests.
- `dotnet publish agent/native/RemoteBoot.Agent.csproj -c Release -r linux-x64 -o /tmp/rb-agent-publish`: PASS; published NativeAOT `--self-test`: PASS.
- `dotnet run -- --discover --timeout-ms 200`: prints `[]`, exit 0.
- Real ESP after application-only upload: pinned `--discover --profile` returned device `df72cc31...` with valid signature/source/IPv4; `--resolve` printed `10.0.1.10`; stale-cache profile (`10.0.1.99`) recovered via mDNS hostname `remote-boot-833acadf6957.local`; `--apply-profile` persisted `url=http://10.0.1.10/`, `blind_mode=true`, pin/revision and preserved the token; discovery identity was generated on the ESP (`Discovery device ID: df72cc31...`).
- Unbound discovery returned `[]` on this host, while unicast/mDNS pinned discovery worked. The firmware sets `SO_BROADCAST`; AP/bridge versus host-network cause remains unproven.
- `git diff --check`: PASS.

## Limits / handoff

- No Windows runtime, DHCP IP change (only stale-cache mDNS/unicast), hardware NVS endurance or EFI write was tested. The unbound finder did not enumerate the ESP on this host; the cause remains unproven. Profile/cached-IP and mDNS paths work. Task3 subsequently implemented and QEMU-verified iPXE mDNS candidate fallback with pinned signed proof.
- The shared fixture proves exact wire interoperability; loopback/LAN responders are synthetic.
- Task 3 iPXE and Task 4 installers remain; Task 5 dashboard is implemented separately. Firmware was backed up, uploaded application-only, and hash-verified; full NVS changed only by the new discovery identity/allocations (Wi-Fi and MicroLink peer keys survived).

Final integration embeds the public fixture for single-binary delivery and replaces synchronous DNS with one asynchronous lookup inside a monotonic deadline. Linux standalone self-test, stalled-DNS regression and real stale-cache resolution passed.
