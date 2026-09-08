#!/usr/bin/env bash
rb_require() { for cmd in "$@"; do command -v "$cmd" >/dev/null || { printf 'Missing dependency: %s\n' "$cmd" >&2; return 1; }; done; }
rb_api() {
    local method=$1 path=$2 payload=${3:-'{}'}
    [[ $RB_URL =~ ^http://[0-9.]+$ && $RB_TOKEN =~ ^[A-Za-z0-9_-]{24,128}$ ]] || { echo 'Invalid URL or token format' >&2; return 1; }
    printf 'header = "Authorization: Bearer %s"\n' "$RB_TOKEN" |
        curl --config - --silent --show-error --fail --connect-timeout 3 --max-time 8 --request "$method" --header 'Content-Type: application/json' --data-binary "$payload" "$RB_URL/api/v1/$path"
}
rb_catalog() {
    local line id active name detail lower hidden blocked
    local records='[]' order='[]' raw
    raw=$(efibootmgr -v) || return 1
    while IFS= read -r line; do
        if [[ $line =~ ^BootOrder:[[:space:]]*(.*)$ ]]; then order=$(jq -cn --arg s "${BASH_REMATCH[1]}" '$s|split(",")'); fi
        if [[ $line =~ ^Boot([0-9A-Fa-f]{4})(\*?)[[:space:]]+(.*)$ ]]; then
            id=${BASH_REMATCH[1]^^}; active=${BASH_REMATCH[2]}; detail=${BASH_REMATCH[3]}; name=${detail%%$'\t'*}
            # efibootmgr -v separates description from device path with a tab.
            if [[ $name == "$detail" ]]; then name=${detail%% HD\(*}; name=${name%% PciRoot\(*}; name=${name%% VenHw\(*}; fi
            name=${name:0:63}; lower=${detail,,}; blocked=false; hidden=false
            if [[ $lower == *'remote boot'* || $lower == *ipxe* || -z $active ]]; then blocked=true; hidden=true; fi
            if [[ $lower == *ipv4* || $lower == *ipv6* || $lower == *network* || $lower == *usb* || $lower == *dvd* || $lower == *cdrom* ]]; then hidden=true; fi
            records=$(jq -cn --argjson a "$records" --arg id "$id" --arg name "$name" --argjson hidden "$hidden" --argjson blocked "$blocked" '$a+[{id:$id,name:$name,hidden:$hidden,blocked:$blocked}]')
        fi
    done <<< "$raw"
    jq -cn --argjson entries "$records" --argjson order "$order" '{systems:($entries|unique_by(.id)|sort_by(.id as $id|($order|index($id))//65536))}'
}
rb_target_exists() {
    local id=$1
    [[ $id =~ ^[0-9A-Fa-f]{4}$ ]] && rb_catalog | jq -e --arg id "${id^^}" '.systems[]|select(.id==$id and .blocked==false)' >/dev/null
}
