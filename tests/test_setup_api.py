#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "firmware/src/main.cpp").read_text(encoding="utf-8")
microlink = (root / "firmware/src/microlink_runtime.cpp").read_text(encoding="utf-8")
platformio = (root / "platformio.ini").read_text(encoding="utf-8")

for route in (
    "/api/v1/setup/state",
    "/api/v1/setup/password",
    "/api/v1/setup/computer",
    "/api/v1/setup/boot",
    "/api/v1/setup/integrations",
    "/api/v1/setup/finish",
):
    assert route in main

assert 'setup_complete' in main
assert 'SETUP_STAGE_MISMATCH' in main
assert 'AUTH_RATE_LIMIT' in main
assert 'tailscale_auth_key' in main
assert 'tailscale_auth_key_set' in (root / "firmware/include/config_policy.hpp").read_text(encoding="utf-8")
assert 'WiFi.mode(WIFI_STA)' in main
assert 'WiFi.setAutoReconnect(true)' in main
assert 'DNSServer' not in main
assert 'WiFi.softAP' not in main
assert 'setupKey' not in main
assert 'microlink_config.generated.h' not in microlink
assert 'authKey_.c_str()' in microlink
assert 'embed_microlink_config.py' not in platformio

print("PASS: first-run setup API, STA-only networking and NVS Tailscale configuration")
