# Task 5 report — dashboard, profile export and preparedness

Status: DONE_WITH_CONCERNS (UI/value tests and firmware build pass; full browser session against the MicroLink firmware was blocked by slow large-asset transfer).

## Implemented

- Added **Ativar modo cego — encontrar a ESP automaticamente quando o IP mudar** to the new-PC dialog and existing-PC settings; unchecked by default. Copy states that the checkbox alone does not update agents/EFI or reboot the PC.
- PC save now sends `blind_mode` as a boolean (including unchecked), refreshes PC metadata after save and re-renders the requested revision/pin state.
- Added **Exportar perfil de descoberta** in authenticated PC settings. It calls `GET /api/v2/pcs/<id>/discovery-profile` and downloads the public JSON profile; no token is included.
- Added `pcBlindMode` and `pcPreparation` helpers. The PC panel and agent list show requested mode, loader/boot preparedness and per-agent prepared revision/mode separately from online/observed state. Partial dual-boot is visible as partial.
- Added explanatory copy for first trust, `.local` finder, LAN isolation and per-OS preparation; unchanged legacy PC/DHCP behavior.
- Updated English/Portuguese READMEs and `docs/native-agent.md` with the blind-mode flow and new agent CLI.
- Tests: `pcBlindMode` default/true, old PC not falsely ready, partial boot/agent state, complete state, UI landmarks (`newPcBlindMode`, `exportProfile`, `preparation-note`, `INVALID_BLIND_MODE`), deterministic embedded asset.

## Evidence

- `node tests/test_dashboard_validation.js`: PASS.
- `python3 tests/test_dashboard_ui.py`: PASS.
- `bash tests/run.sh`: PASS (full suite including dashboard, discovery runtime, firmware and protocol tests).
- `esp32c3_4mb_microlink` final build: PASS, 1,544,224-byte application (37.99%), application-only upload hash verified.
- Hardware discovery smoke: pinned signed discovery, mDNS stale-cache recovery and `--apply-profile` persist against the real ESP (see task-1-hardware-report.md).

## Limits

- Full browser session against the uploaded MicroLink firmware could not complete because the large gzip dashboard asset stalled partway over the bench link; small API endpoints worked. UI/profile code is verified by JS value/structure tests and the embedded-asset test, not a full authenticated browser run on this firmware.
- No Windows/browser dual-boot physical validation. Task 3 iPXE and Task 4 installers remain.
