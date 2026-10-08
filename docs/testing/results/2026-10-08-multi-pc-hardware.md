# Multi-PC v3 hardware validation

Hardware: ESP32-C3, MicroLink variant, LAN and Tailscale. Date: 2026-10-08.

The integration runner `tests/hardware/test_multi_pc_live.py` passed against the actual ESP with four simulated computers, 96 boot entries and four authenticated WebSocket clients. The full-capacity soak lasted 120 seconds.

Verified:

- First-access/login behavior, authentication rejection and legacy API rejection.
- Four-PC capacity, duplicate MAC rejection and three pending pairing requests.
- Short-code pairing and idempotent confirmation.
- Independent catalogs, pagination, ordering, hidden entries and generation checks.
- Token-derived PC scope, stale sessions and malformed catalog rejection.
- Targeted discovery and simulated shutdown acknowledgement/result.
- Durable rollback after invalid updates, without disconnecting agents.
- Tailscale control/DERP coexistence at capacity, authenticated HTTP and agent WebSocket/catalog synchronization through the VPN.
- Eight agent bindings, alternate installations, capacity rejection and immediate revocation.
- NVS configuration/catalog persistence across ESP restart.
- Sinric selection clears stale mappings; deleting the selected PC disables its integration.

The measured load run recorded a minimum sampled free heap of 39,244 bytes, a minimum sampled largest block of 24,576 bytes and 54 online Tailscale samples. Maximum observed HTTP latency across the integration run was 8.719 seconds; this is not a latency guarantee. Host VPN sessions may take time to renew after the ESP restarts; the exact cause of an initially delayed connection was not established.

NativeAOT binaries and installer fixtures passed on both Windows and Linux in GitHub Actions at `e1e5180`. Both firmware variants and the portable persistence, transaction, framing, parser and cryptographic tests passed in CI at that commit. The final dashboard suite passed 288/288 cases across Chromium, Firefox and WebKit at four viewport sizes, including 63-character unbroken labels.

After the final dashboard upload (USB read-back hash verified), the real embedded UI passed a read-only Chromium smoke test at 20:44 UTC: four PC cards, four connected agents, all four catalogs/status pages, 20 authenticated GET requests and zero mutation attempts. Page and long-label overflow assertions passed. A separate authenticated Tailscale HTTP probe under load responded in 0.485 seconds with control/DERP online and no control reconnects.

The complete hardware runner was repeated on that final uploaded build and passed all checks again, including Sinric cleanup and durable reload: 120-second soak, 55 online Tailscale samples, minimum sampled free heap 41,836 bytes, largest block 24,576 bytes and maximum HTTP latency 8.203 seconds. Temporary PCs/agents were removed by the runner after the test.

Limits: agent clients and host power responses in the hardware runner are simulated. No physical PC shutdown, restart, UEFI mutation, Sinric cloud command or two-hour soak is claimed. Real installations on the user's computers remain the acceptance step for those operations. Private credentials, serial logs and locally configured firmware binaries are excluded from published artifacts.
