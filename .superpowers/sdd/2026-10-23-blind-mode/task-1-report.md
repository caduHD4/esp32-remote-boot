# Task1 report — firmware and canonical discovery

Status: DONE_WITH_CONCERNS (two review findings fixed; synthetic/software verification only, no physical validation).

## Implemented

- `shared/discovery_protocol.h`: C-compatible exact query40/reply177 framing, max512, canonical byte-order, nonce/ID/key/source/port binding and address rejection. Framing validation explicitly does not replace ECDSA verification.
- Shared `tests/fixtures/discovery-v1.json`: deterministic valid signed response and invalid mutations, synthetic PUBLIC TEST ONLY P-256 scalar1, no actual device identity or credential. JSON keys `profile`, `nonce_hex`, `query_hex`, `unbound_query_hex`, `reply_hex`, `source_ipv4`, `invalid[{name,reply_hex}]`.
- ESP identity: separate `rb-discovery` namespace, one149-byte versioned blob (`RBI1`, ID16, scalar32, SEC1 point65, SHA256). Validate checksum, private/public keys and their mathematical relationship; no overwrite/rotation for existing malformed identity. One preparation attempt with connected Wi-Fi entropy; failed identity leaves normal mode running. USB emits only public ID/fingerprint.
- UDP: one nonblocking datagram per loop via lwIP `recvfrom` into a fixed 512-byte stack buffer (no per-poll heap allocation); exact 40-byte query, oversized/unknown lengths discarded, four signing attempts per sliding 1000 ms window, free/largest heap admission before receive and service work. mDNS responder initialized once; failed service registration retried independently every 5 s. Unique `remote-boot-<last12hex>.local` and `_remote-boot._udp`.
- Optional PC mode/revision preserve legacy schema3/protocol2. Legacy validation does not insert fields into stored documents (avoids growing a near-limit old config); API metadata supplies false/1/0 defaults. Creation starts revision1, only a changed requested mode increments revision, invalid types/overflow rejected, activation requires ready identity.
- Admin identity/profile routes, pairing profile, observed hello capability metadata, token-bound installation reports independent of a live WebSocket. Reports validate mode/revision/IDs/types/hashes before change; repeated identical reports bypass ConfigStore writes. Transaction rollback preserves old preparation on failed persistence.

## Review fix (post-ITERATE)

Review `3dde762..31fabaa` returned ITERATE with two Important findings. Both are fixed:

1. Receive allocation: `WiFiUDP::parsePacket()` allocated 1,460 bytes on every connected loop before heap admission. Replaced with a raw nonblocking lwIP UDP socket, fixed 512-byte stack receive buffer, exact 40-byte length check, and heap admission before receive and service work. One datagram per loop; malformed/oversized traffic consumes the datagram but never reaches signing.
2. mDNS retry: `MDNS.addService()` result was ignored and `MDNS.begin()` doubled as service readiness. Split `mdnsInitialized` from `mdnsReady`; the responder is initialized once, a failed service registration retries with the existing 5 s backoff, and reconnect/IP changes do not tear it down or re-add unrelated services.

Focused regression `tests/test_discovery_runtime.py` extracts the real `DiscoveryRuntime::tick` and uses bounded syscall/SDK adapters. It covers low-memory admission before receive (no receive/sign/queue drain), 1,000 idle polls with zero allocations, datagram lengths 0/1/39/41/512/2048 each consuming exactly one datagram without signing, invalid flags, one valid signed reply, and mDNS `begin`/`add` failure/recovery/reconnect. PASS.

## API metadata additions (parent should add these names to contract)

`GET /api/v2/discovery` and `bootstrap.status.discovery`: `ready`, `status` (`not_prepared`, `storage_unavailable`, `identity_error`, `ready`), `discovery_version:1`; when ready, `device_id`, `public_key`, `fingerprint`, `hostname`, `last_ipv4`, `http_port`, `ws_port`.

Public profile route has exactly the normative fields. PCs export `blind_mode`, `mode_revision`, `boot_mode_revision`, `boot_blind_mode`, and report hashes `boot_ipxe_sha256`, `boot_loader_sha256` when present. Agents export:

