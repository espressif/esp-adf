/* Write and read NVS with ESP_DISPATCHER in a task which its stack is in external memory.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_error.h"
#include "audio_mem.h"
#include "esp_dispatcher.h"
#include "esp_log.h"
#include "nvs_action.h"
#include "nvs_flash.h"

static const char *TAG = "DISPATCHER_EXAMPLE";

static esp_err_t nvs_write_str(void *instance, action_arg_t *arg, action_result_t *result)
{
    nvs_handle my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        goto exit;
    } else {
        ESP_LOGI(TAG, "Write %s to NVS ... ", (char *)arg->data);
        err = nvs_set_str(my_handle, "hello", (char *)arg->data);
        ESP_LOGI(TAG, "Operation %s", (err == ESP_OK) ? "Done" : "Failed");
        if (err != ESP_OK) {
            goto exit;
        }
        ESP_LOGI(TAG, "Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        ESP_LOGI(TAG, "Operation %s", (err == ESP_OK) ? "Done" : "Failed");
        // Close
        nvs_close(my_handle);
    }
exit:
    if (arg->data) {
        free(arg->data);
    }
    return err;
}

static esp_err_t nvs_read_str(void *instance, action_arg_t *arg, action_result_t *result)
{
    nvs_handle my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        goto exit;
    } else {
        ESP_LOGI(TAG, "Read from NVS ... ");
        result->len = 128;
        result->data = audio_calloc(1, result->len);
        err = nvs_get_str(my_handle, "hello", (char *)result->data, (size_t *)&result->len);
        ESP_LOGI(TAG, "Operation %s", (err == ESP_OK) ? "Done" : "Failed");
        if (err != ESP_OK) {
            goto exit;
        }
        // Close
        nvs_close(my_handle);
    }
exit:
    return err;
}

static void nvs_read_cb(action_result_t result, void *user_data)
{
    if (result.data) {
        ESP_LOGI(TAG, "Read result: %s", (char *)result.data);
        free(result.data);
    }
}

static void task(void *param)
{
    esp_dispatcher_handle_t dispatcher = (esp_dispatcher_handle_t)param;
    action_result_t result = { 0 };
    action_arg_t arg = { 0 };

    char *str = "Hello, ESP dispatcher!";
    arg.data = calloc(1, strlen(str) + 1);
    memcpy(arg.data, str, strlen(str));
    arg.len = strlen(str);

    ESP_LOGI(TAG, "Sync invoke begin");
    esp_dispatcher_execute_with_func(dispatcher, nvs_write_str, NULL, &arg, &result);
    ESP_LOGI(TAG, "Sync invoke end with %d\n", result.err);

    ESP_LOGI(TAG, "Async invoke begin");
    esp_dispatcher_execute_with_func_async(dispatcher, nvs_read_str, NULL, NULL, nvs_read_cb, NULL);
    ESP_LOGI(TAG, "async invoke end\n");

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Write and Read NVS with dispatcher and actions.");

    ESP_LOGI(TAG, "Open");
    nvs_handle nvs_sync_handle = 0;
    nvs_action_open_args_t open = {
        .name = "action_sync",
        .open_mode = NVS_READWRITE,
    };
    action_arg_t open_arg = {
        .data = &open,
        .len = sizeof(nvs_action_open_args_t),
    };
    memset(&result, 0x00, sizeof(action_result_t));
    esp_dispatcher_execute_with_func(dispatcher, nvs_action_open, NULL, &open_arg, &result);
    if (result.err == ESP_OK && result.data != NULL) {
        nvs_sync_handle = *(nvs_handle *)result.data;
        free(result.data);
    } else {
        ESP_LOGE(TAG, "Open nvs failed");
        goto exit;
    }

    ESP_LOGI(TAG, "Set a u8 number");
    nvs_action_set_args_t set = {
        .key = "action_sync",
        .type = NVS_TYPE_U8,
        .value.u8 = 0xAB,
        .len = sizeof(uint8_t),
    };
    action_arg_t set_arg = {
        .data = &set,
        .len = sizeof(nvs_action_set_args_t),
    };
    memset(&result, 0x00, sizeof(action_result_t));
    esp_dispatcher_execute_with_func(dispatcher, nvs_action_set, (void *)nvs_sync_handle, &set_arg, &result);
    AUDIO_CHECK(TAG, result.err == ESP_OK, goto exit, "Got error");

    ESP_LOGI(TAG, "Commit");
    memset(&result, 0x00, sizeof(action_result_t));
    esp_dispatcher_execute_with_func(dispatcher, nvs_action_commit, (void *)nvs_sync_handle, NULL, &result);
    AUDIO_CHECK(TAG, result.err == ESP_OK, goto exit, "Got error");

    ESP_LOGI(TAG, "Read from nvs");
    nvs_action_get_args_t get = {
        .key = "action_sync",
        .type = NVS_TYPE_U8,
        .wanted_size = sizeof(uint8_t),
    };
    action_arg_t get_arg = {
        .data = &get,
        .len = sizeof(nvs_action_get_args_t),
    };
    memset(&result, 0x00, sizeof(action_result_t));
    esp_dispatcher_execute_with_func(dispatcher, nvs_action_get, (void *)nvs_sync_handle, &get_arg, &result);
    if (result.err == ESP_OK && result.data) {
        uint8_t *ret = (uint8_t *)result.data;
        ESP_LOGI(TAG, "Read %d from nvs", *ret);
        free(result.data);
    } else {
        ESP_LOGE(TAG, "Get return %d, data %p", result.err, result.data);
        goto exit;
    }

    ESP_LOGI(TAG, "Close");
    memset(&result, 0x00, sizeof(action_result_t));
    esp_dispatcher_execute_with_func(dispatcher, nvs_action_close, (void *)nvs_sync_handle, NULL, &result);

exit:
    ESP_LOGW(TAG, "Task exit");
    vTaskDelete(NULL);
}

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_dispatcher_config_t d_cfg = ESP_DISPATCHER_CONFIG_DEFAULT();
    d_cfg.stack_in_ext = false;  // Need flash operation.
    esp_dispatcher_handle_t dispatcher = esp_dispatcher_create(&d_cfg);

    const size_t stack_size = 3096;
    StackType_t *stack = audio_calloc(1, stack_size);
    static StaticTask_t tcb = { 0 };
    xTaskCreateStaticPinnedToCore((TaskFunction_t)&task, "test_task", stack_size, dispatcher, 5, stack, &tcb, 1);
    ESP_LOGI(TAG, "Task create\n");
}
