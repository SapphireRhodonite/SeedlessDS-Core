#!/usr/bin/env bash

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-/tmp/port-x86}"
mkdir -p "$OUT"
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror=incompatible-pointer-types \
    -I"$HERE/../../include" \
    -o "$OUT/seedless_port_x86" \
    "$HERE/main.c" "$HERE/platform_posix.c" "$HERE/api_stub.c" -lpthread
echo "==> $OUT/seedless_port_x86"
