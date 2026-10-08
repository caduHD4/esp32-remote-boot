# Multi-PC implementation ledger

Plan: docs/superpowers/plans/2026-10-08-multi-pc-agent-pairing.md
Base: 01c9cce; branch: feat/multi-pc-agent-pairing

- Root: firmware schema 3, durable NVS, per-PC runtime, pairing, API/WS v2, Sinric, workflows, hardware integration.
- native_agents: native CLI/service, pairing/installers and native tests.
- dashboard: dashboard v2 and browser tests.
- boot_efi: PC-scoped iPXE/EFI builders/installers/tests.

Implementation complete in all owners. Dedicated feature branch; shared checkout with explicit ownership. Native/EFI and two independent firmware/API reviews performed; actionable findings corrected. No real host shutdown/reboot/EFI mutations in automated tests.

Verified: 13 portable C++ executables plus document ownership tests, including actual ConfigStore fault injection and an 18KB real-ArduinoJson transaction test; Native loopback/FakeHost and installer fixtures; EFI helper fixtures; both firmware variants compile; ESP USB upload at0x20000 with read-back hash. UI276/276 passed; GitHub CI and Windows/Linux NativeAOT passed at33467c5.

In progress: actual ESP four-client integration/capacity tests, MicroLink persistence, GitHub CI/NativeAOT binaries and release. Final upload will embed frozen dashboard source. Physical two-PC OS energy/UEFI operations and long soak require dedicated hardware; do not infer them from simulated clients.

Hardware diagnostics found two memory faults: duplicate long-poll map storage/DOM caused control reconnects; a full-snapshot new(nothrow) allocation still aborted with ESP-IDF exceptions disabled. Fixes: shared bounded map workspace/selective delta parser, dynamic TLS record buffers with unchanged guards, ownership-based API transactions with durable rollback, and 512-byte streamed NVS banks. Targeted portable, actual ArduinoJson, CRC/fault and parser tests passed; final hardware load run remains pending.

Four PCs/96 entries, four sessions, catalog isolation/order, ACK/result and durable rollback passed on actual ESP. The first soak exposed idle 32KiB workspace retention during DERP admission. Follow-up releases the initial-map workspace, decrypts Noise in place and streams H2 DATA without another frame-sized allocation. Lifecycle/OOM/framing and actual mbedTLS AEAD tests pass.

The corrected firmware passed the 120-second four-client/96-entry coexistence soak, authenticated HTTP/WebSocket/catalog sync through Tailscale, eight bindings/capacity rejection/revocation, NVS reboot persistence and Sinric selection/removal cleanup. Live read-only browser smoke observed four PCs and four online agents. The browser inspection found a long-label overflow; corrected CSS passed the full 288-case UI suite. Final USB upload passed read-back hash verification; the new actual browser run confirmed four online PCs/agents and no horizontal overflow. CI and Windows/Linux NativeAOT are green at e1e5180. See docs/testing/results/2026-10-08-multi-pc-hardware.md for measured results and limits. Agent release publication and checksum verification remain the final delivery step.
