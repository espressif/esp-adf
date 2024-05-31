/**
 * Copyright (2019) Baidu Inc. All rights reserved.
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
 * File: lightduer_uuid.h
 * Auth: Chen Chi (chenchi01@baidu.com)
 * Desc: uuid generation
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_UUID_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_UUID_H

#ifdef __cplusplus
extern "C" {
#endif

#define DUER_UUID_STRLEN (37)

void duer_generate_uuid(char *uuid_out);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_UUID_H
