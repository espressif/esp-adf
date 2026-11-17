/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "test_fixtures.h"
#include "esp_playlist.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"

typedef struct {
    esp_media_db_handle_t  db;
    esp_playlist_handle_t  playlist;
    SemaphoreHandle_t      lock;
    SemaphoreHandle_t      start;
    SemaphoreHandle_t      done;
    int                    writer_errors;
    int                    reader_errors;
} shared_fixture_t;

static void catalog_writer(void *arg)
{
    shared_fixture_t *f = arg;
    xSemaphoreTake(f->start, portMAX_DELAY);
    const esp_media_db_item_t item = {.name = "transient", .url = "file:/transient.mp3"};
    for (int i = 0; i < 32; i++) {
        xSemaphoreTake(f->lock, portMAX_DELAY);
        int count = 0;
        if (esp_media_db_add(f->db, &item, 1) != ESP_OK ||
            esp_media_db_get_count(f->db, &count) != ESP_OK || count != PLAYLIST_TEST_MEDIA_COUNT + 1 ||
            esp_media_db_remove(f->db, &item, 1) != ESP_OK) {
            f->writer_errors++;
        }
        xSemaphoreGive(f->lock);
        vTaskDelay(1);
    }
    xSemaphoreGive(f->done);
    vTaskDelete(NULL);
}

static void playlist_reader(void *arg)
{
    shared_fixture_t *f = arg;
    xSemaphoreTake(f->start, portMAX_DELAY);
    for (int i = 0; i < 32; i++) {
        xSemaphoreTake(f->lock, portMAX_DELAY);
        int count = 0;
        esp_playlist_info_t info = {0};
        char *json = NULL;
        if (esp_media_db_get_count(f->db, &count) != ESP_OK || count != PLAYLIST_TEST_MEDIA_COUNT ||
            esp_playlist_curr(f->playlist, &info) != ESP_OK || strcmp(info.media_name, "song_00") != 0 ||
            esp_playlist_export_ram(f->playlist, &json, NULL) != ESP_OK) {
            f->reader_errors++;
        }
        free(json);
        xSemaphoreGive(f->lock);
        vTaskDelay(1);
    }
    xSemaphoreGive(f->done);
    vTaskDelete(NULL);
}

TEST_CASE("playlist and catalog support two tasks serialized by the caller", "[esp_playlist][concurrency]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    shared_fixture_t f = {.db = fixture.media_db};
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "shared"
                                 }, &f.playlist));
    TEST_ESP_OK(esp_playlist_import_media(f.playlist, f.db, NULL));
    f.lock = xSemaphoreCreateMutex();
    f.start = xSemaphoreCreateCounting(2, 0);
    f.done = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(f.lock);
    TEST_ASSERT_NOT_NULL(f.start);
    TEST_ASSERT_NOT_NULL(f.done);
    TaskHandle_t writer = NULL;
    TaskHandle_t reader = NULL;
    BaseType_t first = xTaskCreate(catalog_writer, "catalog_writer", 8192, &f, 5, &writer);
    BaseType_t second = xTaskCreate(playlist_reader, "playlist_reader", 8192, &f, 5, &reader);
    if (first != pdPASS || second != pdPASS) {
        if (writer) {
            vTaskDelete(writer);
        }
        if (reader) {
            vTaskDelete(reader);
        }
        TEST_FAIL_MESSAGE("Cannot create the two test tasks");
    }
    xSemaphoreGive(f.start);
    xSemaphoreGive(f.start);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(f.done, pdMS_TO_TICKS(30000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(f.done, pdMS_TO_TICKS(30000)));
    vTaskDelay(2);
    TEST_ASSERT_EQUAL(0, f.writer_errors);
    TEST_ASSERT_EQUAL(0, f.reader_errors);
    vSemaphoreDelete(f.lock);
    vSemaphoreDelete(f.start);
    vSemaphoreDelete(f.done);
    TEST_ESP_OK(esp_playlist_del(f.playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}
