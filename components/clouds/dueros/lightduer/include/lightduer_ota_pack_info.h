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
 * File: lightduer_ota_pack_info.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA pack include head file
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACK_INFO_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACK_INFO_H

#include <stdint.h>
#include "lightduer_log.h"

#define KEY_LEN	128

#define _DEBUG_

// package header
typedef struct _duer_package_header_s {
   // 'mbed' verify tag
   unsigned char tag[4];
   // package header size
   uint32_t header_size;
   // package signature size : 1024 bits
   uint32_t package_sig_size;
   // package signature : include meta.json and all modules
   unsigned char package_sig[KEY_LEN];
   // meta.json signature size : 1024 bits
   uint32_t meta_sig_size;
   // meta.json signature
   unsigned char meta_sig[KEY_LEN];
   // meta.json size : used for decompress meta.json from package body
   uint32_t meta_size;
   // package size before decompress
   uint32_t ori_package_size;
} duer_ota_package_header_t;

// all modules included in mbed package is 'js' and mbed executable '.bin' file
typedef enum _duer_module_type {
	ModuleTypeALL,
	ModuleTypeJS,
	ModuleTypeSO,
	ModuleTypeBIN,
	ModuleTypeJSON,
	ModuleTypeIMG
} duer_ota_module_type_t;

typedef struct _duer_file_type_pair_s {
	char *string;
	duer_ota_module_type_t type;
} duer_ota_file_type_t;


typedef enum _duer_package_type {
	PackageTypeApp,
	PackageTypeOS,
	PackageTypeProfile,
	PackageTypeUnknown
} duer_ota_package_type_t;

typedef struct _duer_pack_module_info_s {

   // module name : xx.js / xx.bin
   unsigned char *name;
   // module type : js / bin
   duer_ota_module_type_t type;
   uint32_t  module_size;
   uint8_t  update;
   unsigned char *version;
   unsigned char *hw_version;
   // offset from file begin
   uint32_t offset;
} duer_ota_unpack_module_info_t;

// meta info : used for extract meta data and modules array data
typedef struct _duer_meta_info_s {
   // decompressed meta data size
   uint32_t meta_data_size;
   // decompressed module data offset
   uint32_t module_data_offset;
   // decompressed meta data
   unsigned char *meta_data;
   // json object
   void *meta_object;
} duer_ota_meta_info_t;

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACK_INFO_H
