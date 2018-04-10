/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _GOOGLE_TRANSLATE_H_
#define _GOOGLE_TRANSLATE_H_

#include "esp_err.h"
#include "audio_event_iface.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief      Translate `text` from `lang_from` to `lang_to` languages with google `api_key`
 *
 * @param[in]  text       The text to translate
 * @param[in]  lang_from  The language from
 * @param[in]  lang_to    The language to
 * @param[in]  api_key    The api key
 *
 * @return     The translated text
 */
char *google_translate(const char *text, const char *lang_from, const char *lang_to, const char *api_key);

#ifdef __cplusplus
}
#endif

#endif
