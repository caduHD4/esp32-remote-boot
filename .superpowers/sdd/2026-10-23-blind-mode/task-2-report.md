# Task 2 report — native agent and local finder

Status: DONE_WITH_CONCERNS (software/synthetic verification; no Windows runtime, physical LAN or EFI validation).

## Implemented

- `agent/native/Discovery.cs`: canonical query40/reply177, P-256/SHA256/P1363 verification, nonce/device/key/fingerprint/source-IPv4/source-port/HTTP/WS binding, strict length/magic/type/flags, bounded 10 s attempt, cache-IP first then limited and per-interface directed broadcast (loopback/tunnel/VPN excluded), one valid reply per pinned attempt, no deadline extension or key substitution.
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
- `dotnet run -- --discover --timeout-ms 200`: prints `[]`, exit 0 (no ESP discovery yet because firmware upload is pending).
- `git diff --check`: PASS.

## Limits / handoff

- No Windows runtime, physical LAN/DHCP transition, hardware NVS endurance or EFI write was tested. Discovery socket/interface behavior on real Windows/Linux needs hardware smoke.
- The shared fixture proves exact wire interoperability; loopback/LAN responders are synthetic.
- Task 3 iPXE, Task 4 installers and Task 5 dashboard remain. Firmware upload and ESP hardware smoke are parent-controlled after the flash backup.
