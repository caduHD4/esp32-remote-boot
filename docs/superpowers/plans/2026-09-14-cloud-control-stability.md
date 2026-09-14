# Cloud control stability implementation plan

Spec: ../specs/2026-09-14-cloud-control-stability.md

## Global Constraints

Preserve authentication, local boot/shutdown semantics, credentials and NVS.
No real power operations in tests. No new hardware or external service required.
Keep changes in the existing feature worktree. Never claim hardware validation
from native tests or compilation. Publish through configured GitHub integration
only after review; no merge or force push.

## Task 1: Sinric dispatch and lightweight dashboard

Files: firmware/src/main.cpp, firmware/include/sinric_policy.hpp,
firmware/include/config_validation.hpp (if applicable), firmware/web/app.js,
scripts/embed_web.py and regression tests.

- [x] Add failing behavioral regression tests for stale slot indices, normalized
  IDs, rejected boot requests and retained OFF-reset retries. Read pinned SDK
  dispatch/event semantics before implementing; do not replace SDK with mocks.
- [x] Resolve each incoming Device ID against current slots, share existing boot
  and shutdown validation, report rejection reasons without secrets. Retry failed
  OFF reset at a bounded cadence without blocking or replaying boot commands.
- [x] Add failing tests for nonoverlapping polling and hidden-tab behavior.
- [x] Add one authenticated bootstrap response containing config and status;
  preserve old endpoints. Use a single shared status/config serialization path.
- [x] Cache gzip public UI with a content-derived ETag and conditional GET;
  keep sensitive responses no-store. Prevent overlapping polls, pause hidden tabs,
  recover on visibility, and bound fetch duration. Prioritize Sinric servicing.
- [x] Run relevant tests, regenerate assets through existing script, self-review,
  commit and provide report. No push; controller publishes after full review.

## Task 2: MicroLink transport and diagnostics

Files: components/microlink/src/ml_derp.c, ml_coord.c, ml_wg_mgr.c,
wireguard sources, microlink runtime/header and dashboard status rendering.

- [x] Inspect current state transitions and allocation lifetimes; use primary
  protocol sources to distinguish ServerInfo startup semantics from validity.
- [x] Add failing behavioral tests for each extracted transport/scheduling policy.
- [x] Consume ServerInfo through incremental receive handling, preserving bounds
  and transport cleanup; don't wait for DERP in the coordinator task.
- [x] Preserve fragmented DERP headers/payloads across polls and treat EOF as a
  disconnect. For long-poll HTTP/2/MapResponse, don't silently discard incomplete
  updates: preserve framing across Noise records or fail explicitly and reconnect
  for a fresh map, with regression cases for fragmentation and closed streams.
- [x] Make failed long-poll startup/reconnect visible and avoid declaring usable
  connectivity from control registration alone. Expose safe aggregate diagnostic
  counters/states (no keys), using existing synchronization where required.
- [x] Prioritize WireGuard packets over a bounded discovery batch, avoid redundant
  relay PONGs for direct requests, and suppress hot-path unconditional serial logs.
- [x] Coordinate low-memory TLS retry admission/backoff without disabling local
  access, permanently starving Sinric, or pretending a threshold guarantees success.
- [x] Run relevant tests, self-review, commit and provide report. Document anything
  remaining dependent on hardware reproduction rather than speculative rewrites.

## Task 3: Verification, documentation and publication

- [ ] Review each implementation task and address important findings.
- [ ] Run ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh and configuration tests.
- [ ] Attempt both firmware builds if toolchains are available; otherwise use CI
  after publication and explicitly distinguish its result from hardware testing.
- [ ] Record actual changes, test results and CachyOS flash/test commands in docs.
- [ ] Whole-change review, verify remote branch parent/tree, publish without force
  through GitHub integration, inspect workflow result and report exact status.
