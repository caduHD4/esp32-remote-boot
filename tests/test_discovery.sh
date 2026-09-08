#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=agent/linux/common.sh
source "$root/agent/linux/common.sh"
efibootmgr() { cat "$root/tests/fixtures/efibootmgr.txt"; }
catalog=$(rb_catalog)
jq -e '.systems|length==8' <<< "$catalog" >/dev/null
jq -e '.systems[0].id=="0002" and .systems[1].id=="0001"' <<< "$catalog" >/dev/null
jq -e '.systems[]|select(.id=="0004")|.blocked and .hidden' <<< "$catalog" >/dev/null
jq -e '.systems[]|select(.id=="0005")|.hidden and (.blocked|not)' <<< "$catalog" >/dev/null
rb_target_exists 0001
if rb_target_exists 0004; then exit 1; fi
printf 'PASS: discovery ordering, duplicate IDs, hidden network/USB, recursion, generic loaders\n'
