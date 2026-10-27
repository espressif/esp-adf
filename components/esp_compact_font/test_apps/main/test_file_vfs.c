/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_vfs.h"
#include "test_helpers.h"

#define MEMFS_MOUNT   "/cfmem"
#define MEMFS_PATH    "/cfmem/ascii.gfh"
#define MEMFS_MAX_FD  4

typedef struct {
    bool    used;
    size_t  pos;
} memfs_fd_t;

static const uint8_t *s_memfs_data;
static size_t s_memfs_size;
static bool s_memfs_mounted;
static memfs_fd_t s_memfs_fds[MEMFS_MAX_FD];

static bool memfs_path_match(const char *path)
{
    return path && (strcmp(path, MEMFS_PATH) == 0 ||
                    strcmp(path, "ascii.gfh") == 0 ||
                    strcmp(path, "/ascii.gfh") == 0);
}

static int memfs_open(const char *path, int flags, int mode)
{
    int fd;

    (void)flags;
    (void)mode;
    if (!memfs_path_match(path) || s_memfs_data == NULL) {
        errno = ENOENT;
        return -1;
    }
    for (fd = 0; fd < MEMFS_MAX_FD; ++fd) {
        if (!s_memfs_fds[fd].used) {
            s_memfs_fds[fd].used = true;
            s_memfs_fds[fd].pos = 0;
            return fd;
        }
    }
    errno = EMFILE;
    return -1;
}

static ssize_t memfs_read(int fd, void *dst, size_t size)
{
    size_t remain;
    size_t n;

    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used || dst == NULL) {
        errno = EBADF;
        return -1;
    }
    if (s_memfs_fds[fd].pos >= s_memfs_size) {
        return 0;
    }
    remain = s_memfs_size - s_memfs_fds[fd].pos;
    n = size < remain ? size : remain;
    memcpy(dst, s_memfs_data + s_memfs_fds[fd].pos, n);
    s_memfs_fds[fd].pos += n;
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

static off_t memfs_lseek(int fd, off_t offset, int whence)
{
    size_t pos;

    if (fd < 0 || fd >= MEMFS_MAX_FD || !s_memfs_fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    if (whence == SEEK_SET) {
        pos = (size_t)offset;
    } else if (whence == SEEK_CUR) {
        pos = s_memfs_fds[fd].pos + (size_t)offset;
    } else if (whence == SEEK_END) {
        pos = s_memfs_size + (size_t)offset;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (pos > s_memfs_size) {
        errno = EINVAL;
        return -1;
    }
    s_memfs_fds[fd].pos = pos;
    return (off_t)pos;
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

    if (data == NULL || size == 0 || s_memfs_mounted) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(s_memfs_fds, 0, sizeof(s_memfs_fds));
    s_memfs_data = data;
    s_memfs_size = size;
    if (esp_vfs_register(MEMFS_MOUNT, &vfs, NULL) != ESP_OK) {
        s_memfs_data = NULL;
        s_memfs_size = 0;
        return ESP_FAIL;
    }
    s_memfs_mounted = true;
    return ESP_OK;
}

static void memfs_unmount(void)
{
    if (!s_memfs_mounted) {
        return;
    }
    (void)esp_vfs_unregister(MEMFS_MOUNT);
    memset(s_memfs_fds, 0, sizeof(s_memfs_fds));
    s_memfs_data = NULL;
    s_memfs_size = 0;
    s_memfs_mounted = false;
}

static void compare_file_vs_mem(bool prefer_ram)
{
    esp_compact_font_handle_t mem_font;
    esp_compact_font_handle_t file_font;
    static uint8_t mem_dst[ESP_COMPACT_FONT_GLYPH_MAX];
    static uint8_t file_dst[ESP_COMPACT_FONT_GLYPH_MAX];
    esp_compact_font_glyph_t mem_g;
    esp_compact_font_glyph_t file_g;
    size_t need;

    /* First open() keeps the VFS fd-table mutex, about 88 bytes. */
    test_heap_tolerance(100);
    TEST_ASSERT_EQUAL(ESP_OK, memfs_mount(test_font_mem(), test_font_len()));

    mem_font = test_open_font(prefer_ram);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_set_mem_src(mem_font, test_font_mem(), test_font_len()));

    file_font = test_open_font(prefer_ram);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_set_file_src(file_font, MEMFS_PATH));

    TEST_ASSERT_EQUAL_STRING(esp_compact_font_name(mem_font), esp_compact_font_name(file_font));
    TEST_ASSERT_EQUAL(esp_compact_font_glyph_count(mem_font),
                      esp_compact_font_glyph_count(file_font));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode(mem_font, 'A', mem_dst, sizeof(mem_dst), &mem_g));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode(file_font, 'A', file_dst, sizeof(file_dst), &file_g));
    TEST_ASSERT_EQUAL(mem_g.w, file_g.w);
    TEST_ASSERT_EQUAL(mem_g.h, file_g.h);
    TEST_ASSERT_EQUAL(mem_g.left, file_g.left);
    TEST_ASSERT_EQUAL(mem_g.top, file_g.top);
    TEST_ASSERT_EQUAL(mem_g.advance, file_g.advance);
    need = (size_t)mem_g.w * (size_t)mem_g.h;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(mem_dst, file_dst, need);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(mem_font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(file_font));
    memfs_unmount();
}

TEST_CASE("set_file_src via vfs matches set_mem_src", "[esp_compact_font]")
{
    compare_file_vs_mem(false);
}

TEST_CASE("set_file_src prefer_ram via vfs matches set_mem_src", "[esp_compact_font]")
{
    compare_file_vs_mem(true);
}
