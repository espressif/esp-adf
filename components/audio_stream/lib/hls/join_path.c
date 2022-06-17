/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <string.h>
#include "audio_mem.h"
#include "join_path.h"

static char* get_slash(char* s, int len, int dir)
{
    if (len == 0) {
        return NULL;
    }
    if (dir) {
        while (len-- > 0) {
           if (s[len] == '/') {
               return &s[len];
           }
        }
    } else {
        char* e = s + len;
        while (s < e) {
            if (*(s++) == '/') {
                if (*s == '/') {
                   s++;
                   continue;
                }
                return s-1;
            }
        }
    }
    return NULL;
}

char* join_url(char* base, char* ext)
{
    if (memcmp(ext, "http", 4) == 0) {
        return audio_strdup(ext);
    }
    int base_len = strlen(base);
    int ext_len  = strlen(ext);
    int ext_skip = 0;
    char* s;
    if (memcmp(base, "http", 4)) {
        // local path
        char* ask = strchr(ext, '?');
        if (ask > ext) {
            ext_len = ask - ext;
        }
    }
    if (*ext == '/') {
        if (*(ext+1) == '/') {
            s = strstr(base, "//");
        } else {
            s = get_slash(base, base_len, 0);
        }
        if (s == NULL) {
           return NULL;
        }
        base_len = s - base;
    } else if (*ext == '.') {
        s = get_slash(base, base_len, 1);
        if (s == NULL) {
            return NULL;
        }
        base_len = s - base;
        if (ext_len == 1) {
            ext_skip = 1;
        } else if (*(ext+1) == '/') {
           ext_skip = 2;
        } else {
            while (memcmp(ext + ext_skip, "../", 3) == 0) {
                ext_skip += 3;
                s = get_slash(base, base_len, 1);
                if (s == NULL) {
                return NULL;
                }
                base_len = s - base;
            }
        }
        base_len++;
    } else if (*ext == '#') {
    } else if (*ext == '?') {
         char* a = strrchr(base, '?');
         if (a) {
             base_len = a - base;
         }
    } else {
        s = get_slash(base, base_len, 1);
        if (s == NULL) {
            return NULL;
        }
        base_len = s - base + 1;
    }
    int t = base_len + ext_len - ext_skip + 1;
    char* dst = (char*) audio_malloc(t);
    if (dst == NULL) {
        return dst;
    }
    memcpy(dst, base, base_len);
    memcpy(dst + base_len, ext + ext_skip, ext_len - ext_skip);
    dst[t-1] = 0;
    return dst;
}
