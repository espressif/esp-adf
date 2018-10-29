/*
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
 *
 * File: lightduer_ota_installer.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA installer head file
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_INSTALLER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_INSTALLER_H

#define PACKAGE_NAME_LENGTH_MAX (15)
#define PACKAGE_TYPE_OS         (1)
#define PACKAGE_TYPE_APP        (2)
#define PACKAGE_NO_UPDATE       (0)
#define PACKAGE_UPDATE          (1)

typedef struct _duer_meta_data_basic_info_s {
	char package_name[PACKAGE_NAME_LENGTH_MAX + 1];
	unsigned char package_type;
	unsigned char package_update;
} duer_ota_meta_data_basic_info_t;

#define MODULE_NAME_LENGTH_MAX (PACKAGE_NAME_LENGTH_MAX)
#define MODULE_VERSION_LENGTH_MAX   (15)
#define HARDWARE_VERSION_LENGTH_MAX (15)
#define MODULE_SIGNATURE_LENGTH     (33)
#define MODULE_TYPE_BIN             (1)
#define MODULE_NO_UPDATE            (0)
#define MODULE_UPDATE               (1)

typedef struct _duer_module_info_s {
	char module_name[MODULE_NAME_LENGTH_MAX + 1];
	char module_version[MODULE_VERSION_LENGTH_MAX + 1];
	char module_support_hw_version[HARDWARE_VERSION_LENGTH_MAX + 1];
	unsigned char module_signature[MODULE_SIGNATURE_LENGTH];
	unsigned int module_size;
	unsigned char module_type;
	unsigned char module_update;
} duer_ota_module_info_t;

#define PACKAGE_INSTALL_PATH_MAX  (255)
typedef struct _duer_meta_data_install_info_s {
	char package_install_path[PACKAGE_INSTALL_PATH_MAX + 1];
	unsigned int module_count;
	duer_ota_module_info_t *module_list;
} duer_ota_meta_data_install_info_t;

#define PACKAGE_VERSION_LENGTH MODULE_VERSION_LENGTH_MAX
typedef struct _duer_meta_data_update_info_s {
	char package_version[PACKAGE_VERSION_LENGTH];
} duer_ota_meta_data_update_info_t;

typedef struct _duer_meta_data_extension_s {
	unsigned int pair_count;
	char **keys;
	char **values;
} duer_ota_meta_data_extension_t;

typedef struct _duer_package_meta_data_s {
    duer_ota_meta_data_basic_info_t basic_info;
    duer_ota_meta_data_install_info_t install_info;
    duer_ota_meta_data_update_info_t update_info;
    duer_ota_meta_data_extension_t extension_info;
} duer_ota_package_meta_data_t;

typedef struct _duer_ota_installer_s {
    /*
     * You can open do someting before receive data
     * Such as open file, Initialise Flash, Initialise DMA ...
     */
    int (*notify_data_begin)(void *cxt);

    /*
     * You can get install information from meta param
     * If you want to use this, Call me
     */
	int (*notify_meta_data)(void *cxt, duer_ota_package_meta_data_t *meta);

    /*
     * You can get the file which you upload to Duer Cloud
     */
	int (*notify_module_data)(
            void *cxt,
            unsigned int offset,
            unsigned char *data,
            unsigned int size);

    /*
     * You can do some thing after you get the file
     * Such as close file, free resource
     */
	int (*notify_data_end)(void *cxt);

    /*
     * You can do someting before update the firmware
     * Such as Erase Flash ...
     */
	int (*update_img_begin)(void *cxt);

    /*
     * Burn the new firmware to the Flash
     */
	int (*update_img)(void *cxt);

    /*
     * You can check the new firmware, and select the boot partition
     */
	int (*update_img_end)(void *cxt);
} duer_ota_installer_t;

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_INSTALLER_H
