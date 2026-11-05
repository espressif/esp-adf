#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: Apache-2.0
#
# Build the host twin of the font benchmark and check it against the
# published corpus (about 9618 glyphs, 36 px, no cache).
#
#   ./host_bench.sh [atlas_dir]
#
# atlas_dir defaults to ./fonts. Generate those files with prepare_fonts.py.

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
comp=$(cd "$here/../.." && pwd)
atlas=${1:-"$here/fonts"}
cc=${CC:-gcc}
out=${HOST_BENCH_OUT:-/tmp/cf_host}
bench_source=${HOST_BENCH_SOURCE:-"$here/host_bench.c"}
cache_source=${CACHE_SOURCE:-"$comp/src/esp_cf_cache.c"}

# Published file sizes for the prepare_fonts.py corpus, in KiB rounded up.
expect_kb() {
    case "$1:$2" in
        1:lvgl) echo 1335 ;;
        1:gfh) echo 788 ;;
        2:lvgl) echo 1778 ;;
        2:gfh) echo 1474 ;;
        4:lvgl) echo 3066 ;;
        4:gfh) echo 2970 ;;
        *) return 1 ;;
    esac
}

atlas_ok() {
    local path=$1
    local size mag tag

    [[ -f "$path" ]] || return 1
    size=$(stat -c%s "$path")
    [[ "$size" -ge 65536 ]] || return 1
    case "$path" in
        *.gfh)
            mag=$(od -An -t x1 -N 4 "$path" | tr -d ' \n')
            [[ "$mag" == "47464831" ]]
            ;;
        *.bin)
            tag=$(dd if="$path" bs=1 skip=4 count=4 status=none 2>/dev/null || true)
            [[ "$tag" == "head" ]]
            ;;
        *)
            return 1
            ;;
    esac
}

need_atlas() {
    local name path
    local missing=0

    for name in \
        lvgl_1bpp.bin lvgl_2bpp.bin lvgl_4bpp.bin \
        gfh_1bpp.gfh gfh_2bpp.gfh gfh_4bpp.gfh
    do
        path="$atlas/$name"
        if ! atlas_ok "$path"; then
            echo "atlas not ready: $path" >&2
            missing=1
        fi
    done
    if [[ "$missing" -ne 0 ]]; then
        echo "Generate the corpus first: python3 \"$here/prepare_fonts.py\"" >&2
        exit 1
    fi
}

stub=""
log=""
cleanup() {
    [[ -n "$stub" ]] && rm -rf "$stub"
    [[ -n "$log" ]] && rm -f "$log"
    true
}
trap cleanup EXIT

compile_host() {
    local inc_log

    inc_log="$here/../../../../pc/include"
    if [[ ! -f "$inc_log/esp_log.h" ]]; then
        stub=$(mktemp -d)
        inc_log=$stub
        cat >"$stub/esp_log.h" <<'EOF'
#pragma once
#include <stdio.h>
#ifndef ESP_LOGE
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
#endif
EOF
    fi

    echo "cc -O2 -> $out"
    "$cc" -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE \
        -I"$comp/include" -I"$comp/private_inc" -I"$inc_log" \
        -o "$out" \
        "$bench_source" \
        "$comp/src/esp_cf.c" \
        "$cache_source" \
        "$comp/src/esp_cf_gfh.c" \
        "$comp/src/gfh_dec.c" \
        "$comp/src/esp_cf_lvgl.c" \
        "$comp/src/esp_cf_scale.c" \
        "$comp/src/esp_cf_blend.c" \
        "$comp/src/esp_cf_draw.c" \
        -Wl,-T,"$here/host_codec.ld"
    if [[ -n "$stub" ]]; then
        rm -rf "$stub"
        stub=""
    fi
}

field() {
    local line=$1 key=$2
    printf '%s\n' "$line" | sed -n "s/.*${key}=\\([^ ]*\\).*/\\1/p"
}

check_log() {
    local log=$1
    local bit line lv_g gf_g lv_kb gf_kb lv_bad gf_bad lv_us gf_us px
    local fail=0

    if grep -E -q 'codec=(lvgl|gfh) missing' "$log"; then
        echo "bench reported a missing atlas" >&2
        fail=1
    fi

    printf '%s\n' "bpp  LVGL_KB  GFH_KB  LVGL_us  GFH_us  glyphs  bad"
    for bit in 1 2 4; do
        line=$(grep -E "^COMPARE bit=${bit} " "$log" || true)
        if [[ -z "$line" ]]; then
            echo "no COMPARE line for ${bit} bpp" >&2
            fail=1
            continue
        fi
        lv_g=$(field "$line" lvgl_glyphs)
        gf_g=$(field "$line" gfh_glyphs)
        lv_kb=$(field "$line" lvgl_file_kb)
        gf_kb=$(field "$line" gfh_file_kb)
        lv_bad=$(field "$line" lvgl_bad)
        gf_bad=$(field "$line" gfh_bad)
        lv_us=$(field "$line" lvgl_us_glyph)
        gf_us=$(field "$line" gfh_us_glyph)
        printf '%-3s %7s %7s %8s %7s %7s  %s/%s\n' \
            "$bit" "$lv_kb" "$gf_kb" "$lv_us" "$gf_us" "$lv_g" "$lv_bad" "$gf_bad"

        if [[ "$lv_g" != "$gf_g" || "$lv_g" == "0" ]]; then
            echo "${bit} bpp: glyph counts differ or are zero (${lv_g} vs ${gf_g})" >&2
            fail=1
        fi
        if [[ "$lv_bad" != "0" || "$gf_bad" != "0" ]]; then
            echo "${bit} bpp: decode errors lvgl_bad=${lv_bad} gfh_bad=${gf_bad}" >&2
            fail=1
        fi
        if [[ "$lv_kb" != "$(expect_kb "$bit" lvgl)" || "$gf_kb" != "$(expect_kb "$bit" gfh)" ]]; then
            echo "${bit} bpp: file size ${lv_kb}/${gf_kb} KiB, expected $(expect_kb "$bit" lvgl)/$(expect_kb "$bit" gfh)" >&2
            fail=1
        fi

        px=$(field "$(grep -E "^BENCH bit=${bit} codec=lvgl " "$log" | head -n 1)" px)
        if [[ "$px" != "36" ]]; then
            echo "${bit} bpp: base pixel is ${px:-missing}, expected 36" >&2
            fail=1
        fi
    done

    if [[ "$fail" -ne 0 ]]; then
        exit 1
    fi
    echo "host bench ok"
}

compile_host
need_atlas

if [[ -n ${REAL_CACHE_FILE:-} ]]; then
    if [[ -n ${REAL_CACHE_TEXT:-} ]]; then
        "$out" "$REAL_CACHE_FILE" "$REAL_CACHE_TEXT" "${REAL_CACHE_SLOTS:-512}"
    else
        "$out" "$REAL_CACHE_FILE"
    fi
    exit
fi

log=$(mktemp)
set +e
"$out" "$atlas" | tee "$log"
rc=${PIPESTATUS[0]}
set -e
if [[ "$rc" -ne 0 ]]; then
    echo "host bench exited ${rc}" >&2
    exit "$rc"
fi
check_log "$log"
