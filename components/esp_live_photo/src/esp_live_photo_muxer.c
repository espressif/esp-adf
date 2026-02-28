/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "esp_live_photo_muxer.h"
#include "esp_log.h"

#define TAG  "LIVE_PHOTO_MUXER"

/**
 * @brief  Runtime writer context for embedded MP4 output
 */
typedef struct {
    esp_muxer_file_writer_t  io;
    uint32_t                 mp4_size;
    uint32_t                 mp4_offset;
    uint32_t                 pos;
    void                    *ctx;
} mp4_writer_ctx_t;

/**
 * @brief  Internal EXIF fields used for cover JPEG rewrite
 */
typedef struct {
    char      make[32];
    char      model[32];
    char      date_time[72];
    char      date_time_original[20];
    char      date_time_digitized[20];
    uint8_t   exif_version[4];
    uint32_t  pixel_x_dim;
    uint32_t  pixel_y_dim;
    uint16_t  orientation;
    bool      has_exif_version;
} live_photo_exif_info_t;

/**
 * @brief  Live photo muxer instance
 */
typedef struct {
    esp_muxer_handle_t      mp4_muxer;
    mp4_muxer_config_t      mp4_config;
    mp4_writer_ctx_t        writer_ctx;
    live_photo_exif_info_t  exif_info;
    uint8_t                *cover;
    uint32_t                cover_len;
    uint32_t                write_count;
    bool                    write_err;
} live_photo_muxer_t;

/**
 * @brief  Wrapper handle used by esp_muxer registration table
 */
typedef struct {
    esp_muxer_type_t    type;
    esp_muxer_handle_t  child;
} esp_muxer_info_t;

/**
 * @brief  Close muxer instance and finalize JPEG cover rewrite
 */
static esp_muxer_err_t live_photo_close(esp_muxer_handle_t muxer);

static const char xmp_ns[] = "http://ns.adobe.com/xap/1.0/";

static int writer_seek(mp4_writer_ctx_t *w, uint32_t pos)
{
    return w->io.on_seek(w->ctx, w->mp4_offset + pos);
}

static mp4_writer_ctx_t *s_writer_ctx = NULL;

static void *live_photo_on_open(char *path)
{
    // Already opened
    return s_writer_ctx;
}

static int live_photo_on_write(void *writer, void *buffer, int len)
{
    mp4_writer_ctx_t *w = (mp4_writer_ctx_t *)writer;
    w->pos += len;
    if (w->pos > w->mp4_size) {
        w->mp4_size = w->pos;
    }
    return w->io.on_write(w->ctx, buffer, len);
}

static int live_photo_on_seek(void *writer, uint64_t pos)
{
    mp4_writer_ctx_t *w = (mp4_writer_ctx_t *)writer;
    w->pos = (uint32_t)pos;
    return writer_seek(w, (uint32_t)pos);
}

static int live_photo_on_close(void *writer)
{
    return 0;
}

static void set_default_exif_datetime(live_photo_exif_info_t *exif_info)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0 && tv.tv_sec > 0) {
        time_t now = (time_t)tv.tv_sec;
        struct tm tm_now;
        if (localtime_r(&now, &tm_now) != NULL) {
            snprintf(exif_info->date_time, sizeof(exif_info->date_time),
                     "%04d:%02d:%02d %02d:%02d:%02d",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
            return;
        }
    }
    snprintf(exif_info->date_time, sizeof(exif_info->date_time), "2026:01:01 00:00:00");
}

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v & 0xFF);
}

