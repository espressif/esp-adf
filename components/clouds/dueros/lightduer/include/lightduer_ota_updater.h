/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * File: lightduer_ota_updater.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Updater Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H

#include "lightduer_ota_unpack.h"
#include "lightduer_ota_downloader.h"
#include "mbedtls/md5.h"

#define TRANSACTION_LEN   (65)
#define VERSION_LEN       (16)
#define SIGNATURE_LEN     (129)
#define MD5_LEN           (16)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _duer_ota_switch {
    ENABLE_OTA  =  1,
    DISABLE_OTA = -1,
} duer_ota_switch;

typedef enum _duer_ota_reboot {
    ENABLE_REBOOT  =  1,
    DISABLE_REBOOT = -1,
} duer_ota_reboot;

typedef struct _duer_ota_update_command_s {
    char transaction[TRANSACTION_LEN + 1];
    char version[VERSION_LEN + 1];
    char old_version[VERSION_LEN + 1];
    char url[URL_LEN + 1];
    char signature[SIGNATURE_LEN + 1];
    unsigned int size;
} duer_ota_update_command_t;

typedef struct _duer_ota_updater_s {
    int id;
    duer_ota_unpacker_t *unpacker;
    duer_ota_downloader_t *downloader;
    duer_ota_update_command_t *update_cmd;
    size_t received_data_size;
    mbedtls_md5_context md5_ctx;
} duer_ota_updater_t;

typedef struct _duer_ota_init_ops_s {
/*
 * You need to call duer_ota_unpack_register_updater()
 * to implement duer_ota_installer_t in init_updater callback function
 * Call duer_ota_unpack_set_private_data() to set your privater data
 */
    int (*init_updater)(duer_ota_updater_t *updater);
/*
 * You can free the resource which you create in init_updater callback function
 *
 */
    int (*uninit_updater)(duer_ota_updater_t *updater);
/*
 * Implement reboot system function
 */
    int (*reboot)(void *arg);
} duer_ota_init_ops_t;

typedef void (*OTA_Updater)(int arg, void *update_cmd);

/*
 * Call it to initialize the OTA module
 *
 * @param ops: You need to implement the structure OTAInitOps
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_init_ota(duer_ota_init_ops_t *ops);

/*
 * User Call it to enable/disable OTA
 *
 * @param ops: You need to implement the structure OTAInitOps
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_set_switch(duer_ota_switch flag);

/*
 * Get the OTA status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_get_switch(void);

/*
 * Set the OTA reboot status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_set_reboot(duer_ota_reboot reboot);

/*
 * get the OTA reboot status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_get_reboot(void);

/*
 * get the OTA update command
 *
 * @param void: duer_ota_updater_t *
 *
 * @return Success: duer_ota_update_command_t*
 *         Failed:  NULL
 */
extern duer_ota_update_command_t *duer_ota_get_update_cmd(const duer_ota_updater_t *updater);

/*
 * Create a OTA updater to update the firmware
 *
 * @param update_cmd: Update command
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_update_firmware(duer_ota_update_command_t *update_cmd);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H
