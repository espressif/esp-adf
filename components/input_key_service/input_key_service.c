#include <string.h>
#include "esp_log.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "rom/queue.h"
#include "esp_peripherals.h"
#include "input_key_service.h"
#include "audio_event_iface.h"

#define INPUT_KEY_SERVICE_TASK_STACK_SIZE (2 * 1024)
#define INPUT_KEY_SERVICE_TASK_PRIORITY   (5)
#define INPUT_KEY_SERVICE_TASK_ON_CORE    (1)

static const char *TAG = "INPUT_KEY_SERVICE";

typedef struct input_key {
    input_key_service_info_t  input_key_info;
    STAILQ_ENTRY(input_key)  entries;
} input_key_node_t;

typedef struct {
    periph_service_state_t ser_state;
    esp_periph_set_handle_t periph_handle;
    STAILQ_HEAD(key_list, input_key) input_info_list;
} input_key_service_t;

static esp_err_t input_key_service_event_send(periph_service_handle_t input_key_handle, periph_service_state_t state)
{
    AUDIO_NULL_CHECK(TAG, input_key_handle, return ESP_ERR_INVALID_ARG);

    input_key_service_t *input_key_ser = (input_key_service_t *)periph_service_get_data(input_key_handle);
    QueueHandle_t input_ser_queue = esp_periph_set_get_queue(input_key_ser->periph_handle);

    audio_event_iface_msg_t msg = {0};
    msg.source = (void *)input_key_handle;
    msg.cmd = state;

    if (xQueueSend(input_ser_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "input key service event send failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t input_key_service_event_receive(periph_service_handle_t handle, void *msg, TickType_t ticks)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);

    input_key_service_t *input_key_ser = periph_service_get_data(handle);
    QueueHandle_t input_ser_queue = esp_periph_set_get_queue(input_key_ser->periph_handle);

    if (xQueueReceive(input_ser_queue, msg, ticks) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int get_input_key_user_id(input_key_service_t *input_key_ser, int source_type, int act_id)
{
    AUDIO_NULL_CHECK(TAG, input_key_ser, return USER_ID_UNKNOWN);

    input_key_service_info_t *key_info = NULL;
    input_key_node_t *tmp_node = NULL;

    STAILQ_FOREACH(tmp_node, &input_key_ser->input_info_list, entries) {
        key_info = &(tmp_node->input_key_info);
        if (key_info->type == source_type && key_info->act_id == act_id) {
            return key_info->user_id;
        }
    }

    return USER_ID_UNKNOWN;
}

static void input_key_service_task(void *parameters)
{
    periph_service_handle_t input_key_handle = (periph_service_handle_t)parameters;

    periph_service_event_t ser_evt = {0};
    input_key_service_t *input_key_ser = periph_service_get_data(input_key_handle);

    input_key_ser->ser_state = PERIPH_SERVICE_STATE_RUNNING;
    audio_event_iface_msg_t msg = {0};

    while (1) {
        if (input_key_service_event_receive(input_key_handle, &msg, portMAX_DELAY) == ESP_OK) {
            ser_evt.data = (void *)get_input_key_user_id(input_key_ser, msg.source_type, (int)msg.data);
            if ((int)ser_evt.data >= 0) {
                ser_evt.type = msg.cmd; // action
                ser_evt.source = (void *)msg.source_type;
                ser_evt.len = (int)msg.data;
                periph_service_callback(input_key_handle, &ser_evt);
            }
        }

        if (msg.source == input_key_handle && msg.cmd == PERIPH_SERVICE_STATE_STOPED) {
            ESP_LOGW(TAG, "The input key service will be destroyed");
            break;
        }
    }

    input_key_node_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &input_key_ser->input_info_list, entries, tmp) {
        STAILQ_REMOVE(&input_key_ser->input_info_list, item, input_key, entries);
        free(item);
    }
    free(input_key_ser);
    vTaskDelete(NULL);
}

static esp_err_t input_key_service_start(periph_service_handle_t input_key_handle)
{
    return ESP_OK;
}

static esp_err_t input_key_service_stop(periph_service_handle_t input_key_handle)
{
    return ESP_OK;
}

static esp_err_t input_key_service_destroy(periph_service_handle_t input_key_handle)
{
    AUDIO_NULL_CHECK(TAG, input_key_handle, return ESP_ERR_INVALID_ARG);
    input_key_service_event_send(input_key_handle, PERIPH_SERVICE_STATE_STOPED);
    return ESP_OK;
}

periph_service_state_t get_input_key_service_state(periph_service_handle_t input_key_handle)
{
    AUDIO_NULL_CHECK(TAG, input_key_handle, return PERIPH_SERVICE_STATE_UNKNOWN);
    input_key_service_t *input_key_ser = (input_key_service_t *)periph_service_get_data(input_key_handle);
    return input_key_ser->ser_state;
}

esp_err_t input_key_service_add_key(periph_service_handle_t input_key_handle, input_key_service_info_t *input_key_info, int add_key_num)
{
    AUDIO_NULL_CHECK(TAG, input_key_handle, return ESP_ERR_INVALID_ARG);
    AUDIO_NULL_CHECK(TAG, input_key_info, return ESP_ERR_INVALID_ARG);
    if (add_key_num <= 0) {
        return ESP_FAIL;
    }

    input_key_service_t *input_key_ser = periph_service_get_data(input_key_handle);
    AUDIO_NULL_CHECK(TAG, input_key_ser, return ESP_FAIL);

    for (int i = 0; i < add_key_num; i++) {
        input_key_node_t *input_key_node = (input_key_node_t *)audio_calloc(1, sizeof(input_key_node_t));
        memcpy(&input_key_node->input_key_info, &input_key_info[i], sizeof(input_key_service_info_t));
        STAILQ_INSERT_TAIL(&input_key_ser->input_info_list, input_key_node, entries);
    }
    return ESP_OK;
}

periph_service_handle_t input_key_service_create(esp_periph_set_handle_t periph_handle)
{
    AUDIO_NULL_CHECK(TAG, periph_handle, return NULL);

    periph_service_config_t input_cfg = {
        .task_stack = INPUT_KEY_SERVICE_TASK_STACK_SIZE,
        .task_prio = INPUT_KEY_SERVICE_TASK_PRIORITY,
        .task_core = INPUT_KEY_SERVICE_TASK_ON_CORE,
        .task_func = input_key_service_task,
        .service_start = input_key_service_start,
        .service_stop = input_key_service_stop,
        .service_destroy = input_key_service_destroy,
        .service_ioctl = NULL,
        .service_name = "input_key_service",
    };

    input_key_service_t *input_key_ser = NULL;
    periph_service_handle_t input_key_handle = NULL;

    input_key_ser = (input_key_service_t *)audio_calloc(1, sizeof(input_key_service_t));
    AUDIO_NULL_CHECK(TAG, input_key_ser, goto _create_service_failed);

    input_key_ser->periph_handle = periph_handle;
    input_key_ser->ser_state = PERIPH_SERVICE_STATE_UNKNOWN;

    STAILQ_INIT(&input_key_ser->input_info_list);

    input_cfg.user_data = (void *)input_key_ser;

    input_key_handle = periph_service_create(&input_cfg);
    AUDIO_NULL_CHECK(TAG, input_key_handle, goto _create_service_failed);

    return input_key_handle;

_create_service_failed:
    if (input_key_handle) {
        free(input_key_handle);
        input_key_handle = NULL;
    }
    if (input_key_ser) {
        free(input_key_ser);
        input_key_ser = NULL;
    }
    return NULL;
}
