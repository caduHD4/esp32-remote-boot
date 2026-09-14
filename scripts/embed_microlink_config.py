#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


def checked_string(value, field):
    if not isinstance(value, str):
        raise ValueError(f"{field} must be a string")
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError(f"{field} must not contain control characters")
    return value


def c_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def sdkconfig_values(path):
    values = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-11]] = None
    return values


def invalidate_stale_sdkconfig(defaults, generated):
    if not generated.exists():
        return False

    expected = sdkconfig_values(defaults)
    current = sdkconfig_values(generated)
    if all(current.get(key) == value for key, value in expected.items()):
        return False

    generated.unlink()
    print(f"Removed stale generated SDK configuration: {generated.name}")
    return True


def generate(source, output):
    try:
        if not source.exists():
            configured = False
            auth_key = ""
            device_name = ""
        else:
            config = json.loads(source.read_text(encoding="utf-8"))
            if not isinstance(config, dict):
                raise ValueError("root must be an object")
            auth_key = checked_string(config.get("auth_key"), "auth_key")
            device_name = checked_string(config.get("device_name", ""), "device_name")
            if not auth_key.startswith("tskey-auth-") or len(auth_key) < 20:
                raise ValueError("auth_key must be a valid tskey-auth key")
            if len(device_name.encode("utf-8")) > 63:
                raise ValueError("device_name must contain at most 63 bytes in UTF-8")
            configured = True
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"config.local.microlink.json: {error}", file=sys.stderr)
        raise ValueError from error

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "#pragma once\n"
        f"#define REMOTE_BOOT_MICROLINK_CONFIGURED {1 if configured else 0}\n"
        f'#define REMOTE_BOOT_MICROLINK_AUTH_KEY "{c_string(auth_key)}"\n'
        f'#define REMOTE_BOOT_MICROLINK_DEVICE_NAME "{c_string(device_name)}"\n',
        encoding="utf-8",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sdkconfig-defaults", type=Path)
    parser.add_argument("--sdkconfig-generated", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.sdkconfig_defaults and arguments.sdkconfig_generated:
            invalidate_stale_sdkconfig(
                arguments.sdkconfig_defaults,
                arguments.sdkconfig_generated,
            )
        generate(arguments.input, arguments.output)
    except ValueError:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
else:
    Import("env")
    project_root = Path(env["PROJECT_DIR"])
    try:
        invalidate_stale_sdkconfig(
            project_root / "sdkconfig.defaults",
            project_root / f"sdkconfig.{env['PIOENV']}",
        )
        generate(
            project_root / "config.local.microlink.json",
            project_root / "firmware/include/microlink_config.generated.h",
        )
    except ValueError:
        raise SystemExit(1)
