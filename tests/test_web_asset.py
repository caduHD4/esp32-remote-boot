#!/usr/bin/env python3
import gzip
import hashlib
import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("embed_web", ROOT / "scripts/embed_web.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


document = MODULE.build_web_document(ROOT)
assert isinstance(document, bytes)
assert b"{{APP_CSS}}" not in document
assert b"{{APP_JS}}" not in document
assert b"<style>" in document
assert b"<script>" in document
assert b'<script src="http' not in document
assert b'<link href="http' not in document

first = gzip.compress(document, mtime=0)
second = gzip.compress(MODULE.build_web_document(ROOT), mtime=0)
assert first == second
assert gzip.decompress(first) == document

MODULE.write_web_asset(ROOT)
header = (ROOT / "firmware/include/web_asset.h").read_text()
assert header.startswith("#pragma once\n#include <pgmspace.h>\n")
assert "\\n" not in header[:80]
assert "const unsigned char webAsset[] PROGMEM" in header
etag = hashlib.sha256(first).hexdigest()[:16]
assert f'const char webAssetEtag[] = "{etag}";' in header

print("PASS: deterministic multi-source dashboard asset")
