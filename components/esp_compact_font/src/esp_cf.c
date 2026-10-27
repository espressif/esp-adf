/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Compact font: open an atlas and decode one glyph.
 * GFH, GFZ, and LVGL can stay on an open file and read one frame at a time.
 * The glyph cache lives in esp_cf_cache.c. The allocator comes from the caller.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(ESP_PLATFORM)
#include "esp_memory_utils.h"
#endif  /* defined(ESP_PLATFORM) */

#include "esp_log.h"
#include "esp_compact_font_priv.h"

static const char *TAG = "COMPACT_FONT";

extern const esp_compact_font_desc_t _esp_compact_font_codec_array_start;
extern const esp_compact_font_desc_t _esp_compact_font_codec_array_end;

static const esp_compact_font_desc_t *codec_begin(void)
{
    return &_esp_compact_font_codec_array_start;
}

static const esp_compact_font_desc_t *codec_end(void)
{
    return &_esp_compact_font_codec_array_end;
}

static void *libc_malloc(size_t n, void *ctx)
{
    (void)ctx;
    return malloc(n);
}

static void libc_free(void *p, void *ctx)
{
    (void)ctx;
    free(p);
}

static void alloc_fix(esp_cf_alloc_t *a)
{
    if (!a->malloc) {
        a->malloc = libc_malloc;
    }
    if (!a->free) {
        a->free = libc_free;
    }
    if (!a->retain) {
        a->retain = a->malloc;
    }
    if (!a->release) {
        a->release = a->free;
    }
}

static int alloc_ok(const esp_cf_alloc_t *a)
{
    return a != NULL;
}

void *esp_cf_malloc(esp_cf_t *font, size_t n)
{
    if (!font || !n || !font->alloc.malloc) {
        return NULL;
    }
    return font->alloc.malloc(n, font->alloc.ctx);
}

void esp_cf_free(esp_cf_t *font, void *p)
{
    if (!font || !p || !font->alloc.free) {
        return;
    }
    font->alloc.free(p, font->alloc.ctx);
}

void *esp_cf_retain(esp_cf_t *font, size_t n)
{
    if (!font || !n || !font->alloc.retain) {
        return NULL;
    }
    return font->alloc.retain(n, font->alloc.ctx);
}

void esp_cf_release(esp_cf_t *font, void *p)
{
    if (!font || !p || !font->alloc.release) {
        return;
    }
    font->alloc.release(p, font->alloc.ctx);
}

void esp_cf_set_priv(esp_cf_t *font, void *priv)
{
    if (font) {
        font->priv = priv;
    }
}

void *esp_cf_priv(esp_cf_t *font)
{
    return font ? font->priv : NULL;
}

void esp_cf_set_extra(esp_cf_t *font, size_t bytes)
{
    if (font) {
        font->extra = bytes;
    }
}

const uint8_t *esp_cf_payload(const esp_cf_t *font)
{
    return font ? font->payload : NULL;
}

static const uint8_t *read_scratch(esp_cf_t *font, size_t n)
{
    void *blk;
    size_t cap;

    if (font->work && font->work_cap >= n) {
        return font->work;
    }
    cap = n;
    if (font->work_cap > 0 && font->work_cap <= SIZE_MAX / 2 && cap < font->work_cap * 2) {
        cap = font->work_cap * 2;
    }
    blk = esp_cf_malloc(font, cap);
    if (!blk) {
        return NULL;
    }
    /* The smaller block is released only when a larger read replaces it.
     * The live block stays until close. */
    esp_cf_free(font, font->work);
    font->work = blk;
    font->work_cap = cap;
    return blk;
}

