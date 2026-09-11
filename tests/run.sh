#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temp=$(mktemp -d); trap 'rm -rf "$temp"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$root/tests/test_core.cpp" -o "$temp/test_core"
"$temp/test_core"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "$root/tests/test_power.cpp" -o "$temp/test_power"
"$temp/test_power"
g++ -std=c++17 -Wall -Wextra -Werror "$root/tests/test_setup_network.cpp" -o "$temp/test_setup_network"
"$temp/test_setup_network"
bash "$root/tests/test_shutdown.sh"
bash "$root/tests/test_discovery.sh"
bash "$root/tests/test_efi_helpers.sh"
while IFS= read -r -d '' file; do bash -n "$file"; done < <(find "$root/agent" "$root/installer" "$root/ipxe" "$root/scripts" "$root/tests" -name '*.sh' -print0)
python3 "$root/tests/test_embed_local_wifi.py"
python3 "$root/scripts/scan_secrets.py"
