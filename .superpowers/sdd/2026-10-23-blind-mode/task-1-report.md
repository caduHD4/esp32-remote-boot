# Task1 report — firmware and canonical discovery

Status: DONE_WITH_CONCERNS (parent completing final builds/review; no physical validation).

## Implemented

- `shared/discovery_protocol.h`: C-compatible exact query40/reply177 framing, max512, canonical byte-order, nonce/ID/key/source/port binding and address rejection. Framing validation explicitly does not replace ECDSA verification.
- Shared `tests/fixtures/discovery-v1.json`: deterministic valid signed response and invalid mutations, synthetic PUBLIC TEST ONLY P-256 scalar1, no actual device identity or credential. JSON keys `profile`, `nonce_hex`, `query_hex`, `unbound_query_hex`, `reply_hex`, `source_ipv4`, `invalid[{name,reply_hex}]`.
- ESP identity: separate `rb-discovery` namespace, one149-byte versioned blob (`RBI1`, ID16, scalar32, SEC1 point65, SHA256). Validate checksum, private/public keys and their mathematical relationship; no overwrite/rotation for existing malformed identity. One preparation attempt with connected Wi-Fi entropy; failed identity leaves normal mode running. USB emits only public ID/fingerprint.
- UDP: one packet per loop, fixed40-byte query/177-byte reply buffers, unknown lengths discarded, four signing attempts per sliding1000ms window, free/largest heap admission limits. Services register/recover on IP change; failed UDP/mDNS registration retries at5s, not each loop. Unique `remote-boot-<last12hex>.local` and `_remote-boot._udp`.
- Optional PC mode/revision preserve legacy schema3/protocol2. Legacy validation does not insert fields into stored documents (avoids growing a near-limit old config); API metadata supplies false/1/0 defaults. Creation starts revision1, only a changed requested mode increments revision, invalid types/overflow rejected, activation requires ready identity.
- Admin identity/profile routes, pairing profile, observed hello capability metadata, token-bound installation reports independent of a live WebSocket. Reports validate mode/revision/IDs/types/hashes before change; repeated identical reports bypass ConfigStore writes. Transaction rollback preserves old preparation on failed persistence.

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
7. Parent reported MicroLink full build PASS,1542336 bytes37.96%, before final small hardening; parent will incremental-build final commit. Parent then reported standard build PASS1138848 bytes28.03% on latest sources; log `build-task1-standard.log`.
8. `git diff --check`: PASS.

## Limits / handoff

- No hardware, NVS, EFI, network/router, credential files/generated local Wi-Fi headers or Graphify accessed. Existing deletion of `config.local.example.json` preserved and excluded from commit.
- Tests are native/synthetic; no claim about live DHCP transitions, physical reboot, UDP service behavior on an actual ESP, or physical NVS endurance. Crypto uses the firmware mbedTLS API; shared vectors are independently verified with Python cryptography. Parent final builds remain required.
- Parent integrates new tests into `tests/run.sh` and records required host dependencies (OpenSSL headers/libcrypto and Python cryptography; real ArduinoJson7 include).
- Parent should independently review commit and complete both final firmware builds before marking Task1 accepted.
