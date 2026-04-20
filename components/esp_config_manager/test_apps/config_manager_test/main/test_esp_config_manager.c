/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "unity.h"

#include "esp_config_manager.h"
#include "esp_config_storage.h"

#define MOCK_CAP  512

typedef struct {
    uint8_t  blob[2][MOCK_CAP];
    size_t   len[2];
    bool     fail_write_primary;
    bool     fail_write_backup;
    bool     fail_read_primary;
    bool     fail_read_backup;
} mock_ram_ctx_t;

static void mock_ram_reset(mock_ram_ctx_t *m)
{
    memset(m, 0, sizeof(*m));
}

static esp_err_t mock_ram_read(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    mock_ram_ctx_t *m = (mock_ram_ctx_t *)ctx;
    int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    if ((idx == 0 && m->fail_read_primary) || (idx == 1 && m->fail_read_backup)) {
        return ESP_FAIL;
    }
    if (m->len[idx] == 0) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (*inout_len < m->len[idx]) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(buf, m->blob[idx], m->len[idx]);
    *inout_len = m->len[idx];
    return ESP_OK;
}

static esp_err_t mock_ram_write(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    mock_ram_ctx_t *m = (mock_ram_ctx_t *)ctx;
    int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    if ((idx == 0 && m->fail_write_primary) || (idx == 1 && m->fail_write_backup)) {
        return ESP_FAIL;
    }
    TEST_ASSERT_LESS_OR_EQUAL(MOCK_CAP, len);
    memcpy(m->blob[idx], buf, len);
    m->len[idx] = len;
    return ESP_OK;
}

static esp_err_t mock_ram_erase(void *ctx, esp_config_slot_t slot)
{
    mock_ram_ctx_t *m = (mock_ram_ctx_t *)ctx;
    int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    m->len[idx] = 0;
    return ESP_OK;
}

static const esp_config_storage_ops_t s_mock_ram_ops = {
    .read  = mock_ram_read,
    .write = mock_ram_write,
    .erase = mock_ram_erase,
};

static esp_err_t test_ram_storage_open(mock_ram_ctx_t *ram, esp_config_storage_t *out)
{
    return esp_config_storage_init(&s_mock_ram_ops, (void *)ram, out);
}

typedef struct __attribute__((packed)) {
    uint32_t  u32;
    uint16_t  u16;
    uint8_t   pad[10];
} test_pod_t;

static const test_pod_t s_test_defaults = {.u32 = 0x11223344, .u16 = 0xAABB, .pad = {0x11}};

typedef struct {
    uint8_t  key;
} xor_crypto_ctx_t;

static esp_err_t xor_encrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size,
                             size_t *out_len)
{
    xor_crypto_ctx_t *x = (xor_crypto_ctx_t *)ctx;
    if (out_size < in_len) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < in_len; i++) {
        out[i] = (uint8_t)(in[i] ^ x->key);
    }
    *out_len = in_len;
    return ESP_OK;
}

static esp_err_t xor_decrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size,
                             size_t *out_len)
{
    return xor_encrypt(ctx, in, in_len, out, out_size, out_len);
}

static esp_err_t merge_always_fail(void *user_ctx, const void *loaded, size_t loaded_len, const void *defaults,
                                   size_t default_size, void *out)
{
    (void)user_ctx;
    (void)loaded;
    (void)loaded_len;
    (void)defaults;
    (void)default_size;
    (void)out;
    return ESP_ERR_INVALID_STATE;
}

/** Succeeds for fresh defaults merge (no stored payload); fails once a stored payload is present. */
static esp_err_t merge_reject_nonempty_loaded(void *user_ctx, const void *loaded, size_t loaded_len,
                                              const void *defaults, size_t default_size, void *out)
{
    (void)user_ctx;
    if (loaded_len > 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    memcpy(out, defaults, default_size);
    return ESP_OK;
}

static esp_err_t merge_count_prefix(void *user_ctx, const void *loaded, size_t loaded_len, const void *defaults,
                                    size_t default_size, void *out)
{
    int *ctr = (int *)user_ctx;
    (*ctr)++;
    memcpy(out, defaults, default_size);
    if (loaded && loaded_len > 0) {
        size_t n = loaded_len < default_size ? loaded_len : default_size;
        memcpy(out, loaded, n);
    }
    return ESP_OK;
}

TEST_CASE("esp_config_manager init rejects invalid arguments", "[esp_config_manager]")
{
    esp_config_manager_handle_t h = NULL;
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);

    esp_config_storage_t store = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &store));

    esp_config_manager_cfg_t cfg = {
        .storage = store,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 1,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(NULL, &h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(&cfg, NULL));

    esp_config_manager_cfg_t bad = cfg;
    bad.storage = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(&bad, &h));

    static const esp_config_storage_ops_t s_ops_no_read = {
        .read = NULL,
        .write = mock_ram_write,
    };
    esp_config_storage_t junk = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_storage_init(&s_ops_no_read, (void *)&ram, &junk));
    TEST_ASSERT_NULL(junk);

    bad = cfg;
    bad.default_config = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(&bad, &h));

    bad = cfg;
    bad.default_size = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(&bad, &h));

    bad = cfg;
    bad.crypto_extra_size = 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_init(&bad, &h));

    TEST_ASSERT_NULL(h);
    esp_config_storage_deinit(store);
}