static uint16_t read_u16_endian(const uint8_t *p, bool little_endian)
{
    if (little_endian) {
        return (uint16_t)(p[0] | (p[1] << 8));
    }
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_u32_endian(const uint8_t *p, bool little_endian)
{
    if (little_endian) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static size_t strnlen_s(const char *s, size_t max_len)
{
    if (s == NULL) {
        return 0;
    }
    size_t n = 0;
    while (n < max_len && s[n] != '\0') {
        n++;
    }
    return n;
}

static uint32_t build_exif_payload(uint8_t *buf, uint32_t buf_size, const live_photo_exif_info_t *info)
{
    if (buf == NULL || info == NULL || buf_size < 64) {
        return 0;
    }

    bool has_make = info->make[0] != '\0';
    bool has_model = info->model[0] != '\0';
    bool has_dt = info->date_time[0] != '\0';
    bool has_exif_ver = info->has_exif_version;
    bool has_dto = info->date_time_original[0] != '\0';
    bool has_dtd = info->date_time_digitized[0] != '\0';
    bool has_px = info->pixel_x_dim > 0;
    bool has_py = info->pixel_y_dim > 0;
    bool has_sub_ifd = has_exif_ver || has_dto || has_dtd || has_px || has_py;
    uint16_t entry_count = (uint16_t)(1 + (has_make ? 1 : 0) + (has_model ? 1 : 0) + (has_dt ? 1 : 0)
                                      + (has_sub_ifd ? 1 : 0));
    uint16_t sub_entry_count = (uint16_t)((has_exif_ver ? 1 : 0) + (has_dto ? 1 : 0)
                                          + (has_dtd ? 1 : 0) + (has_px ? 1 : 0) + (has_py ? 1 : 0));

    uint32_t make_len = has_make ? (uint32_t)strnlen_s(info->make, sizeof(info->make) - 1) + 1 : 0;
    uint32_t model_len = has_model ? (uint32_t)strnlen_s(info->model, sizeof(info->model) - 1) + 1 : 0;
    uint32_t dt_len = has_dt ? (uint32_t)strnlen_s(info->date_time, sizeof(info->date_time) - 1) + 1 : 0;
    uint32_t dto_len = has_dto ? (uint32_t)strnlen_s(info->date_time_original, sizeof(info->date_time_original) - 1) + 1 : 0;
    uint32_t dtd_len = has_dtd ? (uint32_t)strnlen_s(info->date_time_digitized, sizeof(info->date_time_digitized) - 1) + 1 : 0;

    uint32_t tiff_ifd0_off = 8;
    uint32_t ifd0_size = 2 + (uint32_t)entry_count * 12 + 4;
    uint32_t extra_off = tiff_ifd0_off + ifd0_size;
    uint32_t ifd0_data_len = make_len + model_len + dt_len;
    uint32_t sub_ifd_off = extra_off + ifd0_data_len;
    uint32_t sub_ifd_size = has_sub_ifd ? (2 + (uint32_t)sub_entry_count * 12 + 4 + dto_len + dtd_len) : 0;
    uint32_t tiff_size = sub_ifd_off + sub_ifd_size;
    uint32_t payload_len = 6 + tiff_size;
    if (payload_len > buf_size) {
        return 0;
    }

    memset(buf, 0, payload_len);
    memcpy(buf, "Exif\0\0", 6);
    uint8_t *tiff = buf + 6;
    tiff[0] = 'M';
    tiff[1] = 'M';
    write_be16(tiff + 2, 0x002A);
    write_be32(tiff + 4, tiff_ifd0_off);

    uint8_t *ifd0 = tiff + tiff_ifd0_off;
    write_be16(ifd0, entry_count);
    uint32_t entry_idx = 0;
    uint32_t data_cursor = extra_off;

    // Orientation (required in this minimal EXIF block)
    uint8_t *e = ifd0 + 2 + entry_idx * 12;
    write_be16(e + 0, 0x0112);
    write_be16(e + 2, 3);
    write_be32(e + 4, 1);
    write_be16(e + 8, info->orientation ? info->orientation : 1);
    entry_idx++;

    if (has_make) {
        e = ifd0 + 2 + entry_idx * 12;
        write_be16(e + 0, 0x010F);
        write_be16(e + 2, 2);
        write_be32(e + 4, make_len);
        write_be32(e + 8, data_cursor);
        memcpy(tiff + data_cursor, info->make, make_len - 1);
        data_cursor += make_len;
        entry_idx++;
    }
    if (has_model) {
        e = ifd0 + 2 + entry_idx * 12;
        write_be16(e + 0, 0x0110);
        write_be16(e + 2, 2);
        write_be32(e + 4, model_len);
        write_be32(e + 8, data_cursor);
        memcpy(tiff + data_cursor, info->model, model_len - 1);
        data_cursor += model_len;
        entry_idx++;
    }
    if (has_dt) {
        e = ifd0 + 2 + entry_idx * 12;
        write_be16(e + 0, 0x0132);
        write_be16(e + 2, 2);
        write_be32(e + 4, dt_len);
        write_be32(e + 8, data_cursor);
        memcpy(tiff + data_cursor, info->date_time, dt_len - 1);
        data_cursor += dt_len;
        entry_idx++;
    }
    if (has_sub_ifd) {
        e = ifd0 + 2 + entry_idx * 12;
        write_be16(e + 0, 0x8769);
        write_be16(e + 2, 4);
        write_be32(e + 4, 1);
        write_be32(e + 8, sub_ifd_off);
        entry_idx++;
    }

    write_be32(ifd0 + 2 + entry_count * 12, 0);

    if (has_sub_ifd) {
        uint8_t *sub_ifd = tiff + sub_ifd_off;
        write_be16(sub_ifd, sub_entry_count);
        uint32_t sub_idx = 0;
        uint32_t sub_data_cursor = sub_ifd_off + 2 + (uint32_t)sub_entry_count * 12 + 4;
        if (has_exif_ver) {
            uint8_t *se = sub_ifd + 2 + sub_idx * 12;
            write_be16(se + 0, 0x9000);
            write_be16(se + 2, 7);
            write_be32(se + 4, 4);
            memcpy(se + 8, info->exif_version, 4);
            sub_idx++;
        }
        if (has_dto) {
            uint8_t *se = sub_ifd + 2 + sub_idx * 12;
            write_be16(se + 0, 0x9003);
            write_be16(se + 2, 2);
            write_be32(se + 4, dto_len);
            write_be32(se + 8, sub_data_cursor);
            memcpy(tiff + sub_data_cursor, info->date_time_original, dto_len - 1);
            sub_data_cursor += dto_len;
            sub_idx++;
        }
        if (has_dtd) {
            uint8_t *se = sub_ifd + 2 + sub_idx * 12;
            write_be16(se + 0, 0x9004);
            write_be16(se + 2, 2);
            write_be32(se + 4, dtd_len);
            write_be32(se + 8, sub_data_cursor);
            memcpy(tiff + sub_data_cursor, info->date_time_digitized, dtd_len - 1);
            sub_data_cursor += dtd_len;
            sub_idx++;
        }
        if (has_px) {
            uint8_t *se = sub_ifd + 2 + sub_idx * 12;
            write_be16(se + 0, 0xA002);
            write_be16(se + 2, 4);
            write_be32(se + 4, 1);
            write_be32(se + 8, info->pixel_x_dim);
            sub_idx++;
        }
        if (has_py) {
            uint8_t *se = sub_ifd + 2 + sub_idx * 12;
            write_be16(se + 0, 0xA003);
            write_be16(se + 2, 4);
            write_be32(se + 4, 1);
            write_be32(se + 8, info->pixel_y_dim);
            sub_idx++;
        }
        write_be32(sub_ifd + 2 + (uint32_t)sub_entry_count * 12, 0);
    }
    return payload_len;
}

static bool copy_exif_ascii_from_ifd(const uint8_t *tiff, uint32_t tiff_len, bool little_endian, const uint8_t *entry,
                                     char *dst, size_t dst_size)
{
    uint32_t count = read_u32_endian(entry + 4, little_endian);
    if (dst == NULL || dst_size == 0 || count == 0) {
        return false;
    }
    const uint8_t *src = NULL;
    uint32_t src_len = 0;
    if (count <= 4) {
        src = entry + 8;
        src_len = count;
    } else {
        uint32_t off = read_u32_endian(entry + 8, little_endian);
        if (off >= tiff_len || off + count > tiff_len) {
            return false;
        }
        src = tiff + off;
        src_len = count;
    }
    size_t n = src_len;
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
    return true;
}

static bool copy_exif_long_from_ifd(const uint8_t *tiff, uint32_t tiff_len, bool little_endian, const uint8_t *entry,
                                    uint32_t *value)
{
    uint16_t type = read_u16_endian(entry + 2, little_endian);
    uint32_t count = read_u32_endian(entry + 4, little_endian);
    if (value == NULL || type != 4 || count != 1) {
        return false;
    }
    (void)tiff;
    (void)tiff_len;
    *value = read_u32_endian(entry + 8, little_endian);
    return true;
}

static void fill_exif_info(live_photo_exif_info_t *exif_info, const uint8_t *tiff, uint32_t tiff_len,
                           const uint8_t *ent, bool little_endian)
{
    uint16_t tag = read_u16_endian(ent + 0, little_endian);
    uint16_t type = read_u16_endian(ent + 2, little_endian);
    uint32_t count = read_u32_endian(ent + 4, little_endian);
    if (tag == 0x0112 && type == 3 && count == 1) {
        exif_info->orientation = read_u16_endian(ent + 8, little_endian);
    } else if (tag == 0x010F && type == 2) {
        copy_exif_ascii_from_ifd(tiff, tiff_len, little_endian, ent, exif_info->make, sizeof(exif_info->make));
    } else if (tag == 0x0110 && type == 2) {
        copy_exif_ascii_from_ifd(tiff, tiff_len, little_endian, ent, exif_info->model, sizeof(exif_info->model));
    } else if (tag == 0x0132 && type == 2) {
        copy_exif_ascii_from_ifd(tiff, tiff_len, little_endian, ent, exif_info->date_time, sizeof(exif_info->date_time));
    }
    else if (tag == 0x9003 && type == 2) {
        copy_exif_ascii_from_ifd(tiff, tiff_len, little_endian, ent,
                                 exif_info->date_time_original, sizeof(exif_info->date_time_original));
    } else if (tag == 0x9004 && type == 2) {
        copy_exif_ascii_from_ifd(tiff, tiff_len, little_endian, ent,
                                 exif_info->date_time_digitized, sizeof(exif_info->date_time_digitized));
    } else if (tag == 0xA002 && type == 4 && count == 1) {
        copy_exif_long_from_ifd(tiff, tiff_len, little_endian, ent, &exif_info->pixel_x_dim);
    } else if (tag == 0xA003 && type == 4 && count == 1) {
        copy_exif_long_from_ifd(tiff, tiff_len, little_endian, ent, &exif_info->pixel_y_dim);
    }
}

static void try_extract_basic_exif_info(const uint8_t *jpeg, uint32_t jpeg_len, live_photo_exif_info_t *exif_info)
{
    if (jpeg == NULL || exif_info == NULL || jpeg_len < 4 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) {
        return;
    }
    uint32_t i = 2;
    while (i + 4 < jpeg_len) {
        if (jpeg[i] != 0xFF) {
            i++;
            continue;
        }
        while (i < jpeg_len && jpeg[i] == 0xFF) {
            i++;
        }
        if (i >= jpeg_len) {
            break;
        }
        uint8_t marker = jpeg[i++];
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01 || marker == 0xD8) {
            continue;
        }
        if (i + 2 > jpeg_len) {
            break;
        }
        uint16_t seg_len = (uint16_t)((jpeg[i] << 8) | jpeg[i + 1]);
        if (seg_len < 2 || i + seg_len > jpeg_len) {
            break;
        }
        if (marker == 0xC0 || marker == 0xC2) {
            if (seg_len >= 7 && i + 6 < jpeg_len) {
                uint16_t height = (uint16_t)((jpeg[i + 3] << 8) | jpeg[i + 4]);
                uint16_t width = (uint16_t)((jpeg[i + 5] << 8) | jpeg[i + 6]);
                if (exif_info->pixel_x_dim == 0 || exif_info->pixel_y_dim == 0) {
                    exif_info->pixel_x_dim = width;
                    exif_info->pixel_y_dim = height;
                    ESP_LOGI(TAG, "SOF(%02X): Width: %u, Height: %u", marker, width, height);
                }
            }
        }
        else if (marker == 0xE1 && seg_len >= 8 && memcmp(jpeg + i + 2, "Exif\0\0", 6) == 0) {
            const uint8_t *tiff = jpeg + i + 8;
            uint32_t tiff_len = (uint32_t)seg_len - 8;
            if (tiff_len < 8) {
                break;
            }
            bool little_endian = (tiff[0] == 'I' && tiff[1] == 'I');
            bool big_endian = (tiff[0] == 'M' && tiff[1] == 'M');
            if (!little_endian && !big_endian) {
                break;
            }
            uint16_t magic = read_u16_endian(tiff + 2, little_endian);
            if (magic != 0x2A) {
                break;
            }
            uint32_t ifd0_off = read_u32_endian(tiff + 4, little_endian);
            if (ifd0_off + 2 > tiff_len) {
                break;
            }
            uint16_t entry_count = read_u16_endian(tiff + ifd0_off, little_endian);
            uint32_t entries_off = ifd0_off + 2;
            for (uint32_t n = 0; n < entry_count; n++) {
                uint32_t ent_off = entries_off + n * 12;
                if (ent_off + 12 > tiff_len) {
                    break;
                }
                const uint8_t *ent = tiff + ent_off;
                uint16_t tag = read_u16_endian(ent + 0, little_endian);
                if (tag != 0x8769) {
                    fill_exif_info(exif_info, tiff, tiff_len, ent, little_endian);
                } else {
                    uint32_t sub_ifd_off = read_u32_endian(ent + 8, little_endian);
                    if (sub_ifd_off + 2 > tiff_len) {
                        continue;
                    }
                    uint16_t sub_entry_count = read_u16_endian(tiff + sub_ifd_off, little_endian);
                    uint32_t sub_entries_off = sub_ifd_off + 2;
                    for (uint32_t sn = 0; sn < sub_entry_count; sn++) {
                        uint32_t sub_ent_off = sub_entries_off + sn * 12;
                        if (sub_ent_off + 12 > tiff_len) {
                            break;
                        }
                        const uint8_t *sub_ent = tiff + sub_ent_off;
                        fill_exif_info(exif_info, tiff, tiff_len, sub_ent, little_endian);
                    }
                }
            }
            break;
        }
        i += seg_len;
    }
}

static uint32_t build_xmp(char *buf, uint32_t buf_size, uint32_t mp4_offset, uint32_t file_size)
{
    static const char *kTpl =
        R"(<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="Adobe XMP Core Test.SNAPSHOT">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
    <rdf:Description rdf:about=""
        xmlns:GCamera="http://ns.google.com/photos/1.0/camera/"
        xmlns:Container="http://ns.google.com/photos/1.0/container/"
        xmlns:Item="http://ns.google.com/photos/1.0/container/item/"
    GCamera:MotionPhoto="1"
    GCamera:MotionPhotoVersion="1"
    GCamera:MotionPhotoPresentationTimestampUs="%u">
    <Container:Directory>
        <rdf:Seq>
        <rdf:li rdf:parseType="Resource">
            <Container:Item
            Item:Mime="image/jpeg"
            Item:Semantic="Primary"
            Item:Padding="24"/>
        </rdf:li>
        <rdf:li rdf:parseType="Resource">
            <Container:Item
            Item:Mime="video/mp4"
            Item:Semantic="MotionPhoto"
            Item:Length="%u"
            Item:Padding="0"/>
        </rdf:li>
        </rdf:Seq>
    </Container:Directory>
    </rdf:Description>
</rdf:RDF>
</x:xmpmeta>)";
    (void)mp4_offset;
    int n = snprintf(buf, buf_size, kTpl, 2000000, file_size);
    if (n <= 0 || n >= (int)buf_size) {
        return 0;
    }
    return (uint32_t)n;
}

