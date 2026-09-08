#!/usr/bin/env bash
# Called by the authenticated heartbeat loop. Never evaluates remote shell text.
# Variables below are supplied by agent.sh (or the mock harness).
# shellcheck disable=SC2154
rb_shutdown_command() {
    local reply=$1 payload=$2 command_id command_session confirmation
    [[ $allow_shutdown == true ]] || return 0
    command_id=$(jq -r '.command.id//empty' <<< "$reply")
    command_session=$(jq -r '.command.session_id//empty' <<< "$reply")
    [[ $(jq -r '.command.action//empty' <<< "$reply") == shutdown &&
       $command_id =~ ^[0-9a-f]{32}$ && $command_session == "$session_id" &&
       $command_id != "$ack" ]] || return 0
    # Record before acknowledgment: a lost reply must never execute twice.
    mkdir -p "$(dirname -- "$power_ack_path")"
    printf '%s' "$command_id" > "$power_ack_path"
    ack=$command_id
    if confirmation=$(rb_api POST heartbeat "$(jq -c --arg ack "$ack" '.ack=$ack' <<< "$payload")") &&
       [[ $(jq -r '.ack_accepted//false' <<< "$confirmation") == true ]]; then
        systemctl poweroff
    else
        echo 'Shutdown canceled: acknowledgment rejected or unavailable' >&2
    fi
}
