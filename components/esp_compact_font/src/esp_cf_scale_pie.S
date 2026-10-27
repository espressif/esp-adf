/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/*
 * RISC-V PIE helpers for compact-font alpha resampling.
 */
#include "soc/soc_caps.h"

#if SOC_CPU_HAS_PIE

    .text
    .align  4

/*
 * void esp_cf_u8_avg2_rows_asm(const uint8_t *a, const uint8_t *b, uint8_t *dst,
 *                             int n)
 * n must be a positive multiple of 4. Rounded byte average:
 *   dst = (a | b) - (((a ^ b) >> 1) & 0x7F7F7F7F)
 */
    .type   esp_cf_u8_avg2_rows_asm, @function
    .global esp_cf_u8_avg2_rows_asm
esp_cf_u8_avg2_rows_asm:
    blez    a3, 2f
1:
    lw      t0, 0(a0)
    lw      t1, 0(a1)
    or      t2, t0, t1
    xor     t3, t0, t1
    srli    t3, t3, 1
    li      t4, 0x7F7F7F7F
    and     t3, t3, t4
    sub     t2, t2, t3
    sw      t2, 0(a2)
    addi    a0, a0, 4
    addi    a1, a1, 4
    addi    a2, a2, 4
    addi    a3, a3, -4
    bgtz    a3, 1b
2:
    ret
    .size   esp_cf_u8_avg2_rows_asm, .-esp_cf_u8_avg2_rows_asm

/*
 * void esp_cf_box_h_row_36_28_asm(const uint8_t *src, uint8_t *dst)
 * Fixed-ratio horizontal box filter (36 -> 28).
 */
    .type   esp_cf_box_h_row_36_28_asm, @function
    .global esp_cf_box_h_row_36_28_asm
    .align  4
    .global esp_cf_box_h_36_28_tbl
esp_cf_box_h_36_28_tbl:
    .byte   0, 1,  1, 1,  2, 1,  3, 2,  5, 1,  6, 1,  7, 2,  9, 1
    .byte  10, 1, 11, 1, 12, 2, 14, 1, 15, 1, 16, 2, 18, 1, 19, 1
    .byte  20, 1, 21, 2, 23, 1, 24, 1, 25, 2, 27, 1, 28, 1, 29, 1
    .byte  30, 2, 32, 1, 33, 1, 34, 2

esp_cf_box_h_row_36_28_asm:
    la      t0, esp_cf_box_h_36_28_tbl
    li      t1, 28
1:
    lbu     t2, 0(t0)
    lbu     t3, 1(t0)
    addi    t0, t0, 2
    add     t4, a0, t2
    li      t5, 2
    beq     t3, t5, 2f
    lbu     t6, 0(t4)
    sb      t6, 0(a1)
    j       3f
2:
    lbu     t6, 0(t4)
    lbu     t2, 1(t4)
    add     t6, t6, t2
    addi    t6, t6, 1
    srli    t6, t6, 1
    sb      t6, 0(a1)
3:
    addi    a1, a1, 1
    addi    t1, t1, -1
    bgtz    t1, 1b
    ret
    .size   esp_cf_box_h_row_36_28_asm, .-esp_cf_box_h_row_36_28_asm

#endif /* SOC_CPU_HAS_PIE */