static int find_jpeg_eoi(const uint8_t *jpeg, uint32_t len)
{
    if (len < 2) {
        return -1;
    }
    for (int i = (int)len - 2; i >= 0; i--) {
        if (jpeg[i] == 0xFF && jpeg[i + 1] == 0xD9) {
            return i + 2;
        }
    }
    return -1;
}

static esp_muxer_err_t write_jpeg_without_old_app1(live_photo_muxer_t *m, uint32_t eoi)
{
    const uint8_t *buf = m->cover;
    uint32_t i = 2;

    while (i + 1 < eoi) {
        if (buf[i] != 0xFF) {
            i++;
            continue;
        }
        while (i < eoi && buf[i] == 0xFF) {
            i++;
        }
        if (i >= eoi) {
            break;
        }
        uint8_t marker = buf[i++];
        if (marker == 0xD9) {
            break;
        }
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01 || marker == 0xD8) {
            continue;
        }
        if (i + 2 > eoi) {
            return ESP_MUXER_ERR_FAIL;
        }
        uint16_t seg_len = (buf[i] << 8) | buf[i + 1];
        if (seg_len < 2 || i + seg_len > eoi) {
            return ESP_MUXER_ERR_FAIL;
        }

        uint32_t seg_start = i - 2;
        uint32_t seg_total = (uint32_t)seg_len + 2;

        if (marker == 0xE1) {
            // Drop all old APP1 blocks (EXIF/XMP) and rebuild them from API input.
            i += seg_len;
            continue;
        }

        if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, (void *)(buf + seg_start), seg_total) < 0) {
            return ESP_MUXER_ERR_FAIL;
        }

        if (marker == 0xDA) {
            uint32_t data_start = i + seg_len;
            if (data_start > eoi) {
                return ESP_MUXER_ERR_FAIL;
            }
            if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, (void *)(buf + data_start), eoi - data_start) < 0) {
                return ESP_MUXER_ERR_FAIL;
            }
            return ESP_MUXER_ERR_OK;
        }
        i += seg_len;
    }
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t write_cover_with_xmp(live_photo_muxer_t *m)
{
    if (m->cover == NULL || m->cover_len < 4) {
        return ESP_MUXER_ERR_FAIL;
    }
    int eoi = find_jpeg_eoi(m->cover, m->cover_len);
    if (eoi < 2 || m->cover[0] != 0xFF || m->cover[1] != 0xD8) {
        ESP_LOGE(TAG, "Wrong JPEG image");
        return ESP_MUXER_ERR_FAIL;
    }
    char xmp[1536];
    uint32_t xmp_len = build_xmp(xmp, sizeof(xmp), m->writer_ctx.mp4_offset, m->writer_ctx.mp4_size);
    if (xmp_len == 0) {
        ESP_LOGE(TAG, "Build XMP failed");
        return ESP_MUXER_ERR_FAIL;
    }
    if (m->writer_ctx.io.on_seek(m->writer_ctx.ctx, 0) != 0) {
        return ESP_MUXER_ERR_FAIL;
    }
    if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, m->cover, 2) != 2) {
        return ESP_MUXER_ERR_FAIL;
    }

    char out[2048];
    int w = 0;
    out[w++] = 0xFF;
    out[w++] = 0xE1;
    char *app_len_ptr = out + w;
    w += 2;
    uint32_t exif_len = build_exif_payload((uint8_t *)out + w, sizeof(out) - (uint32_t)w, &m->exif_info);
    if (exif_len == 0) {
        ESP_LOGE(TAG, "Build EXIF failed");
        return ESP_MUXER_ERR_FAIL;
    }
    uint16_t app1_exif_len = (uint16_t)(exif_len + 2);
    app_len_ptr[0] = (uint8_t)(app1_exif_len >> 8);
    app_len_ptr[1] = (uint8_t)(app1_exif_len & 0xFF);
    w += exif_len;
    if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, out, w) < 0) {
        return ESP_MUXER_ERR_WRITE_DATA;
    }

    w = 4;
    memcpy(out + w, xmp_ns, sizeof(xmp_ns));
    w += sizeof(xmp_ns);

    memcpy(out + w, xmp, xmp_len);
    uint16_t app_len = xmp_len + sizeof(xmp_ns) + 2;
    app_len_ptr[0] = (uint8_t)(app_len >> 8);
    app_len_ptr[1] = (uint8_t)(app_len & 0xFF);
    w += xmp_len;
    if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, out, w) != w) {
        return ESP_MUXER_ERR_WRITE_DATA;
    }
    return write_jpeg_without_old_app1(m, (uint32_t)eoi);
}

