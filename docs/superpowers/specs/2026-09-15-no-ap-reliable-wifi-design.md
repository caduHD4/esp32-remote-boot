# No-AP Wi-Fi Reliability and Dashboard Design

## Goal

Create `feat/no-ap-reliable-wifi` from `feat/microlink-tailscale-poc`. The ESP32-C3 must use only Wi-Fi credentials embedded from `config.local.json`, recover from Wi-Fi loss without enabling an access point, and keep the dashboard responsive on LAN and Tailscale.

## Credentials

- Admin and agent credentials accept 8–128 UTF-8 bytes.
- Control characters are rejected because they are unsafe in JSON and HTTP Bearer authentication.
- Admin and agent credentials must remain different. The agent credential is stored on a PC; making it administrative would widen compromise impact.
- Existing 24–128 character alphanumeric, underscore, and hyphen tokens remain valid after migration.
- Firmware validation, dashboard validation, installers, and the NativeAOT agent use the same rule.

## Wi-Fi lifecycle

- SoftAP, DNS captive portal, setup recovery AP, and their policy/test are removed.
- `config.local.json` remains the only source of Wi-Fi credentials for a new device.
- A non-blocking Wi-Fi lifecycle retries association after boot and after disconnection. It uses bounded exponential backoff, resetting to the initial delay after connection.
- The HTTP and WebSocket servers start independently of association. Network-dependent work (WoL, Sinric, MicroLink) observes the connection state.
- Local Wi-Fi configuration can be saved through the dashboard only after a valid bootstrap configuration exists; no recovery AP is created for an invalid or unavailable network.
- The serial log and status API report connection state, last failure, attempt count, and next retry delay.

## Bootstrap

A device built with a valid `config.local.json` and no NVS configuration joins the configured LAN, exposes only an initial credential-creation route, then persists a schema-3 configuration. No random administration token is generated or printed. A persisted legacy schema is migrated without forcing users to replace existing credentials.

## MicroLink

MicroLink receives every Wi-Fi transition. If Wi-Fi returns after a loss, an active stale instance is stopped and a new instance is started after the connection is stable. Startup failures are retried with bounded backoff rather than being permanently disabled by the current single-attempt lifecycle. Tailscale configuration remains build-time in this branch; moving its auth key to the dashboard is out of scope.

## Dashboard performance

- The gzip dashboard asset retains an ETag and is served with immutable cache semantics keyed by that ETag/build asset.
- Bootstrap supplies config and status in one request. Status polling remains non-overlapping, pauses while the page is hidden, and refreshes immediately when visible.
- Requests are aborted during restart/navigation changes and use a single in-flight status request.
- Dashboard startup does not wait on Sinric or MicroLink. Tailscale status is rendered from the normal status response.

## Test strategy

- Host unit tests cover credential validation, Wi-Fi retry schedule, state reset after reconnect, and MicroLink restart eligibility.
- Dashboard Node tests cover credential validation, bootstrap de-duplication, and polling behavior.
- API/source policy tests assert that SoftAP/DNS are absent and that recovery never creates an AP.
- The full existing test suite plus both PlatformIO environments must pass.

## Exclusions

- No multi-PC model.
- No Tailscale auth-key dashboard migration.
- No OTA/partition redesign.
