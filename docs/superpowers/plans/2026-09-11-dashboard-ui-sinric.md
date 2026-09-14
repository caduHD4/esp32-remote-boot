# Dashboard UI/UX and Sinric Guard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a responsive dark-technology dashboard and prevent incomplete Sinric credentials from starting the SDK or producing repeated serial errors.

**Architecture:** Keep the ESP32 serving one gzip/PROGMEM response, but compose that response deterministically from semantic HTML, CSS, and JavaScript source files. Put browser-side Sinric checks in a pure exported function and firmware-side readiness in a dependency-free policy header so both layers can be tested directly.

**Tech Stack:** HTML5, CSS3, Vanilla JavaScript, inline SVG, Python 3 embed/test scripts, C++17 native tests, PlatformIO/Arduino ESP32.

**Spec:** `docs/superpowers/specs/2026-09-11-dashboard-ui-sinric-design.md`

## Global Constraints

- Operate fully offline without frameworks, CDNs, remote fonts, or remote icon libraries.
- Preserve API paths, field names, NVS schema 2, WoL, agents, BootNext, and UEFI flow.
- Produce one deterministic gzip asset in `firmware/include/web_asset.h`.
- Support viewport widths from 320 px and `prefers-reduced-motion`.
- Never expose stored credentials; only consume existing `*_set` booleans.
- Keep the firmware APP below the existing 90% size gate.

---

### Task 1: Deterministic multi-source web asset

**Files:**
- Modify: `scripts/embed_web.py`
- Create: `firmware/web/app.css`
- Create: `firmware/web/app.js`
- Modify: `firmware/web/index.html`
- Create: `tests/test_web_asset.py`
- Modify: `tests/run.sh`

**Interfaces:**
- Consumes: `firmware/web/index.html` containing `{{APP_CSS}}` and `{{APP_JS}}`.
- Produces: `build_web_document(root: Path) -> bytes` and deterministic gzip bytes written to `firmware/include/web_asset.h`.

- [ ] **Step 1: Write the failing asset test**

Create a Python test that imports `scripts/embed_web.py` without PlatformIO, calls `build_web_document()`, verifies placeholders are replaced exactly once, checks no external `http://` or `https://` asset reference exists, and confirms two gzip builds with `mtime=0` are identical.

- [ ] **Step 2: Verify RED**

Run: `python3 tests/test_web_asset.py`  
Expected: FAIL because `build_web_document`, `app.css`, and `app.js` do not exist.

- [ ] **Step 3: Implement the deterministic composer**

Define:

```python
def build_web_document(root: Path) -> bytes:
    template = (root / "firmware/web/index.html").read_text()
    css = (root / "firmware/web/app.css").read_text()
    js = (root / "firmware/web/app.js").read_text()
    if template.count("{{APP_CSS}}") != 1 or template.count("{{APP_JS}}") != 1:
        raise ValueError("web template placeholders must occur exactly once")
    return template.replace("{{APP_CSS}}", css).replace("{{APP_JS}}", js).encode()
```

Keep the PlatformIO `Import("env")` entry behind an execution guard so the function is importable in tests. Compress using `gzip.compress(document, mtime=0)`.

- [ ] **Step 4: Verify GREEN**

Run: `python3 tests/test_web_asset.py`  
Expected: PASS.

- [ ] **Step 5: Register and commit**

Add `python3 "$root/tests/test_web_asset.py"` to `tests/run.sh`.

Commit: `test: add deterministic dashboard asset pipeline`

### Task 2: Responsive dashboard and accessible interactions

**Files:**
- Modify: `firmware/web/index.html`
- Modify: `firmware/web/app.css`
- Modify: `firmware/web/app.js`
- Create: `tests/test_dashboard_ui.py`

**Interfaces:**
- Consumes: existing `/api/v1/*` endpoints and config/status response keys.
- Produces: five navigation views, status cards, boot cards, form sections, toast feedback, loading states, and the same API payload shapes used by the current dashboard.

- [ ] **Step 1: Write failing static UI tests**

Assert semantic landmarks, five navigation targets, inline SVG sprite, `role="status"`, `role="alert"`, 320 px media query, 44 px touch target, sticky mobile navigation, desktop sidebar breakpoint, focus-visible styling, `prefers-reduced-motion`, DHCP conditional class, destructive action class, and absence of external assets.

- [ ] **Step 2: Verify RED**

Run: `python3 tests/test_dashboard_ui.py`  
Expected: FAIL because the current monolithic dashboard lacks the new structure and files.

- [ ] **Step 3: Implement semantic HTML and CSS**

Create views `overview`, `boot`, `settings`, `sinric`, and `system`. Implement the approved dark technological palette, responsive sidebar/mobile tab bar, card grids, badges, 44 px controls, skeletons, toast stack, inline SVG icons, focus-visible rules, and reduced-motion override.

- [ ] **Step 4: Port existing behavior into app.js**