static void *default_open(char *path)
{
    FILE *fp = fopen(path, "wb");
    return fp;
}

static int default_write(void *writer, void *buffer, int len)
{
    FILE *fp = (FILE *)writer;
    if (fp == NULL) {
        return -1;
    }
    return fwrite(buffer, 1, len, fp);
}

static int default_seek(void *writer, uint64_t pos)
{
    FILE *fp = (FILE *)writer;
    if (fp == NULL) {
        return -1;
    }
    return fseek(fp, pos, SEEK_SET);
}

static int default_close(void *writer)
{
    FILE *fp = (FILE *)writer;
    if (fp == NULL) {
        return -1;
    }
    return fclose(fp);
}

static void live_photo_get_default_writer(esp_muxer_file_writer_t *writer)
{
    writer->on_open = default_open;
    writer->on_write = default_write;
    writer->on_seek = default_seek;
    writer->on_close = default_close;
}

static int live_photo_check_file(live_photo_muxer_t *m)
{
    if (m->write_err) {
        return ESP_MUXER_ERR_WRITE_DATA;
    }
    if (m->write_count) {
        m->write_count++;
        return ESP_MUXER_ERR_OK;
    }
    char buffer[256] = {0};
    if (m->mp4_config.base_config.url_pattern_ex) {
        esp_muxer_slice_info_t slice_info = {
            .file_path = buffer,
            .len = sizeof(buffer),
            .slice_index = 0,
        };
        m->mp4_config.base_config.url_pattern_ex(&slice_info, m->mp4_config.base_config.ctx);
    } else if (m->mp4_config.base_config.url_pattern) {
        m->mp4_config.base_config.url_pattern(buffer, sizeof(buffer), 0);
    }
    // Workaround for open callback
    s_writer_ctx = &m->writer_ctx;
    m->writer_ctx.ctx = m->writer_ctx.io.on_open(buffer);
    if (m->writer_ctx.ctx == NULL) {
        m->write_err = true;
        ESP_LOGE(TAG, "Fail to open file for live photo");
        return ESP_MUXER_ERR_WRITE_DATA;
    }
    if (m->writer_ctx.io.on_seek(m->writer_ctx.ctx, 0) != 0) {
        m->write_err = true;
        ESP_LOGE(TAG, "Fail to seek file for live photo");
        return ESP_MUXER_ERR_WRITE_DATA;
    }
    // Reserve space for JPEG image
    uint32_t left = m->writer_ctx.mp4_offset;
    while (left) {
        uint32_t n = left > sizeof(buffer) ? sizeof(buffer) : left;
        if (m->writer_ctx.io.on_write(m->writer_ctx.ctx, buffer, n) != (int)n) {
            m->write_err = true;
            ESP_LOGE(TAG, "Fail to write data to file for live photo");
            return ESP_MUXER_ERR_WRITE_DATA;
        }
        left -= n;
    }
    m->write_count++;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_handle_t live_photo_open(esp_muxer_config_t *cfg, uint32_t size)
{
    if (cfg == NULL || size < sizeof(esp_live_photo_muxer_config_t)) {
        ESP_LOGE(TAG, "Invalid argument cfg:%p size:%d", cfg, size);
        return NULL;
    }
    esp_live_photo_muxer_config_t *lp_cfg = (esp_live_photo_muxer_config_t *)cfg;
    live_photo_muxer_t *m = calloc(1, sizeof(live_photo_muxer_t));
    if (m == NULL) {
        ESP_LOGE(TAG, "No memory for live photo muxer");
        return NULL;
    }
    live_photo_get_default_writer(&m->writer_ctx.io);

    m->mp4_config = lp_cfg->mp4_config;
    m->writer_ctx.mp4_offset = lp_cfg->reserve_cover_size;
    snprintf(m->exif_info.make, sizeof(m->exif_info.make), "Espressif");
    snprintf(m->exif_info.model, sizeof(m->exif_info.model), "ESP32P4");
    m->exif_info.orientation = 1;
    set_default_exif_datetime(&m->exif_info);

    do {
        m->mp4_config.base_config.muxer_type = ESP_MUXER_TYPE_MP4;
        m->mp4_muxer = esp_muxer_open(&m->mp4_config.base_config, sizeof(mp4_muxer_config_t));
        if (m->mp4_muxer == NULL) {
            ESP_LOGE(TAG, "Fail to open MP4 muxer");
            break;
        }
        esp_muxer_file_writer_t writer = {
            .on_open = live_photo_on_open,
            .on_write = live_photo_on_write,
            .on_seek = live_photo_on_seek,
            .on_close = live_photo_on_close,
        };
        esp_muxer_set_file_writer(m->mp4_muxer, &writer);
        return m;
    } while (0);

    live_photo_close(m);
    return NULL;
}

static esp_muxer_err_t live_photo_add_video_stream(esp_muxer_handle_t muxer, esp_muxer_video_stream_info_t *video_info,
                                                   int *stream_index)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    return esp_muxer_add_video_stream(m->mp4_muxer, video_info, stream_index);
}

