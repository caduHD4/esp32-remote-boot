#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/agent/linux/power.sh"
temp=$(mktemp -d); trap 'rm -rf "$temp"' EXIT
power_ack_path=$temp/ack
session_id=session-test
allow_shutdown=true
ack=''; calls=0; accepted=true
rb_api() { printf '{"ack_accepted":%s}' "$accepted"; }
systemctl() { [[ $1 == poweroff ]]; calls=$((calls+1)); }
reply='{"command":{"id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","action":"shutdown","session_id":"session-test"}}'
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 ]]
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 ]]
ack=''; allow_shutdown=false
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 && -z $ack ]]
allow_shutdown=true; session_id=new-session
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 && -z $ack ]]
session_id=session-test; accepted=false
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 ]]
ack=''; rb_api() { return 1; }
rb_shutdown_command "$reply" '{}'
[[ $calls == 1 ]]
echo 'Linux shutdown dispatch: passed (poweroff mocked)'