- `observed_discovery_version`, `observed_blind_mode`, `observed_mode_revision` (RAM only, zero/default until valid hello; not readiness proof).
- `prepared_discovery_version`, `prepared_blind_mode`, `prepared_mode_revision`, `boot_prepared`, `ipxe_sha256`, `loader_sha256` (persisted report metadata).
- Existing `online`/`connected` remain separate from preparedness; all observations must be compared to current PC mode/revision.

HTTP report: unknown bearer403, malformed/mismatched report400, stale mode/revision409, success200. For `boot_prepared:false`, records per-agent report and leaves previously reported PC boot artifacts unchanged. Installation reports never prove a physical boot.

## Red/green and validation evidence

1. Wrote canonical framing test first; compile failed because `shared/discovery_protocol.h` absent. Added header; native test PASS.
2. Wrote identity persistence/corruption test first; compile failed because identity store absent. Added store; PASS. Separate rate-budget and registration-backoff tests failed on missing classes before implementation, then PASS.
3. Wrote config/default/toggle/report tests first; compile failed because config helper absent. Added helper; PASS. Added legacy no-mutation test first, observed missing validator failure, then implemented pure validator and PASS.
4. Updated existing main-loop adapter only after full suite exposed missing `discovery` adapter. Existing Wi-Fi/Sinric recovery assertions remain and pass.
5. `bash tests/run.sh`: PASS (full existing native/shell/Python/dashboard/secret-pattern suite).
6. New focused tests all PASS:
   - `g++ -std=c++17 -Wall -Wextra -Werror tests/test_discovery_protocol.cpp` then executable.
   - Same flags `tests/test_discovery_identity.cpp -lcrypto`: first generation, readback, corrupt/truncated identity, no rewrite/rotation, sliding signing budget, service retry.
   - Same flags plus `-I.pio/libdeps/esp32c3_4mb/ArduinoJson/src tests/test_discovery_config.cpp`: real ArduinoJson defaults, strict fields, mode transitions, token/IDs/revision/hash checks, idempotency.
   - `python3 tests/test_discovery_vectors.py`: compiles actual shared C helper, cryptography verifies signature and fixture negatives/replay/source mismatch.
   - `python3 tests/test_discovery_preparation_route.py`: extracts actual HTTP handler, uses real ArduinoJson, DocumentTransaction and ConfigStore with transport/Preferences adapters; verifies HTTP errors, repeated report zero extra NVS writes, write failure rollback and retry. `ARDUINOJSON_INCLUDE` may override installed public dependency location.
   - `python3 tests/test_discovery_runtime.py`: actual tick with bounded syscall/SDK adapters; fixed-buffer receive, early memory admission, one-datagram limit, malformed traffic and mDNS retry/reconnect.
7. Final builds after the review fix: standard `esp32c3_4mb` PASS 1138032 bytes 28.01% (`build-task1-standard-fix.log`); MicroLink `esp32c3_4mb_microlink` PASS 1542384 bytes 37.96% (`build-task1-microlink-final-fix.log`).
8. `git diff --check`: PASS.
9. All discovery tests are integrated into `tests/run.sh`; `bash tests/run.sh` PASS. CI and release workflows install `libssl-dev`/Python `cryptography` and run `pio pkg install -e esp32c3_4mb` before the suite so the real ArduinoJson include and OpenSSL headers are present.

## Limits / handoff

- No hardware, NVS, EFI, network/router, credential files/generated local Wi-Fi headers or Graphify accessed. Existing deletion of `config.local.example.json` preserved and excluded from commit.
- Tests are native/synthetic; no claim about live DHCP transitions, physical reboot, UDP service behavior on an actual ESP, or physical NVS endurance. Crypto uses the firmware mbedTLS API; shared vectors are independently verified with Python cryptography. Both final firmware builds completed after the fix; physical validation remains deferred.
- `tests/run.sh` now requires OpenSSL headers/libcrypto, Python `cryptography`, and a real ArduinoJson 7 include. `ARDUINOJSON_INCLUDE` may override the default `.pio/libdeps/esp32c3_4mb/ArduinoJson/src`.
- Fix re-review: independent read-only review ACCEPT; no remaining Critical/Important issue in scope. Limitations: syscall/SDK internals, stack headroom and real ESPmDNS re-announce behavior remain hardware-unverified.
