#!/usr/bin/env python3
"""Validate the public discovery profile used by the blind iPXE builder.

The profile is public: it never contains tokens, Wi-Fi credentials or private keys.
"""
from pathlib import Path
import hashlib
import ipaddress
import json
import sys

REQUIRED = {
    "profile_version", "protocol", "discovery_version", "pc_id", "blind_mode",
    "mode_revision", "device_id", "public_key", "fingerprint", "last_ipv4",
    "http_port", "ws_port", "hostname",
}


def fail(message):
    raise SystemExit("invalid discovery profile: " + message)


def hex_value(value, length, name):
    if not isinstance(value, str) or len(value) != length or value != value.lower():
        fail(f"{name} must be {length} lowercase hexadecimal characters")
    if any(ch not in "0123456789abcdef" for ch in value):
        fail(f"{name} is not hexadecimal")
    return value


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail("duplicate field: " + key)
        result[key] = value
    return result


def load(path):
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    except (OSError, ValueError) as error:
        fail(str(error))
    if not isinstance(document, dict):
        fail("top level must be an object")
    unknown = set(document) - REQUIRED
    missing = REQUIRED - set(document)
    if unknown:
        fail("unknown fields: " + ", ".join(sorted(unknown)))
    if missing:
        fail("missing fields: " + ", ".join(sorted(missing)))
    for name in ("profile_version", "protocol", "discovery_version", "mode_revision", "http_port", "ws_port"):
        if type(document[name]) is not int:
            fail(f"{name} must be an integer")
    if document["profile_version"] != 1 or document["protocol"] != 2 or document["discovery_version"] != 1:
        fail("unsupported profile/protocol/discovery version")
    if not 1 <= document["mode_revision"] <= 0xffffffff:
        fail("mode_revision must be a positive uint32")
    if document["http_port"] != 80 or document["ws_port"] != 81:
        fail("this version requires http_port=80 and ws_port=81")
    if type(document["blind_mode"]) is not bool:
        fail("blind_mode must be boolean")
    hex_value(document["pc_id"], 32, "pc_id")
    hex_value(document["device_id"], 32, "device_id")
    if document["device_id"] == "0"*32:
        fail("device_id cannot be the unbound listing ID")
    hex_value(document["public_key"], 130, "public_key")
    hex_value(document["fingerprint"], 64, "fingerprint")
    if not document["public_key"].startswith("04"):
        fail("public_key must be an uncompressed SEC1 point")
    point = bytes.fromhex(document["public_key"])
    x, y = int.from_bytes(point[1:33], "big"), int.from_bytes(point[33:], "big")
    prime = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
    curve_b = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
    if x >= prime or y >= prime or (y*y - (x*x*x - 3*x + curve_b)) % prime:
        fail("public_key is not on P-256")
    if hashlib.sha256(bytes.fromhex(document["public_key"])).hexdigest() != document["fingerprint"]:
        fail("fingerprint does not match public_key")
    try:
        address = ipaddress.IPv4Address(document["last_ipv4"])
    except (ValueError, TypeError):
        fail("last_ipv4 is not IPv4")
    if not 0 < int(address) >> 24 < 224 or address.is_loopback or str(address) != document["last_ipv4"]:
        fail("last_ipv4 is not a usable unicast address")
    hostname = document["hostname"]
    if hostname != "remote-boot-" + document["device_id"][-12:] + ".local":
        fail("hostname must be a unique remote-boot-<id>.local name")
    return document


def main(argv):
    if len(argv) < 3 or argv[1] not in ("check", "field"):
        raise SystemExit("usage: discovery_profile.py check FILE | field FILE NAME")
    profile = load(argv[2])
    if argv[1] == "check":
        print("ok")
        return 0
    if len(argv) != 4:
        raise SystemExit("usage: discovery_profile.py field FILE NAME")
    name = argv[3]
    if name not in REQUIRED:
        fail("unknown field")
    value = profile[name]
    if isinstance(value, bool):
        print("true" if value else "false")
    else:
        print(value)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