TEST_CASE("esp_config_manager deinit NULL is safe", "[esp_config_manager]")
{
    esp_config_manager_deinit(NULL);
}

TEST_CASE("esp_config_manager load rejects NULL output", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_load(h, NULL, NULL));
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager save rejects invalid image", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 3,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_save(h, NULL, sizeof(s_test_defaults)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_save(h, &s_test_defaults, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_save(h, &s_test_defaults, -1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_config_manager_save(h, &s_test_defaults,
                                                                   sizeof(s_test_defaults) + 1));
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager defaults path persists and reloads", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 4,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t out = {0};
    esp_config_load_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &out, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_DEFAULTS, info.source);
    TEST_ASSERT_EQUAL(0, info.primary_repair_scheduled);
    TEST_ASSERT_EQUAL_MEMORY(&s_test_defaults, &out, sizeof(out));
    TEST_ASSERT(ram.len[0] > 0 && ram.len[1] > 0);
    TEST_ASSERT_TRUE(ram.len[0] > cfg.default_size);

    memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &out, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_PRIMARY, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&s_test_defaults, &out, sizeof(out));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager save load roundtrip on RAM mock", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 5,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t w = s_test_defaults;
    w.u32 = 0xDEADBEEF;
    w.u16 = 3;
    memset(w.pad, 0xCC, sizeof(w.pad));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &w, sizeof(w)));

    test_pod_t r = {0};
    esp_config_load_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_PRIMARY, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager uses backup when primary missing", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 6,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t w = s_test_defaults;
    w.u32 = 7;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &w, sizeof(w)));

    TEST_ASSERT_EQUAL(ESP_OK, mock_ram_erase(&ram, ESP_CONFIG_SLOT_PRIMARY));
    TEST_ASSERT_EQUAL(0, ram.len[0]);
    TEST_ASSERT(ram.len[1] > 0);

    test_pod_t r = {0};
    esp_config_load_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_BACKUP, info.source);
    TEST_ASSERT_TRUE(info.primary_repair_scheduled);
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));

    memset(&r, 0, sizeof(r));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_PRIMARY, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager backup when primary CRC corrupt then repair", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 7,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t w = s_test_defaults;
    w.u32 = 99;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &w, sizeof(w)));

    if (ram.len[0] > 4) {
        ram.blob[0][4] ^= 0xFF;
    }

    test_pod_t r = {0};
    esp_config_load_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_BACKUP, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));

    memset(&r, 0, sizeof(r));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_PRIMARY, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager default merge keeps tail when stored payload shorter", "[esp_config_manager]")
{
    typedef struct __attribute__((packed)) {
        uint8_t  head[4];
        uint8_t  tail[12];
    } wide_t;

    const wide_t defs = {.head = {1, 2, 3, 4}, .tail = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}};
    uint8_t old_partial[2] = {0xAA, 0xBB};

    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_cfg_t old_cfg = {
        .storage = st,
        .default_config = old_partial,
        .default_size = sizeof(old_partial),
        .schema_version = 8,
    };
    esp_config_manager_handle_t old_h = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&old_cfg, &old_h));
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(old_h, old_partial, sizeof(old_partial)));
    esp_config_manager_deinit(old_h);

    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &defs,
        .default_size = sizeof(defs),
        .schema_version = 8,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    wide_t out;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &out, NULL));
    TEST_ASSERT_EQUAL(0xAA, out.head[0]);
    TEST_ASSERT_EQUAL(0xBB, out.head[1]);
    TEST_ASSERT_EQUAL(3, out.head[2]);
    TEST_ASSERT_EQUAL(4, out.head[3]);
    TEST_ASSERT_EQUAL_MEMORY(defs.tail, out.tail, sizeof(out.tail));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager merge_fn failure on defaults path", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 9,
        .merge_fn = merge_always_fail,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t out;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_config_manager_load(h, &out, NULL));
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager merge_fn failure after valid stored record", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));

    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 10,
    };

    esp_config_manager_handle_t h1 = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h1));
    test_pod_t w = s_test_defaults;
    w.u32 = 0xCAFEBABE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h1, &w, sizeof(w)));
    esp_config_manager_deinit(h1);

    esp_config_manager_handle_t h2 = NULL;
    esp_config_manager_cfg_t cfg2 = cfg;
    cfg2.merge_fn = merge_reject_nonempty_loaded;
    cfg2.merge_ctx = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg2, &h2));

    test_pod_t out;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, esp_config_manager_load(h2, &out, NULL));
    esp_config_manager_deinit(h2);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager custom merge_fn is invoked", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    int merges = 0;
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 11,
        .merge_fn = merge_count_prefix,
        .merge_ctx = &merges,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t out;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &out, NULL));
    TEST_ASSERT_EQUAL(1, merges);
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager XOR crypto roundtrip", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    xor_crypto_ctx_t x = {.key = 0x5A};
    esp_config_crypto_ops_t cry = {.encrypt = xor_encrypt, .decrypt = xor_decrypt, .ctx = &x};

    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 12,
        .crypto = &cry,
        .crypto_extra_size = 0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t w = s_test_defaults;
    w.u32 = 0x01020304;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &w, sizeof(w)));

    test_pod_t r = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &r, NULL));
    TEST_ASSERT_EQUAL_MEMORY(&w, &r, sizeof(r));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager save fails if any slot write fails", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 13,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    ram.fail_write_backup = true;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_config_manager_save(h, &s_test_defaults, sizeof(s_test_defaults)));
    ram.fail_write_backup = false;

    ram.fail_write_primary = true;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_config_manager_save(h, &s_test_defaults, sizeof(s_test_defaults)));
    ram.fail_write_primary = false;

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager repeated init deinit load save", "[esp_config_manager]")
{
    for (int i = 0; i < 15; i++) {
        mock_ram_ctx_t ram;
        mock_ram_reset(&ram);
        esp_config_storage_t st = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
        esp_config_manager_handle_t h = NULL;
        esp_config_manager_cfg_t cfg = {
            .storage = st,
            .default_config = &s_test_defaults,
            .default_size = sizeof(s_test_defaults),
            .schema_version = 15 + (uint32_t)i,
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));
        test_pod_t buf;
        TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &buf, NULL));
        buf.u32 = (uint32_t)i;
        TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &buf, sizeof(buf)));
        esp_config_manager_deinit(h);
        esp_config_storage_deinit(st);
    }
}