Preserve login, status refresh, config rendering, boot, forced WoL, reboot, shutdown, discovery, logs, ESP restart, factory reset, entry order/visibility, and Sinric slots. Add busy-button handling, Portuguese API error mapping, view navigation, toast feedback, DHCP field visibility, and first-invalid-field focus.

- [ ] **Step 5: Verify GREEN**

Run:
```bash
python3 tests/test_dashboard_ui.py
node --check firmware/web/app.js
python3 tests/test_web_asset.py
```
Expected: all PASS.

- [ ] **Step 6: Commit**

Commit: `feat: redesign responsive remote boot dashboard`

### Task 3: Browser-side Sinric validation

**Files:**
- Modify: `firmware/web/app.js`
- Create: `tests/test_dashboard_validation.js`
- Modify: `tests/run.sh`

**Interfaces:**
- Produces: `globalThis.RemoteBootValidation.validateSinric(input)`.
- Input: `{enabled, appKey, appKeySet, appSecret, appSecretSet, slots, validBootIds}`.
- Output: `{valid: boolean, errors: Record<string,string>, slots: Array}`.

- [ ] **Step 1: Write failing behavior tests**

Cover disabled Sinric with empty slots, enabled without App Key, enabled without App Secret, stored credentials via `*_set`, malformed/duplicate 24-character device IDs, invalid targets, valid credentials with no slots warning compatibility, and empty-slot filtering while disabled.

- [ ] **Step 2: Verify RED**

Run: `node tests/test_dashboard_validation.js`  
Expected: FAIL because `RemoteBootValidation.validateSinric` is absent.

- [ ] **Step 3: Implement validation and form integration**

Expose the pure validator before browser initialization and return early when `document` is unavailable. On submit, show field-level messages and `aria-invalid`, focus the first error, do not call `api("config","PUT",...)` on failure, and preserve existing-secret semantics through `sinric_app_key_set` and `sinric_app_secret_set`.

- [ ] **Step 4: Verify GREEN**

Run: `node tests/test_dashboard_validation.js`  
Expected: PASS.

- [ ] **Step 5: Commit**

Commit: `fix: validate Sinric configuration before save`

### Task 4: Firmware Sinric start guard and specific API error

**Files:**
- Create: `firmware/include/sinric_policy.hpp`
- Create: `tests/test_sinric_policy.cpp`
- Modify: `firmware/src/main.cpp`
- Modify: `tests/run.sh`

**Interfaces:**
- Produces:
```cpp
namespace rb {
enum class SinricReadiness { Disabled, Ready, MissingCredentials };
SinricReadiness sinricReadiness(bool enabled, const char* appKey, const char* appSecret);
}
```
- `Ready` requires both credential strings to contain at least 10 bytes, matching current persistence validation.

- [ ] **Step 1: Write failing policy tests**

Assert Disabled for `enabled=false`, MissingCredentials for null/short key or secret, and Ready for both strings of at least 10 bytes.

- [ ] **Step 2: Verify RED**

Run the new C++ test command from `tests/run.sh`.  
Expected: compilation failure because `sinric_policy.hpp` is absent.

- [ ] **Step 3: Implement policy and integrate it**

Use the policy from both `validate()` and `startSinric()`. In the config PUT route, return `SINRIC_CREDENTIALS_REQUIRED` before generic `INVALID_CONFIG`. In `startSinric()`, call `SinricPro.begin()` only for Ready; for MissingCredentials record `SINRIC_CONFIG_INCOMPLETE` once and leave `sinricStarted=false`.

- [ ] **Step 4: Verify GREEN**

Run:
```bash
ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh
pio run -e esp32c3_4mb
```
Expected: PASS and firmware below the 90% APP gate.

- [ ] **Step 5: Commit**

Commit: `fix: guard Sinric startup against incomplete credentials`

### Task 5: Documentation, version, and final verification

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/dashboard.md`
- Modify: `docs/troubleshooting.md`
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Produces version `2.1.3-experimental` and operator guidance for the redesigned dashboard and Sinric validation.

- [ ] **Step 1: Update documentation and version**

Document navigation, responsive behavior, existing-secret indicators, validation behavior, `SINRIC_CREDENTIALS_REQUIRED`, and `SINRIC_CONFIG_INCOMPLETE`. Set `Version[]="2.1.3-experimental"`.

- [ ] **Step 2: Run full verification**

Run:
```bash
ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh
pio run -e esp32c3_4mb
make -C uefi/remote-boot
```
Expected: all commands exit 0 and the PlatformIO size gate remains below 90%.

- [ ] **Step 3: Inspect the final diff**

Confirm only dashboard source/pipeline, Sinric policy/integration, tests, version, changelog, and related documentation changed. Confirm no credential values, IPs, MACs, or generated `config.local.json` entered the diff.

- [ ] **Step 4: Commit**

Commit: `docs: release dashboard redesign as 2.1.3 experimental`
