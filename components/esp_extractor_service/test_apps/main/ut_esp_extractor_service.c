/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "esp_extractor_service.h"
#include "esp_extractor_service_ops.h"
#include "esp_media_dummy_service.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_vfs.h"

/** Keep ut object linked when running tests by tag from app_main. */
void esp_extractor_service_ut_force_link(void)  { }

#define WAV_DATA_BYTES  (16000 * 2 / 10)  /*!< 100 ms of mono S16 @ 16 kHz */
#define WAV_SIZE        (44 + WAV_DATA_BYTES)
#define TEST_RUN_MS     500

#define MEMFS_MOUNT   "/utfs"
#define MEMFS_FILE    "test.wav"
#define MEMFS_PATH    MEMFS_MOUNT "/" MEMFS_FILE
#define MEMFS_MAX_FD  4

typedef struct {
    bool    used;
    size_t  pos;
} memfs_fd_t;

static const uint8_t *s_memfs_data;
static size_t s_memfs_size;
static char s_memfs_mount[32];
static memfs_fd_t s_memfs_fds[MEMFS_MAX_FD];
static bool s_memfs_mounted;

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void make_wav(uint8_t *wav, size_t wav_size, size_t data_bytes)
{
    TEST_ASSERT_TRUE(wav_size >= 44 + data_bytes);
    memset(wav, 0, wav_size);
    memcpy(wav + 0, "RIFF", 4);
    put_le32(wav + 4, (uint32_t)(wav_size - 8));
    memcpy(wav + 8, "WAVEfmt ", 8);
    put_le32(wav + 16, 16);
    put_le16(wav + 20, 1);  /* PCM */
    put_le16(wav + 22, 1);  /* mono */
    put_le32(wav + 24, 16000);
    put_le32(wav + 28, 16000 * 2);
    put_le16(wav + 32, 2);
    put_le16(wav + 34, 16);
    memcpy(wav + 36, "data", 4);
    put_le32(wav + 40, (uint32_t)data_bytes);
    for (size_t i = 0; i < data_bytes; i++) {
        wav[44 + i] = (uint8_t)(i * 3);
    }
}

static const char *memfs_basename(const char *path)
{
    if (path == NULL) {
        return "";
    }
    while (*path == '/') {
        path++;
    }
    return path;
}

static bool memfs_path_match(const char *path)
{
    return strcmp(memfs_basename(path), MEMFS_FILE) == 0;
}

static int memfs_open(const char *path, int flags, int mode)
{
    (void)mode;
    if (!memfs_path_match(path) || s_memfs_data == NULL) {
        errno = ENOENT;
        return -1;
    }
    if ((flags & O_ACCMODE) != O_RDONLY) {
        errno = EACCES;
        return -1;
    }
    for (int i = 0; i < MEMFS_MAX_FD; i++) {
        if (!s_memfs_fds[i].used) {
            s_memfs_fds[i].used = true;
            s_memfs_fds[i].pos = 0;
            return i;
        }
    }
    errno = EMFILE;
    return -1;
}

static ssize_t memfs_read(int fd, void *dst, size_t size)
{
    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used || dst == NULL) {
        errno = EBADF;
        return -1;
    }
    memfs_fd_t *f = &s_memfs_fds[fd];
    if (f->pos >= s_memfs_size) {
        return 0;
    }
    size_t remain = s_memfs_size - f->pos;
    size_t n = size < remain ? size : remain;
    memcpy(dst, s_memfs_data + f->pos, n);
    f->pos += n;
    return (ssize_t)n;
}

static ssize_t memfs_write(int fd, const void *data, size_t size)
{
    (void)fd;
    (void)data;
    (void)size;
    errno = EROFS;
    return -1;
}

static off_t memfs_lseek(int fd, off_t offset, int mode)
{
    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    memfs_fd_t *f = &s_memfs_fds[fd];
    off_t new_pos = (off_t)f->pos;
    if (mode == SEEK_SET) {
        new_pos = offset;
    } else if (mode == SEEK_CUR) {
        new_pos += offset;
    } else if (mode == SEEK_END) {
        new_pos = (off_t)s_memfs_size + offset;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (new_pos < 0 || (size_t)new_pos > s_memfs_size) {
        errno = EINVAL;
        return -1;
    }
    f->pos = (size_t)new_pos;
    return new_pos;
}

static int memfs_close(int fd)
{
    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    s_memfs_fds[fd].used = false;
    s_memfs_fds[fd].pos = 0;
    return 0;
}

static void memfs_fill_stat(struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | 0444;
    st->st_size = (off_t)s_memfs_size;
    st->st_nlink = 1;
}

static int memfs_fstat(int fd, struct stat *st)
{
    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used || st == NULL) {
        errno = EBADF;
        return -1;
    }
    memfs_fill_stat(st);
    return 0;
}

static int memfs_stat(const char *path, struct stat *st)
{
    if (st == NULL || !memfs_path_match(path) || s_memfs_data == NULL) {
        errno = ENOENT;
        return -1;
    }
    memfs_fill_stat(st);
    return 0;
}