const uint8_t *esp_cf_read(esp_cf_t *font, size_t off, void *dst, size_t n)
{
    uint8_t *out;
    size_t got = 0;

    if (!font || off > font->file_len || n > font->file_len - off) {
        return NULL;
    }
    if (!dst && font->file) {
        return font->file + off;
    }
    if (n == 0) {
        return dst;
    }
    if (!dst) {
        dst = (void *)read_scratch(font, n);
        if (!dst) {
            return NULL;
        }
    }
    out = dst;
    if (font->file) {
        memcpy(out, font->file + off, n);
        return out;
    }
    if (font->src_fd < 0 || lseek(font->src_fd, (off_t)off, SEEK_SET) < 0) {
        return NULL;
    }
    while (got < n) {
        ssize_t rd = read(font->src_fd, out + got, n - got);

        if (rd < 0) {
            if (errno == EINTR) {
                continue;
            }
            return NULL;
        }
        if (rd == 0) {
            return NULL;
        }
        got += (size_t)rd;
    }
    return out;
}

int esp_compact_font_get_format_count(void)
{
    return (int)(codec_end() - codec_begin());
}

const char *esp_compact_font_get_format_ext(int index)
{
    if (index < 0 || index >= esp_compact_font_get_format_count()) {
        ESP_LOGE(TAG, "Format ext failed: index %d is out of range", index);
        return NULL;
    }
    return codec_begin()[index].ext;
}

static const esp_cf_codec_t *codec_for_magic(uint32_t magic)
{
    const esp_compact_font_desc_t *it;

    for (it = codec_begin(); it != codec_end(); ++it) {
        if (it->magic == magic) {
            return it;
        }
    }
    return NULL;
}

static int glyph_cmp(const void *a, const void *b)
{
    uint32_t ca = ((const esp_cf_glyph_t *)a)->cp;
    uint32_t cb = ((const esp_cf_glyph_t *)b)->cp;
    return (ca > cb) - (ca < cb);
}

const esp_cf_glyph_t *esp_cf_glyph(const esp_cf_t *font, uint32_t cp)
{
    esp_cf_glyph_t key;

    if (!font || !font->glyphs || font->count == 0) {
        return NULL;
    }
    key.cp = cp;
    return bsearch(&key, font->glyphs, font->count, sizeof(key), glyph_cmp);
}

const esp_cf_glyph_t *esp_cf_glyph_at(const esp_cf_t *font, uint32_t index)
{
    if (!font || !font->glyphs || index >= font->count) {
        return NULL;
    }
    return &font->glyphs[index];
}

static void fill_metrics(esp_compact_font_glyph_t *out, const uint8_t *alpha,
                         const esp_cf_glyph_t *g)
{
    if (!out) {
        return;
    }
    out->alpha = alpha;
    out->w = g->w;
    out->h = g->h;
    out->left = g->left;
    out->top = g->top;
    out->advance = g->advance;
}

