#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
comp=$(cd "$here/.." && pwd)
out=${HOST_CACHE_TEST_OUT:-/tmp/cf_host_cache_test}
bench_out=${HOST_CACHE_BENCH_OUT:-/tmp/cf_host_cache_bench}
cache_source=${CACHE_SOURCE:-"$comp/src/esp_cf_cache.c"}
stub=$(mktemp -d)
trap 'rm -rf "$stub"' EXIT

cat >"$stub/esp_log.h" <<'EOF'
#pragma once
#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
EOF

common=(
    -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -std=c11
    -D_POSIX_C_SOURCE=200809L
    -I"$comp/include" -I"$comp/private_inc" -I"$stub"
    -Wl,--gc-sections
)
if [[ ${HOST_SANITIZE:-0} == 1 ]]; then
    common+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

if [[ ${BENCH_ONLY:-0} != 1 ]]; then
    "${CC:-gcc}" "${common[@]}" \
        -o "$out" \
        "$here/host_cache_test.c" \
        "$cache_source"

    "$out"
fi

"${CC:-gcc}" "${common[@]}" \
    -o "$bench_out" \
    "$here/host_cache_bench.c" \
    "$cache_source"

"$bench_out"
