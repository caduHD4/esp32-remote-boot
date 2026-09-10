#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


def credential(value, field):
    if not isinstance(value, str):
        raise ValueError(f"{field} must be a string")
    return value


def c_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def generate(source, output):
    try:
        if not source.exists():
            ssid = ""
            password = ""
        else:
            config = json.loads(source.read_text(encoding="utf-8"))
            ssid = credential(config.get("ssid"), "ssid")
            password = credential(config.get("wifi_password", ""), "wifi_password")
            if not 1 <= len(ssid) <= 32:
                raise ValueError("ssid must contain 1 to 32 characters")
            if len(password) > 63:
                raise ValueError("wifi_password must contain at most 63 characters")
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"config.local.json: {error}", file=sys.stderr)
        raise ValueError from error

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "#pragma once\n"
        f'#define REMOTE_BOOT_LOCAL_WIFI_SSID "{c_string(ssid)}"\n'
        f'#define REMOTE_BOOT_LOCAL_WIFI_PASSWORD "{c_string(password)}"\n',
        encoding="utf-8",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        generate(arguments.input, arguments.output)
    except ValueError:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
else:
    Import("env")
    root = Path(env["PROJECT_DIR"])
    try:
        generate(root / "config.local.json", root / "firmware/include/local_wifi.generated.h")
    except ValueError:
        raise SystemExit(1)
