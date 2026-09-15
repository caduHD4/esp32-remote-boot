#!/usr/bin/env python3
from pathlib import Path
import importlib.util


ROOT = Path(__file__).resolve().parents[1]
html = (ROOT / "firmware/web/index.html").read_text()
css = (ROOT / "firmware/web/app.css").read_text()
js = (ROOT / "firmware/web/app.js").read_text()

for landmark in ("<header", "<nav", "<main", "<aside"):
    assert landmark in html, landmark
for view in ("overview", "boot", "settings", "sinric", "system"):
    assert f'id="view-{view}"' in html
    assert f'data-view="{view}"' in html
for token in ('<symbol id="icon-', 'role="status"', 'role="alert"', 'aria-live="polite"'):
    assert token in html, token
assert 'id="rememberLogin"' in html

for rule in (
    "min-height:44px",
    "min-width:44px",
    "position:sticky",
    ":focus-visible",
    "prefers-reduced-motion",
    "@media (min-width:960px)",
    "@media (max-width:480px)",
    ".static-fields[hidden]",
    ".danger",
    ".skeleton",
    ".login-remember",
):
    assert rule in css, rule

assert "setActiveView" in js
assert "setDhcpVisibility" in js
assert "setBusy" in js
assert "showToast" in js
assert "storeRememberedLogin" in js

spec = importlib.util.spec_from_file_location("embed_web", ROOT / "scripts/embed_web.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
document = module.build_web_document(ROOT).decode()
assert '<script src="http' not in document
assert '<link href="http' not in document
assert "overflow-x:hidden" in document

print("PASS: responsive accessible dashboard structure")
