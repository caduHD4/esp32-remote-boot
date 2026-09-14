# MicroLink Dashboard Load Stability — Investigation and Fix Plan

> **For Codex:** Execute this plan task by task with red/green tests and verify the ESP32-C3 build before publishing.

**Goal:** Keep the ESP32 online in Tailscale while the remote dashboard is loaded, authenticated, and polled on a phone.

**Architecture:** Preserve the current Arduino + ESP-IDF hybrid and the single-owner control/DERP tasks. Fix transient socket backpressure at the control boundary, make every failed DERP attempt release its TLS/socket resources, reject incomplete DERP handshakes, and reduce avoidable work on the ESP32-C3 hot path. Keep the local dashboard available even while Tailscale reconnects.

**Tech Stack:** ESP32-C3, Arduino as ESP-IDF component, lwIP sockets, mbedTLS, FreeRTOS, MicroLink, native C++ regression tests, PlatformIO.

---

## Evidence and success criteria

The captured failure starts with `coord_send ... errno=11` for a 36-byte encrypted HTTP/2 PING. `EAGAIN` is transient backpressure, but the current code treats it as a dead control connection. A DERP peer close follows while the firmware has accepted a missing `ServerInfo`; simultaneous reconnect attempts then reach `NetworkClient ... Not enough memory`, `BIGNUM - Memory allocation failed`, STUN `errno=12`, and socket failures.

The fix is acceptable when:

- a transient `EAGAIN`, `EWOULDBLOCK`, or `EINTR` is retried within a bounded deadline;
- a real send failure still moves the control task to reconnect;
- every unsuccessful DERP connect path closes the socket and frees all initialized mbedTLS contexts before retrying;
- a DERP connection is not advertised as connected until `ServerInfo` is received and consumed;
- periodic diagnostic logging no longer floods the single-core device during dashboard traffic;
- native regression tests and the `esp32c3_4mb_microlink` PlatformIO build pass.

### Task 1: Add a host-testable I/O policy

**Files:**
- Create: `components/microlink/src/ml_io_policy.h`
- Create: `tests/test_microlink_io_policy.cpp`
- Modify: `tests/run.sh`

1. Add failing tests for transient errno classification, fatal errno classification, retry budget, and bounded delay.
2. Run the focused test and confirm it fails because the policy does not exist.
3. Implement the smallest pure policy needed by the socket code.
4. Re-run the focused test and confirm it passes.

### Task 2: Make control writes tolerate backpressure

**Files:**
- Modify: `components/microlink/src/ml_coord.c`
- Test: `tests/test_microlink_io_policy.cpp`

1. Route `coord_send` errors through the tested policy.
2. Retry transient errors with a short FreeRTOS delay and a finite deadline; reset the retry budget after progress.
3. Keep zero-byte and fatal socket results as connection failures.
4. Run the focused test and full native test suite.

### Task 3: Make DERP connection attempts transactional

**Files:**
- Modify: `components/microlink/src/ml_derp.c`

1. Centralize teardown so it always frees initialized TLS state, regardless of whether the socket descriptor is still valid.
2. Convert post-TLS early returns to one failure path that releases the full attempt.
3. Validate all mbedTLS setup/seed/hostname return values.
4. Require a complete `ServerInfo` before setting `ML_EVT_DERP_CONNECTED`; otherwise cleanly retry without a false online state.
5. Treat TLS close-notify as a normal reconnect reason while preserving cleanup.

### Task 4: Reduce hot-path contention and improve observability

**Files:**
- Modify: `components/microlink/src/ml_wg_mgr.c`
- Modify: `components/microlink/src/ml_derp.c`
- Modify: `firmware/src/main.cpp`

1. Move per-tick timing, packet, and heartbeat logs from INFO/WARN to DEBUG where they do not indicate faults.
2. Keep concise state transitions and actual failures visible.
3. Emit heap/largest-block diagnostics only around connection failures so a hardware retest can distinguish network loss from memory pressure.
4. Do not change dashboard behavior or authentication semantics.

### Task 5: Verify and publish

**Files:**
- Test: `tests/run.sh`
- Build: `platformio.ini` environment `esp32c3_4mb_microlink`

1. Run `bash tests/run.sh`.
2. Run `pio run -e esp32c3_4mb_microlink` with the active PlatformIO Core.
3. Review the diff for secrets and unrelated changes.
4. Commit the fix on `feat/microlink-tailscale-poc`.
5. Publish the commit to the matching GitHub branch through the GitHub integration.
6. Provide the exact upload/monitor commands and the expected healthy log markers for the physical retest.
