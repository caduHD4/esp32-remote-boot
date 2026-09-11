#!/usr/bin/env bash

rb_remote_boot_ids() {
    sed -n 's/^Boot\([0-9A-Fa-f]\{4\}\)\*\{0,1\}[[:space:]]\+Remote Boot iPXE\([[:space:]]\+HD(.*\)\{0,1\}$/\1/p'
}
