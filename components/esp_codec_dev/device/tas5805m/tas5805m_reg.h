/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TAS5805M_REG_H_
#define _TAS5805M_REG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TAS5805M_REG_00         0x00
#define TAS5805M_REG_02         0x02
#define TAS5805M_REG_03         0x03
#define TAS5805M_REG_24         0x24
#define TAS5805M_REG_25         0x25
#define TAS5805M_REG_26         0x26
#define TAS5805M_REG_27         0x27
#define TAS5805M_REG_28         0x28
#define TAS5805M_REG_29         0x29
#define TAS5805M_REG_2A         0x2a
#define TAS5805M_REG_2B         0x2b
#define TAS5805M_REG_35         0x35
#define TAS5805M_REG_7E         0x7e
#define TAS5805M_REG_7F         0x7f

#define TAS5805M_PAGE_00        0x00
#define TAS5805M_PAGE_2A        0x2a

#define TAS5805M_BOOK_00        0x00
#define TAS5805M_BOOK_8C        0x8c

#define MASTER_VOL_REG_ADDR     0X4C
#define MUTE_TIME_REG_ADDR      0X51

#define TAS5805M_DAMP_MODE_BTL  0x0
#define TAS5805M_DAMP_MODE_PBTL 0x04

#ifdef __cplusplus
}
#endif

#endif