TEST_CASE("esp_config_manager defaults persist fails when storage write fails", "[esp_config_manager]")
{
    mock_ram_ctx_t ram;
    mock_ram_reset(&ram);
    ram.fail_write_primary = true;
    ram.fail_write_backup = true;

    esp_config_storage_t st = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, test_ram_storage_open(&ram, &st));
    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = st,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 20,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t out;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_config_manager_load(h, &out, NULL));
    esp_config_manager_deinit(h);
    esp_config_storage_deinit(st);
}

TEST_CASE("esp_config_manager NVS storage smoke", "[esp_config_manager]")
{
    const esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "cfg_mgr_ut",
        .key_primary = "p",
        .key_backup = "b",
    };

    nvs_handle_t nh;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open(nvs_cfg.nvs_namespace, NVS_READWRITE, &nh));
    nvs_erase_all(nh);
    nvs_commit(nh);
    nvs_close(nh);

    esp_config_storage_t nvs_store = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_storage_init_nvs(&nvs_cfg, &nvs_store));

    esp_config_manager_handle_t h = NULL;
    esp_config_manager_cfg_t cfg = {
        .storage = nvs_store,
        .default_config = &s_test_defaults,
        .default_size = sizeof(s_test_defaults),
        .schema_version = 100,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));

    test_pod_t one;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &one, NULL));
    TEST_ASSERT_EQUAL_MEMORY(&s_test_defaults, &one, sizeof(one));

    test_pod_t two = s_test_defaults;
    two.u32 = 0x600DF00D;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_save(h, &two, sizeof(two)));

    esp_config_manager_deinit(h);
    h = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_init(&cfg, &h));
    test_pod_t three;
    esp_config_load_info_t info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_config_manager_load(h, &three, &info));
    TEST_ASSERT_EQUAL(ESP_CONFIG_LOAD_SOURCE_PRIMARY, info.source);
    TEST_ASSERT_EQUAL_MEMORY(&two, &three, sizeof(three));

    esp_config_manager_deinit(h);
    esp_config_storage_deinit(nvs_store);
}
