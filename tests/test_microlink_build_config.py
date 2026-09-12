#!/usr/bin/env python3
import configparser
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = configparser.ConfigParser(interpolation=None)
parser.read(root / "platformio.ini")

stable = parser["env:esp32c3_4mb"]
hybrid = parser["env:esp32c3_4mb_microlink"]

assert stable["framework"].strip() == "arduino"
assert "REMOTE_BOOT_ENABLE_MICROLINK" not in stable["build_flags"]
assert {item.strip() for item in hybrid["framework"].split(",")} == {"arduino", "espidf"}
assert "REMOTE_BOOT_ENABLE_MICROLINK=1" in hybrid["build_flags"]
assert "embed_microlink_config.py" in hybrid["extra_scripts"]
assert hybrid["board_build.sdkconfig_defaults"] == "sdkconfig.microlink.defaults"

settings = {}
for line in (root / "sdkconfig.microlink.defaults").read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#"):
        key, value = line.split("=", 1)
        settings[key] = value

assert settings["CONFIG_IDF_TARGET"] == '"esp32c3"'
assert settings["CONFIG_ML_MAX_PEERS"] == "8"
assert settings["CONFIG_ML_NVS_MAX_PEERS"] == "16"
assert settings["CONFIG_ML_H2_BUFFER_SIZE_KB"] == "64"
assert settings["CONFIG_ML_JSON_BUFFER_SIZE_KB"] == "64"
