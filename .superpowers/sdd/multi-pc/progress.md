# Multi-PC implementation ledger

Plan: docs/superpowers/plans/2026-10-08-multi-pc-agent-pairing.md
Base: 01c9cce; branch: feat/multi-pc-agent-pairing

- Root: firmware schema 3, durable NVS, per-PC runtime, pairing, API/WS v2, Sinric, workflows, hardware integration.
- native_agents: native CLI/service, pairing/installers and native tests.
- dashboard: dashboard v2 and browser tests.
- boot_efi: PC-scoped iPXE/EFI builders/installers/tests.

Implementation complete in all owners. Dedicated feature branch; shared checkout with explicit ownership. Native/EFI and two independent firmware/API reviews performed; actionable findings corrected. No real host shutdown/reboot/EFI mutations in automated tests.

Verified: 13 portable C++ executables including real ConfigStore fault injection; Native loopback/FakeHost and installer fixtures; EFI helper fixtures; both firmware variants compile; ESP USB upload at0x20000 with read-back hash. UI204/204 passed, expanded264-case final run underway.

In progress: actual ESP four-client integration/capacity tests, MicroLink persistence, GitHub CI/NativeAOT binaries and release. Final upload will embed frozen dashboard source. Physical two-PC OS energy/UEFI operations and long soak require dedicated hardware; do not infer them from simulated clients.