static esp_muxer_err_t live_photo_add_audio_stream(esp_muxer_handle_t muxer, esp_muxer_audio_stream_info_t *audio_info,
                                                   int *stream_index)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    return esp_muxer_add_audio_stream(m->mp4_muxer, audio_info, stream_index);
}

static esp_muxer_err_t live_photo_set_writer(esp_muxer_handle_t muxer, esp_muxer_file_writer_t *writer)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    if (m == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Invalid argument muxer:%p, writer:%p", muxer, writer);
        return ESP_MUXER_ERR_INVALID_ARG;
    }
    if (m->writer_ctx.ctx) {
        ESP_LOGE(TAG, "Writer already under usage");
        return ESP_MUXER_ERR_NOT_SUPPORT;
    }
    m->writer_ctx.io = *writer;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t live_photo_add_video_packet(esp_muxer_handle_t muxer, int stream_index,
                                                   esp_muxer_video_packet_t *packet)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    if (live_photo_check_file(m) != ESP_MUXER_ERR_OK) {
        ESP_LOGE(TAG, "Fail to check file for video packet");
        return ESP_MUXER_ERR_FAIL;
    }
    return esp_muxer_add_video_packet(m->mp4_muxer, stream_index, packet);
}

static esp_muxer_err_t live_photo_add_audio_packet(esp_muxer_handle_t muxer, int stream_index,
                                                   esp_muxer_audio_packet_t *packet)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    if (live_photo_check_file(m) != ESP_MUXER_ERR_OK) {
        ESP_LOGE(TAG, "Fail to check file for audio packet");
        return ESP_MUXER_ERR_FAIL;
    }
    return esp_muxer_add_audio_packet(m->mp4_muxer, stream_index, packet);
}