static esp_compact_font_err_t decode_glyph(esp_cf_t *font, const esp_cf_glyph_t *g,
                                           uint8_t *dst, size_t dst_cap,
                                           esp_cf_alpha_t *out)
{
    size_t n;

    if (!font || !font->codec || !font->codec->decode || !g) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    if (g->w < 0 || g->h < 0 || (g->w == 0) != (g->h == 0)) {
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    if (g->w == 0) {
        fill_metrics(out, NULL, g);
        return ESP_COMPACT_FONT_OK;
    }
    n = (size_t)g->w * (size_t)g->h;
    if (n > ESP_CF_GLYPH_MAX) {
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    if (!dst || n > dst_cap) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (font->codec->decode(font, g, dst, dst_cap) != 0) {
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    fill_metrics(out, dst, g);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_decode(esp_compact_font_handle_t font, uint32_t cp,
                                               uint8_t *dst, size_t dst_cap,
                                               esp_compact_font_glyph_t *out)
{
    const esp_cf_glyph_t *g;
    esp_compact_font_err_t err;

    if (!font) {
        ESP_LOGE(TAG, "Decode failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    /* A missing codepoint is normal while drawing a page. Do not log it. */
    g = esp_cf_glyph(font, cp);
    if (!g) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    err = decode_glyph(font, g, dst, dst_cap, out);
    if (err == ESP_COMPACT_FONT_ERR_FAIL) {
        ESP_LOGE(TAG, "Decode failed: U+%04X frame is bad", (unsigned)cp);
    }
    return err;
}

esp_compact_font_err_t esp_compact_font_decode_at(esp_compact_font_handle_t font, uint32_t index,
                                                  uint8_t *dst, size_t dst_cap,
                                                  esp_compact_font_glyph_t *out)
{
    esp_compact_font_err_t err;

    if (!font) {
        ESP_LOGE(TAG, "Decode failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (index >= font->count) {
        ESP_LOGE(TAG, "Decode failed: index %u is past %u glyphs",
                 (unsigned)index, (unsigned)font->count);
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    err = decode_glyph(font, esp_cf_glyph_at(font, index), dst, dst_cap, out);
    if (err == ESP_COMPACT_FONT_ERR_FAIL) {
        ESP_LOGE(TAG, "Decode failed: index %u frame is bad", (unsigned)index);
    }
    return err;
}

int esp_cf_probe_first(esp_cf_t *font)
{
    uint32_t i;

    if (!font || !font->codec || !font->codec->decode) {
        return -1;
    }
    for (i = 0; i < font->count; ++i) {
        const esp_cf_glyph_t *g = &font->glyphs[i];
        uint8_t *px;
        size_t need;

        if (g->w <= 0 || g->h <= 0) {
            continue;
        }
        need = (size_t)g->w * (size_t)g->h;
        if (need > ESP_CF_GLYPH_MAX) {
            return -1;
        }
        px = esp_cf_malloc(font, need);
        if (!px) {
            return -1;
        }
        if (font->codec->decode(font, g, px, need) != 0) {
            esp_cf_free(font, px);
            return -1;
        }
        esp_cf_free(font, px);
        return 0;
    }
    return 0;
}

esp_compact_font_err_t esp_compact_font_codepoint(const esp_compact_font_handle_t font,
                                                  uint32_t index, uint32_t *cp)
{
    const esp_cf_glyph_t *g;

    if (!font || !cp) {
        ESP_LOGE(TAG, "Codepoint failed: font or output is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    g = esp_cf_glyph_at(font, index);
    if (!g) {
        ESP_LOGE(TAG, "Codepoint failed: index %u is past %u glyphs",
                 (unsigned)index, (unsigned)font->count);
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    *cp = g->cp;
    return ESP_COMPACT_FONT_OK;
}

static int glyphs_sorted(const esp_cf_glyph_t *g, uint32_t count)
{
    uint32_t i;

    for (i = 1; i < count; ++i) {
        if (g[i - 1].cp >= g[i].cp) {
            return 0;
        }
    }
    return 1;
}

int esp_cf_index_load(esp_cf_t *font, const uint8_t *index, uint32_t count,
                      uint32_t data_off, size_t file_len, int base_px, int entry)
{
    esp_cf_glyph_t *g;

    if (!font || !index || count == 0 || base_px <= 0) {
        return -1;
    }
    if (entry != 16 && entry != 20) {
        return -1;
    }
    if (count > file_len / (size_t)entry || (size_t)data_off > file_len) {
        return -1;
    }
    g = esp_cf_retain(font, (size_t)count * sizeof(*g));
    if (!g) {
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *fe = index + (size_t)i * (size_t)entry;
        int adv;
        int left;
        int top;
        int w;
        int h;

        memcpy(&g[i].cp, fe, 4);
        if (entry == 16) {
            int8_t sadv;
            int8_t sleft;
            int8_t stop;
            int8_t sw;
            int8_t sh;

            memcpy(&g[i].data_off, fe + 4, 4);
            memcpy(&g[i].frame_len, fe + 8, 2);
            memcpy(&sadv, fe + 10, 1);
            memcpy(&sleft, fe + 11, 1);
            memcpy(&stop, fe + 12, 1);
            memcpy(&sw, fe + 13, 1);
            memcpy(&sh, fe + 14, 1);
            adv = sadv;
            left = sleft;
            top = stop;
            w = sw;
            h = sh;
        } else {
            int16_t sadv;
            int16_t sleft;
            int16_t stop;
            uint16_t sw;
            uint16_t sh;

            memcpy(&sadv, fe + 4, 2);
            memcpy(&sleft, fe + 6, 2);
            memcpy(&stop, fe + 8, 2);
            memcpy(&sw, fe + 10, 2);
            memcpy(&sh, fe + 12, 2);
            memcpy(&g[i].data_off, fe + 14, 4);
            memcpy(&g[i].frame_len, fe + 18, 2);
            adv = sadv;
            left = sleft;
            top = stop;
            w = (int)sw;
            h = (int)sh;
        }
        if (esp_cf_store_px(adv, &g[i].advance) != 0 ||
            esp_cf_store_i8(left, &g[i].left) != 0 ||
            esp_cf_store_i8(top, &g[i].top) != 0 ||
            esp_cf_store_px(w, &g[i].w) != 0 ||
            esp_cf_store_px(h, &g[i].h) != 0 ||
            (w == 0) != (h == 0) ||
            ((w != 0 || h != 0) && g[i].frame_len == 0) ||
            (g[i].frame_len != 0 &&
             ((size_t)g[i].data_off > file_len - (size_t)data_off ||
              (size_t)g[i].frame_len > file_len - (size_t)data_off - (size_t)g[i].data_off))) {
            esp_cf_release(font, g);
            return -1;
        }
    }
    if (!glyphs_sorted(g, count)) {
        esp_cf_release(font, g);
        return -1;
    }
    if (font->glyphs) {
        esp_cf_release(font, font->glyphs);
    }
    font->glyphs = g;
    font->count = count;
    font->base_px = base_px;
    font->payload_off = data_off;
    font->payload = font->file ? font->file + data_off : NULL;
    return 0;
}

int esp_cf_glyphs_adopt(esp_cf_t *font, esp_cf_glyph_t *glyphs, uint32_t count,
                        int base_px, const uint8_t *payload)
{
    if (!font || !glyphs || count == 0 || base_px <= 0) {
        return -1;
    }
    if (!payload && font->file) {
        return -1;
    }
    if (!glyphs_sorted(glyphs, count)) {
        return -1;
    }
    if (font->glyphs) {
        esp_cf_release(font, font->glyphs);
    }
    font->glyphs = glyphs;
    font->count = count;
    font->base_px = base_px;
    font->payload_off = 0;
    font->payload = payload;
    return 0;
}

static void src_free(esp_cf_t *font, const uint8_t *file, size_t len,
                     int owned, int fd)
{
    (void)len;
    if (owned && file && font->alloc.free) {
        font->alloc.free((void *)file, font->alloc.ctx);
    }
    if (fd >= 0) {
        close(fd);
    }
}

static void src_clear(esp_cf_t *font)
{
    font->file = NULL;
    font->file_len = 0;
    font->src_owned = 0;
    font->src_fd = -1;
    font->payload_off = 0;
}

static void face_unbind(esp_cf_t *font)
{
    esp_cf_cache_t *cache = font->cache;

    esp_cf_cache_drop(font);
    font->cache = cache;
    if (font->codec && font->codec->release) {
        font->codec->release(font);
    }
    esp_cf_release(font, font->glyphs);
    font->glyphs = NULL;
    font->count = 0;
    font->codec = NULL;
    font->payload_off = 0;
    font->payload = NULL;
    font->base_px = 0;
    font->ascent = 0;
    font->descent = 0;
    font->extra = 0;
    font->priv = NULL;
}

static void metrics_load(esp_cf_t *font)
{
    uint32_t i;
    int ascent = 0;
    int descent = 0;
    int any = 0;

    for (i = 0; i < font->count; ++i) {
        int top = font->glyphs[i].top;
        int bot = top - font->glyphs[i].h;

        if (!any || top > ascent) {
            ascent = top;
        }
        if (!any || bot < descent) {
            descent = bot;
        }
        any = 1;
    }
    font->ascent = any ? ascent : 0;
    font->descent = any ? descent : 0;
}

static esp_compact_font_err_t face_bind(esp_cf_t *font)
{
    uint32_t magic = 0;
    const esp_cf_codec_t *codec;

    if (font->file_len < 4 || !esp_cf_read(font, 0, &magic, 4)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    codec = codec_for_magic(magic);
    if (!codec) {
        ESP_LOGE(TAG, "Source failed: no codec for magic 0x%08x", (unsigned)magic);
        return ESP_COMPACT_FONT_ERR_NOT_SUPPORTED;
    }
    font->codec = codec;
    if (codec->bind(font, font->file, font->file_len) != 0) {
        face_unbind(font);
        ESP_LOGE(TAG, "Source failed: %s bind rejected the file", codec->name);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    font->render_px = font->base_px;
    metrics_load(font);
    ESP_LOGI(TAG, "Source '%s': %u glyphs, %d px", codec->name,
             (unsigned)font->count, font->base_px);
    return ESP_COMPACT_FONT_OK;
}

static esp_compact_font_err_t src_install(esp_cf_t *font, const uint8_t *file, size_t len,
                                          int owned, int fd)
{
    const uint8_t *old_file = font->file;
    size_t old_len = font->file_len;
    int old_owned = font->src_owned;
    int old_fd = font->src_fd;
    esp_compact_font_err_t err;

    face_unbind(font);
    if (!file && fd < 0) {
        src_free(font, old_file, old_len, old_owned, old_fd);
        src_clear(font);
        return ESP_COMPACT_FONT_OK;
    }
    font->file = file;
    font->file_len = len;
    font->src_owned = owned;
    font->src_fd = fd;
    err = face_bind(font);
    if (err != ESP_COMPACT_FONT_OK) {
        src_free(font, file, len, owned, fd);
        font->file = old_file;
        font->file_len = old_len;
        font->src_owned = old_owned;
        font->src_fd = old_fd;
        if ((!old_file && old_fd < 0) || face_bind(font) != ESP_COMPACT_FONT_OK) {
            src_free(font, old_file, old_len, old_owned, old_fd);
            src_clear(font);
        }
        return err;
    }
    src_free(font, old_file, old_len, old_owned, old_fd);
    return ESP_COMPACT_FONT_OK;
}

static int src_in_ram(const void *p)
{
#if defined(ESP_PLATFORM)
    return esp_ptr_internal(p) || esp_ptr_external_ram(p);
#else
    (void)p;
    return 1;
#endif  /* defined(ESP_PLATFORM) */
}

static esp_compact_font_err_t read_file(int fd, uint8_t *buf, size_t len, const char *path)
{
    size_t got = 0;

    while (got < len) {
        ssize_t n = read(fd, buf + got, len - got);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            ESP_LOGE(TAG, "File source failed: short read of %s", path);
            return ESP_COMPACT_FONT_ERR_FAIL;
        }
        if (n == 0) {
            break;
        }
        got += (size_t)n;
    }
    if (got != len) {
        ESP_LOGE(TAG, "File source failed: short read of %s", path);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    return ESP_COMPACT_FONT_OK;
}

static int stream_magic(const uint8_t *mag, size_t n)
{
    uint32_t magic = 0;

    if (n < 4) {
        return 0;
    }
    memcpy(&magic, mag, 4);
    return codec_for_magic(magic) != NULL;
}

static esp_compact_font_err_t read_into_ram(esp_cf_t *font, int fd, size_t len,
                                            const char *path, const uint8_t **out)
{
    uint8_t *buf = font->alloc.malloc(len, font->alloc.ctx);
    esp_compact_font_err_t err;

    if (!buf) {
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    err = read_file(fd, buf, len, path);
    if (err != ESP_COMPACT_FONT_OK) {
        font->alloc.free(buf, font->alloc.ctx);
        return err;
    }
    *out = buf;
    return ESP_COMPACT_FONT_OK;
}

static esp_compact_font_err_t load_file(esp_cf_t *font, const char *path,
                                        const uint8_t **out, size_t *out_len, int *out_fd)
{
    struct stat st;
    int fd;
    size_t len;
    uint8_t magb[8];
    size_t mag_n;
    int stream;
    esp_compact_font_err_t err;

    *out = NULL;
    *out_len = 0;
    *out_fd = -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "File source failed: cannot open %s", path);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    if (fstat(fd, &st) < 0 || st.st_size <= 0 || st.st_size > LONG_MAX) {
        ESP_LOGE(TAG, "File source failed: %s has no usable size", path);
        close(fd);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    len = (size_t)st.st_size;
    mag_n = len < 8 ? len : 8;
    if (mag_n < 4 || read(fd, magb, mag_n) != (ssize_t)mag_n || lseek(fd, 0, SEEK_SET) < 0) {
        ESP_LOGE(TAG, "File source failed: cannot read %s", path);
        close(fd);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    stream = stream_magic(magb, mag_n);
    if (!stream) {
        close(fd);
        return ESP_COMPACT_FONT_ERR_NOT_SUPPORTED;
    }
    if (!font->prefer_ram) {
        *out_len = len;
        *out_fd = fd;
        ESP_LOGI(TAG, "File source streams %u bytes", (unsigned)len);
        return ESP_COMPACT_FONT_OK;
    }
    err = read_into_ram(font, fd, len, path, out);
    if (err == ESP_COMPACT_FONT_OK) {
        close(fd);
        *out_len = len;
        ESP_LOGI(TAG, "File source read %u bytes into RAM", (unsigned)len);
        return ESP_COMPACT_FONT_OK;
    }
    if (err == ESP_COMPACT_FONT_ERR_NO_MEM && stream && lseek(fd, 0, SEEK_SET) >= 0) {
        *out_len = len;
        *out_fd = fd;
        ESP_LOGW(TAG, "File source streams %u bytes: no room to copy", (unsigned)len);
        return ESP_COMPACT_FONT_OK;
    }
    close(fd);
    if (err == ESP_COMPACT_FONT_ERR_NO_MEM) {
        ESP_LOGE(TAG, "File source failed: no memory for %u bytes", (unsigned)len);
    }
    return err;
}

esp_compact_font_err_t esp_compact_font_open(const esp_compact_font_cfg_t *cfg,
                                             esp_compact_font_handle_t *handle)
{
    esp_cf_t *font;
    esp_cf_alloc_t alloc;

    if (!cfg || !handle || !alloc_ok(&cfg->alloc)) {
        ESP_LOGE(TAG, "Open failed: config is invalid");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    alloc = cfg->alloc;
    alloc_fix(&alloc);
    font = alloc.malloc(sizeof(*font), alloc.ctx);
    if (!font) {
        ESP_LOGE(TAG, "Open failed: allocator returned no memory");
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    memset(font, 0, sizeof(*font));
    font->alloc = alloc;
    font->src_fd = -1;
    font->prefer_ram = cfg->prefer_ram ? 1 : 0;
    *handle = font;
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_close(esp_compact_font_handle_t font)
{
    esp_cf_alloc_t alloc;
    const char *name;

    if (!font) {
        return ESP_COMPACT_FONT_OK;
    }
    name = font->codec ? font->codec->name : "";
    face_unbind(font);
    esp_cf_free(font, font->work);
    font->work = NULL;
    font->work_cap = 0;
    src_free(font, font->file, font->file_len, font->src_owned, font->src_fd);
    alloc = font->alloc;
    alloc.free(font, alloc.ctx);
    ESP_LOGI(TAG, "Close '%s': released", name);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_set_mem_src(esp_compact_font_handle_t font,
                                                    const uint8_t *mem, size_t len)
{
    if (!font) {
        ESP_LOGE(TAG, "Memory source failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (mem && len < 4) {
        ESP_LOGE(TAG, "Memory source failed: file is shorter than 4 bytes");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (mem && font->prefer_ram && !src_in_ram(mem)) {
        uint8_t *copy = font->alloc.malloc(len, font->alloc.ctx);

        if (copy) {
            memcpy(copy, mem, len);
            ESP_LOGI(TAG, "Memory source copied %u bytes into RAM", (unsigned)len);
            return src_install(font, copy, len, 1, -1);
        }
        ESP_LOGW(TAG, "Memory source left in place: no room to copy %u bytes",
                 (unsigned)len);
    }
    return src_install(font, mem, mem ? len : 0, 0, -1);
}

esp_compact_font_err_t esp_compact_font_set_file_src(esp_compact_font_handle_t font,
                                                     const char *path)
{
    const uint8_t *file = NULL;
    size_t len = 0;
    int fd = -1;
    esp_compact_font_err_t err;

    if (!font) {
        ESP_LOGE(TAG, "File source failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (!path) {
        return src_install(font, NULL, 0, 0, -1);
    }
    err = load_file(font, path, &file, &len, &fd);
    if (err != ESP_COMPACT_FONT_OK) {
        return err;
    }
    return src_install(font, file, len, file ? 1 : 0, fd);
}

esp_compact_font_err_t esp_compact_font_exists(const esp_compact_font_handle_t font,
                                               uint32_t cp, bool *existed)
{
    if (!font || !existed) {
        ESP_LOGE(TAG, "Exists failed: font or output is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    *existed = esp_cf_glyph(font, cp) != NULL;
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_advance(const esp_compact_font_handle_t font,
                                                uint32_t cp, int *adv)
{
    const esp_cf_glyph_t *g;
    int pixel;

    if (!font || !adv) {
        ESP_LOGE(TAG, "Advance failed: font or output is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    pixel = font->render_px > 0 ? font->render_px : font->base_px;
    if (pixel <= 0) {
        ESP_LOGE(TAG, "Advance failed: pixel size is not set");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    g = esp_cf_glyph(font, cp);
    if (!g) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    *adv = esp_cf_scale_metric(g->advance, pixel, font->base_px);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_metrics(const esp_compact_font_handle_t font,
                                                esp_compact_font_metrics_t *out)
{
    int pixel;

    if (!font || !out) {
        ESP_LOGE(TAG, "Metrics failed: font or output is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    pixel = font->render_px > 0 ? font->render_px : font->base_px;
    if (pixel <= 0 || font->base_px <= 0) {
        ESP_LOGE(TAG, "Metrics failed: pixel size is not set");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    out->ascent = esp_cf_scale_metric(font->ascent, pixel, font->base_px);
    out->descent = esp_cf_scale_metric(font->descent, pixel, font->base_px);
    return ESP_COMPACT_FONT_OK;
}

int esp_compact_font_get_base_pixel(const esp_compact_font_handle_t font)
{
    return font ? font->base_px : 0;
}

esp_compact_font_err_t esp_compact_font_set_pixel(esp_compact_font_handle_t font, int pixel)
{
    if (!font || pixel <= 0 || pixel > 80) {
        ESP_LOGE(TAG, "Set pixel failed: font or pixel size is invalid");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    font->render_px = pixel;
    return ESP_COMPACT_FONT_OK;
}

int esp_compact_font_get_pixel(const esp_compact_font_handle_t font)
{
    if (!font) {
        return 0;
    }
    return font->render_px > 0 ? font->render_px : font->base_px;
}

const char *esp_compact_font_name(const esp_compact_font_handle_t font)
{
    if (!font || !font->codec) {
        return "";
    }
    return font->codec->name;
}

uint32_t esp_compact_font_glyph_count(const esp_compact_font_handle_t font)
{
    return font ? font->count : 0;
}
