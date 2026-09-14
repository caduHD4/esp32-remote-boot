# Cloud control and dashboard stability

Approved by the user on 2026-09-14 after investigation of Sinric switches not
starting the computer, slow dashboard loading over MicroLink, and Android
Tailscale reachability failures.

## Requirements

- Preserve local boot, shutdown, authentication and configuration semantics.
- Resolve Sinric commands using Device ID, not a mutable configuration index.
  Use the same validated boot/shutdown dispatch as the dashboard; never silently
  substitute a different OS or force a boot of an already-online computer.
- Normalize hexadecimal Device IDs and make rejection causes observable without
  logging credentials. Keep momentary switches usable after transient send errors.
- Reduce authenticated startup requests and avoid overlapping/hidden-tab status
  polling. Cache only public UI assets, never authenticated configuration/status.
- Reduce proven MicroLink scheduling/transport contention and provide diagnostics
  distinguishing coordinator connectivity from a usable WireGuard session.
- Treat DERP ServerInfo as asynchronous protocol input, not a blocking startup
  prerequisite. Preserve frame validation and handle transport failures promptly.
- Validate changes with regression tests and both firmware builds where available.
  Publish reviewed commits on feat/microlink-tailscale-poc using the configured
  GitHub integration. Do not merge, erase NVS, change keys, expose ports, or actuate
  the user's computer as part of testing.

## Evidence and limits

The historical logs show successful tailnet registration, DERP connectivity and
eventually memory failures during use. They are not a reproduction on current
firmware. They do not prove that every Android failure shares one cause. Hardware
latency, roaming and concurrent Sinric/Tailscale operation need a real-board test.
Raw lwIP concurrency and uptime-based WireGuard timestamps require separate proof
before an invasive rewrite; don't bundle speculative architecture changes.