esp_muxer_err_t esp_live_photo_muxer_set_cover_jpeg(esp_muxer_handle_t muxer, esp_live_photo_muxer_cover_info_t *cover_info)
{
    esp_muxer_info_t *info = (esp_muxer_info_t *)muxer;
    if (muxer == NULL || info->child == NULL) {
        ESP_LOGE(TAG, "Invalid argument muxer:%p", muxer);
        return ESP_MUXER_ERR_INVALID_ARG;
    }
    if (cover_info == NULL || cover_info->jpeg == NULL || cover_info->jpeg_len == 0) {
        ESP_LOGE(TAG, "Invalid cover info:%p jpeg:%p jpeg_len:%d", cover_info, cover_info->jpeg,
                 (int)cover_info->jpeg_len);
        return ESP_MUXER_ERR_INVALID_ARG;
    }
    live_photo_muxer_t *m = (live_photo_muxer_t *)info->child;

    if (cover_info->jpeg_len + 256 > m->writer_ctx.mp4_offset) {
        ESP_LOGE(TAG, "JPEG image size over limited %d", (int)m->writer_ctx.mp4_offset);
        return ESP_MUXER_ERR_NO_MEM;
    }
    uint8_t *copy = malloc(cover_info->jpeg_len);
    if (copy == NULL) {
        return ESP_MUXER_ERR_NO_MEM;
    }
    memcpy(copy, cover_info->jpeg, cover_info->jpeg_len);
    try_extract_basic_exif_info(cover_info->jpeg, cover_info->jpeg_len, &m->exif_info);
    if (m->exif_info.orientation == 0) {
        m->exif_info.orientation = 1;
    }
    if (m->exif_info.date_time[0] == '\0') {
        set_default_exif_datetime(&m->exif_info);
    }
    if (m->cover) {
        free(m->cover);
    }
    m->cover = copy;
    m->cover_len = cover_info->jpeg_len;
    if (live_photo_check_file(m) != ESP_MUXER_ERR_OK) {
        ESP_LOGE(TAG, "Fail to check file for cover JPEG");
        return ESP_MUXER_ERR_FAIL;
    }
    return ESP_MUXER_ERR_OK;
}

