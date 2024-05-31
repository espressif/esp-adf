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
/**
 * File: lightduer_snprintf.h
 * Auth: Leliang Zhang(zhangleliang@baidu.com)
 * Desc: Provide snprintf/vsnprintf implementation.
 */

#ifndef BAIDU_DUER_LIGHTDUER_SNPRINTF_H
#define BAIDU_DUER_LIGHTDUER_SNPRINTF_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
  *@param s: the buffer to store the formatted output string
  *@param n: the buffer @s size
  *@param format: the output format
  *@param ...: the input param for the formatting
  *@return the length of the formatted string
  */
int lightduer_snprintf(char *s, size_t n, const char *format, ...);

/**
  *@param s: the buffer to store the formatted output string
  *@param n: the buffer @s size
  *@param format: the output format
  *@param ap: the input param for the formatting
  *@return the length of the formatted string
  */
int lightduer_vsnprintf(char *s, size_t n, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_SNPRINTF_H