static esp_err_t memfs_mount(const uint8_t *data, size_t size)
{
    if (data == NULL || size == 0 || s_memfs_mounted) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(s_memfs_fds, 0, sizeof(s_memfs_fds));
    s_memfs_data = data;
    s_memfs_size = size;
    strlcpy(s_memfs_mount, MEMFS_MOUNT, sizeof(s_memfs_mount));

    static const esp_vfs_t vfs = {
        .flags = ESP_VFS_FLAG_DEFAULT,
        .open = memfs_open,
        .read = memfs_read,
        .write = memfs_write,
        .lseek = memfs_lseek,
        .close = memfs_close,
        .fstat = memfs_fstat,
        .stat = memfs_stat,
    };
    esp_err_t ret = esp_vfs_register(s_memfs_mount, &vfs, NULL);
    if (ret != ESP_OK) {
        s_memfs_data = NULL;
        s_memfs_size = 0;
        return ret;
    }
    s_memfs_mounted = true;
    return ESP_OK;
}

static void memfs_unmount(void)
{
    if (!s_memfs_mounted) {
        return;
    }
    (void)esp_vfs_unregister(s_memfs_mount);
    memset(s_memfs_fds, 0, sizeof(s_memfs_fds));
    s_memfs_data = NULL;
    s_memfs_size = 0;
    s_memfs_mounted = false;
}

static void run_extractor_to_dummy_sink(esp_extractor_service_t *src)
{
    esp_media_dummy_service_t *sink = NULL;
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    sink_cfg.name = "extractor_dummy_sink";
    TEST_ESP_OK(esp_media_dummy_service_create(&sink_cfg, &sink));

    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    vTaskDelay(pdMS_TO_TICKS(TEST_RUN_MS));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));

    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, ESP_MEDIA_DEFAULT_STREAM, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ASSERT_GREATER_THAN(0, stats.audio_byte_count);
    TEST_ASSERT_EQUAL(0, stats.video_frame_count);

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}

TEST_CASE("extractor service acquires frames from memory wav", "[esp_extractor_service]")
{
    uint8_t *wav = calloc(1, WAV_SIZE);
    TEST_ASSERT_NOT_NULL(wav);
    make_wav(wav, WAV_SIZE, WAV_DATA_BYTES);

    esp_extractor_service_cfg_t cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    esp_extractor_service_t *svc = NULL;
    TEST_ESP_OK(esp_extractor_service_create(&cfg, &svc));
    TEST_ESP_OK(esp_extractor_service_set_extract_mask(svc, ESP_EXTRACT_MASK_AUDIO));
    TEST_ESP_OK(esp_extractor_service_set_src_data(svc, wav, WAV_SIZE));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(svc)));

    esp_media_provider_t provider = {0};
    TEST_ESP_OK(esp_media_service_get_provider(ESP_SERVICE_BASE(svc), ESP_MEDIA_DEFAULT_STREAM, &provider));
    esp_media_frame_t frame = {0};
    TEST_ESP_OK(esp_media_provider_acquire_frame(&provider, &frame, 2000));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, frame.type);
    TEST_ASSERT_GREATER_THAN(0, frame.size);
    TEST_ESP_OK(esp_media_provider_release_frame(&provider, &frame));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(svc)));
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(svc)));
    free(svc);
    free(wav);
}

TEST_CASE("extractor service links memory wav to dummy sink", "[esp_extractor_service]")
{
    uint8_t *wav = calloc(1, WAV_SIZE);
    TEST_ASSERT_NOT_NULL(wav);
    make_wav(wav, WAV_SIZE, WAV_DATA_BYTES);

    esp_extractor_service_t *src = NULL;
    esp_extractor_service_cfg_t src_cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    TEST_ESP_OK(esp_extractor_service_create(&src_cfg, &src));
    TEST_ESP_OK(esp_extractor_service_set_extract_mask(src, ESP_EXTRACT_MASK_AUDIO));
    TEST_ESP_OK(esp_extractor_service_set_src_data(src, wav, WAV_SIZE));

    run_extractor_to_dummy_sink(src);

    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(src)));
    free(src);
    free(wav);
}

TEST_CASE("extractor service links vfs wav file to dummy sink", "[esp_extractor_service]")
{
    uint8_t *wav = calloc(1, WAV_SIZE);
    TEST_ASSERT_NOT_NULL(wav);
    make_wav(wav, WAV_SIZE, WAV_DATA_BYTES);
    TEST_ESP_OK(memfs_mount(wav, WAV_SIZE));

    /* Sanity: GMF file IO uses fopen/stat against this VFS path. */
    struct stat st = {0};
    TEST_ASSERT_EQUAL(0, stat(MEMFS_PATH, &st));
    TEST_ASSERT_EQUAL(WAV_SIZE, st.st_size);
    FILE *fp = fopen(MEMFS_PATH, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    uint8_t hdr[4] = {0};
    TEST_ASSERT_EQUAL(4, fread(hdr, 1, 4, fp));
    TEST_ASSERT_EQUAL_STRING_LEN("RIFF", (const char *)hdr, 4);
    fclose(fp);

    esp_extractor_service_t *src = NULL;
    esp_extractor_service_cfg_t src_cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    TEST_ESP_OK(esp_extractor_service_create(&src_cfg, &src));
    TEST_ESP_OK(esp_extractor_service_set_extract_mask(src, ESP_EXTRACT_MASK_AUDIO));
    TEST_ESP_OK(esp_extractor_service_set_url(src, MEMFS_PATH));

    run_extractor_to_dummy_sink(src);

    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(src)));
    free(src);
    memfs_unmount();
    free(wav);
}