esp_muxer_err_t esp_live_photo_muxer_set_exif_info(esp_muxer_handle_t muxer, const esp_live_photo_exif_info_t *exif_info)
{
    esp_muxer_info_t *info = (esp_muxer_info_t *)muxer;
    if (muxer == NULL || info->child == NULL || exif_info == NULL) {
        ESP_LOGE(TAG, "Invalid argument muxer:%p, exif_info:%p", muxer, exif_info);
        return ESP_MUXER_ERR_INVALID_ARG;
    }
    live_photo_muxer_t *m = (live_photo_muxer_t *)info->child;

    memset(&m->exif_info, 0, sizeof(m->exif_info));
    m->exif_info.orientation = exif_info->orientation ? exif_info->orientation : 1;
    if (exif_info->make) {
        snprintf(m->exif_info.make, sizeof(m->exif_info.make), "%s", exif_info->make);
    }
    if (exif_info->model) {
        snprintf(m->exif_info.model, sizeof(m->exif_info.model), "%s", exif_info->model);
    }
    if (exif_info->date_time) {
        snprintf(m->exif_info.date_time, sizeof(m->exif_info.date_time), "%s", exif_info->date_time);
    }
    if (m->exif_info.date_time[0] == '\0') {
        set_default_exif_datetime(&m->exif_info);
    }
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t live_photo_close(esp_muxer_handle_t muxer)
{
    live_photo_muxer_t *m = (live_photo_muxer_t *)muxer;
    esp_muxer_err_t ret = ESP_MUXER_ERR_NOT_FOUND;
    do {
        if (m->mp4_muxer == NULL) {
            break;
        }
        ret = esp_muxer_close(m->mp4_muxer);
        if (ret != ESP_MUXER_ERR_OK) {
            ESP_LOGE(TAG, "Fail to close MP4 muxer");
            break;
        }
        if (m->cover && m->cover_len) {
            ret = write_cover_with_xmp(m);
        }
        if (m->writer_ctx.ctx) {
            m->writer_ctx.io.on_close(m->writer_ctx.ctx);
            m->writer_ctx.ctx = NULL;
        }
    } while (0);
    if (m->cover) {
        free(m->cover);
        m->cover = NULL;
    }
    free(m);
    return ret;
}

esp_muxer_err_t esp_live_photo_muxer_register(void)
{
    static esp_muxer_reg_info_t s_reg = {
        .open = live_photo_open,
        .add_video_stream = live_photo_add_video_stream,
        .add_audio_stream = live_photo_add_audio_stream,
        .set_writer = live_photo_set_writer,
        .add_video_packet = live_photo_add_video_packet,
        .add_audio_packet = live_photo_add_audio_packet,
        .close = live_photo_close,
    };
    return esp_muxer_reg(ESP_MUXER_TYPE_LIVE_PHOTO, &s_reg);
}
