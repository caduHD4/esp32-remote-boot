#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temp=$(mktemp -d); trap 'rm -rf "$temp"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$root/tests/test_core.cpp" -o "$temp/test_core"
"$temp/test_core"
bash "$root/tests/test_discovery.sh"
while IFS= read -r -d '' file; do bash -n "$file"; done < <(find "$root/agent" "$root/installer" "$root/ipxe" "$root/scripts" "$root/tests" -name '*.sh' -print0)
python3 "$root/scripts/scan_secrets.py"
