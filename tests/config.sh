#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temp=$(mktemp -d); trap 'rm -rf "$temp"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -I"$root/.pio/libdeps/esp32c3_4mb/ArduinoJson/src" "$root/tests/test_config.cpp" -o "$temp/test_config"
"$temp/test_config"
