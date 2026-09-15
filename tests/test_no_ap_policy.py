#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "firmware/src/main.cpp").read_text()
dashboard = (root / "firmware/web/app.js").read_text()

for forbidden in ("DNSServer", "WiFi.softAP", "WIFI_AP_STA", "setupAP(", "setup_network_policy"):
    assert forbidden not in main, forbidden
assert "WIFI_CONNECT_ATTEMPT" in main
assert "WiFi.setAutoReconnect(true)" in main
assert "WiFi.disconnect(false,false);" not in main
assert "Calling WiFi.begin() again while the IDF driver is associating" in main
assert "|ssid|" not in main
assert "|wifi_password|" not in main
assert "config.local.json" in dashboard
print("PASS: station-only Wi-Fi policy")
