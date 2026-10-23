# Modo cego Implementation Plan

> For agentic workers: use superpowers:subagent-driven-development task by task. Never parallelize implementation agents. Preserve original local deletion.

**Goal:** Deliver optional per-PC dynamic-IP discovery, including existing-PC assisted conversion, with working firmware/agent/iPXE and truthful hardware acceptance.
**Architecture:** One signed bounded UDP discovery protocol shared across ESP, native agent and custom iPXE command; public per-PC conversion profile pins identity. Existing IPv4 mode remains compatible.
**Tech Stack:** ESP32-C3 Arduino/ESP-IDF/mbedTLS, C# .NET10, C iPXE/UEFI, Bash/PowerShell, JS Playwright.
**Spec:** docs/superpowers/specs/2026-10-23-blind-mode-design.md
**Normative interfaces:** docs/protocol/discovery-v1.md (read verbatim before implementation).

## Global Constraints

- Never erase NVS/flash, credentials, pairing, existing PC IDs or system catalog. No Graphify.
- Preserve deleted config.local.example.json; never stage it or local config/generated secrets.
- No automatic physical EFI writes, BootOrder promotion, PC shutdown/reboot, router change, shared push or deployment during implementation tests.
- All physical operations are parent-controlled after verified backup. Workers never touch hardware or private configs/dumps.
- Missing blind_mode means false. DHCP settings are global and never changed by checkbox.
- Signed discovery must verify identity before any bearer token is sent. No secrets in UDP, exported profile, manifest or EFI.
- Same-LAN IPv4 only; preserve Linux/WSL EFI build and native Windows install workflow.
- Tests before code for behavior changes; evidence before completion claims. Distinguish synthetic, build and physical verification.

## Review Focus

- Changed IP reassigned to another device must never receive stored bearer token.
- Late/multiple/wrong-key discovery replies must not substitute device or extend deadline.
- Legacy schema3/protocol2 records with no new fields keep loading and behaving normally.
- Failed NVS identity load must not silently rotate identity or block normal mode.
- Checkbox requested mode and per-OS/EFI actual preparedness must remain distinct.

### Task 1: Protocol, fixtures and firmware discovery

Own shared/discovery_protocol.h, tests/fixtures/discovery-v1.json, firmware/include/discovery*, firmware/src/discovery*, firmware app_model/app_routes/app_pairing/app_socket/main integration and focused tests. Do not edit web/agent/ipxe/installers. Normative contract defines exact bytes, fields and API paths.

- [ ] Write shared canonical parser/validation tests and signed valid/invalid fixtures before production code; watch expected failures.
- [ ] Implement canonical C header, ESP P-256 identity NVS persistence/integrity, UDP budget, mDNS and safe failure modes.
- [ ] Integrate optional config fields, profiles, pairing metadata, observed agent capabilities and authenticated/idempotent preparation reports.
- [ ] Legacy config, wrong mode types, toggle revision, malformed reports and identity corruption tests. Never expose private key.
- [ ] Run full native suite plus focused tests, compile both firmware variants (parent may assist environment), inspect size budget; self-review and commit only owned files.

### Task 2: Native agent and local finder

Own agent/native/*.cs and native tests only; consume shared fixtures and firmware API unchanged. Read Task1 report for actual integration notes.

- [ ] Add tests for signature verification, wrong/replayed replies, old config, incomplete blind config, atomic persistence and loopback changed-IP discovery before code.
- [ ] Implement signed Discovery client, multi-interface LAN search/deadlines, config/profile validation and backwards-compatible pairing metadata.
- [ ] Integrate endpoint resolution before WebSocket and catalog HTTP; persist through supplied config path; add public finder/resolve/apply/report CLI specified in contract.
- [ ] No arbitrary key rotation or credentials to unverified address. Preserve default permissions/IDs/token and do not add EFI powers.
- [ ] Run managed self-test and focused fixture tests; publish Linux native if toolchain available; compile Windows target without claiming native Windows runtime validation. Commit owned files.

### Task 3: iPXE blind discovery and build

Own ipxe/**, scripts/discovery_profile.py, tests for boot/protocol/QEMU; read shared fixtures. No installer or agent edits.

- [ ] Implement tests for packet/P-256 interoperability and builder profile validation before code. Keep old build invocations valid.
- [ ] Add checked-in rbdiscover iPXE extension using pinned upstream APIs; integrate module in writable task-owned checkout without dirtying unrelated caches.
- [ ] Verify ID/key/nonce/source/length/signature and bounded deadline; derive target HTTP IPv4 only after proof; no secret embedded.
- [ ] Build static and blind images with RemoteBoot.efi embedded and identity-bound manifests.
- [ ] Run virtual DHCP+UDP responder and OVMF/QEMU tests for valid discovery, changed address, spoofed reply, no response and successful continuation; no real EFI changes. Commit owned files.

### Task 4: Assisted installers, conversion and rollback

Own installer/** and installer fixtures; scripts for conversion/recovery as needed. Consume agent CLI, profile and manifest contracts.

- [ ] Test matching/mismatched profiles, confirmation refusal, stale modes, backup, partial errors, rerun and restoration before changes.
- [ ] Support profile-driven Windows/Linux conversion in both directions. Confirm fingerprint first; preserve IDs/tokens/permissions and existing EFI entry/BootOrder.
- [ ] Backup agent config, old EFI/manifest; validate image hashes and profile before writes; call native apply/report with exact owned config path.
- [ ] Explicitly report phase/backup if later step fails. Keep BootNext and physical reboot opt-in; update each OS agent independently.
- [ ] Run Linux and PowerShell fixtures and syntax checks; no live disk/EFI writes. Commit owned files.

### Task 5: Dashboard and end-to-end integration

Own firmware/web/**, UI mock fixtures/tests, README docs and test-runner integration; parent can implement inline after prior task gates.

- [ ] Test checkbox defaults and create/edit payload, old PCs, server rejection, requested vs prepared partial dual-boot, profile export before UI edits.
- [ ] Add checkbox and explanatory copy in new/existing PC forms; export public profile with authenticated fetch; show per-agent and boot preparation without claiming physical success.
- [ ] Keep IP/DHCP unrelated settings intact. Explain first trust, optional .local finder, LAN isolation and one-time Linux/WSL preparation on Windows.
- [ ] Run full suites, browser checks, both firmware builds, agent and EFI builds. One independent whole-branch review, fix material findings with regression tests.
- [ ] Parent: fresh full-flash backup/hash and validate partition/NVS compatibility, application-only upload, read-back hash/config preservation, real LAN signed discovery and authenticated dashboard/agent smoke test without replacing active system agent automatically.
- [ ] Parent: provide assisted physical PC/EFI test artifacts and clearly record anything requiring manual UEFI/reboot validation. Do not claim full physical boot unless performed.

## Integration and completion

Keep local commits on feat/dynamic-ip-discovery; do not push or merge to main unless requested. Record each task's commits/tests/review in .superpowers/sdd/2026-10-23-blind-mode/progress.md. Store reports in same ignored workspace. Do not restart finished tasks after compaction.
