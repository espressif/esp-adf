/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/*
 * RISC-V PIE helper for esp_compact_font_blend_rgb565.
 */
#include "soc/soc_caps.h"

#if SOC_CPU_HAS_PIE

    .text
    .align  4

/*
 * void esp_cf_blend_rgb565_asm(uint16_t *row, int x, const uint8_t *alpha,
 *                             int w, uint16_t color)
 * Blend fg565 onto row[x..x+w) using (a*fg + (256-a)*dst)>>8.
 */
    .type   esp_cf_blend_rgb565_asm, @function
    .global esp_cf_blend_rgb565_asm
esp_cf_blend_rgb565_asm:
    blez    a3, 2f
    slli    t6, a1, 1
    add     a0, a0, t6
1:
    lbu     t0, 0(a2)
    beqz    t0, 3f
    li      t1, 255
    beq     t0, t1, 4f
    lhu     t1, 0(a0)
    srli    t2, t1, 11
    slli    t3, t2, 3
    srli    t2, t2, 2
    or      t3, t3, t2
    srli    t2, t1, 5
    andi    t2, t2, 0x3F
    slli    t4, t2, 2
    srli    t2, t2, 4
    or      t4, t4, t2
    andi    t2, t1, 0x1F
    slli    t5, t2, 3
    srli    t2, t2, 2
    or      t5, t5, t2
    srli    t2, a4, 11
    slli    t2, t2, 3
    neg     t6, t0
    addi    t6, t6, 256
    mul     t2, t2, t0
    mul     t3, t3, t6
    add     t2, t2, t3
    srli    t2, t2, 8
    srli    t3, a4, 5
    andi    t3, t3, 0x3F
    slli    t3, t3, 2
    mul     t3, t3, t0
    mul     t4, t4, t6
    add     t3, t3, t4
    srli    t3, t3, 8
    andi    t4, a4, 0x1F
    slli    t4, t4, 3
    mul     t4, t4, t0
    mul     t5, t5, t6
    add     t4, t4, t5
    srli    t4, t4, 8
    srli    t2, t2, 3
    slli    t2, t2, 11
    srli    t3, t3, 2
    slli    t3, t3, 5
    or      t2, t2, t3
    srli    t4, t4, 3
    or      t2, t2, t4
    sh      t2, 0(a0)
    j       3f
4:
    sh      a4, 0(a0)
3:
    addi    a0, a0, 2
    addi    a2, a2, 1
    addi    a3, a3, -1
    bgtz    a3, 1b
2:
    ret
    .size   esp_cf_blend_rgb565_asm, .-esp_cf_blend_rgb565_asm

#endif /* SOC_CPU_HAS_PIE */
