/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "soc/uart_pins.h"

#include "network_mgr.h"
#include "power_mgr.h"
#include "wakeup_mgr.h"

static const char *TAG = "WAKEUP_MGR";

#define DEFAULT_UART_NUM       CONFIG_ESP_CONSOLE_UART_NUM
#define UART_WAKEUP_THRESHOLD  3
#define UART_EVENT_QUEUE_SIZE  8
#define UART_RX_BUFFER_SIZE    2048

#if CONFIG_EXAMPLE_GPIO_WAKEUP_LEVEL_LOW
#define GPIO_WAKEUP_INTR_TYPE       GPIO_INTR_LOW_LEVEL
#define GPIO_WAKEUP_ACTIVE_LEVEL    0
#define GPIO_WAKEUP_INACTIVE_LEVEL  1
#else
#define GPIO_WAKEUP_INTR_TYPE       GPIO_INTR_HIGH_LEVEL
#define GPIO_WAKEUP_ACTIVE_LEVEL    1
#define GPIO_WAKEUP_INACTIVE_LEVEL  0
#endif  /* CONFIG_EXAMPLE_GPIO_WAKEUP_LEVEL_LOW */

static esp_timer_handle_t s_wakeup_timer;
static QueueHandle_t s_uart_event_queue;
static TaskHandle_t s_uart_wakeup_task_handle;

static void wakeup_timer_cb(void *arg)
{
    power_save_context_t *app_ctx = (power_save_context_t *)arg;
    if (app_ctx == NULL || !app_ctx->waiting_for_wakeup) {
        return;
    }
    ESP_LOGI(TAG, "Wakeup event: timer");
    xEventGroupSetBits(app_ctx->wakeup_event, WAKEUP_TIMER_BIT);
}

esp_err_t wakeup_mgr_wakeup_source_name(EventBits_t bits, const char **name)
{
    ESP_GMF_CHECK(TAG, name != NULL, return ESP_ERR_INVALID_ARG, "Wakeup source name buffer is NULL");
    if (bits & WAKEUP_MQTT_BIT) {
        *name = "MQTT";
        return ESP_OK;
    }
    if (bits & WAKEUP_TIMER_BIT) {
        *name = "TIMER";
        return ESP_OK;
    }
    if (bits & WAKEUP_GPIO_BIT) {
        *name = "GPIO";
        return ESP_OK;
    }
    if (bits & WAKEUP_UART_BIT) {
        *name = "UART";
        return ESP_OK;
    }
    *name = "UNKNOWN";
    return ESP_OK;
}

static void arm_timer_wakeup(void)
{
    if (s_wakeup_timer == NULL) {
        return;
    }
    esp_timer_stop(s_wakeup_timer);
    esp_err_t ret = esp_timer_start_once(s_wakeup_timer, CONFIG_EXAMPLE_TIMER_WAKEUP_MS * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to arm timer wakeup: %s", esp_err_to_name(ret));
    }
}

static esp_err_t configure_timer_wakeup(power_save_context_t *app_ctx)
{
    esp_timer_create_args_t timer_args = {
        .callback = wakeup_timer_cb,
        .arg = app_ctx,
        .name = "wakeup_timer",
    };
    ESP_GMF_RET_ON_ERROR(TAG, esp_timer_create(&timer_args, &s_wakeup_timer), return err_rc_,
                         "Failed to create wakeup timer");
    ESP_LOGI(TAG, "Timer wakeup configured, timeout=%d ms", CONFIG_EXAMPLE_TIMER_WAKEUP_MS);
    return ESP_OK;
}

#if CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP
static void gpio_wakeup_isr(void *arg)
{
    power_save_context_t *app_ctx = (power_save_context_t *)arg;
    /* Disable the level-triggered interrupt first to prevent continuous ISR storm
     * while the pin remains at the active level. Re-enabled in wakeup_mgr_wait(). */
    gpio_intr_disable(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
    if (app_ctx == NULL || !app_ctx->waiting_for_wakeup) {
        return;
    }
    BaseType_t higher_priority_task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(app_ctx->wakeup_event, WAKEUP_GPIO_BIT, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}
#endif  /* CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP */

static esp_err_t configure_gpio_wakeup(power_save_context_t *app_ctx)
{
#if CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM),
        .mode = GPIO_MODE_INPUT,
#if CONFIG_EXAMPLE_GPIO_WAKEUP_LEVEL_LOW
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
#else
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
#endif  /* CONFIG_EXAMPLE_GPIO_WAKEUP_LEVEL_LOW */
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_GMF_RET_ON_ERROR(TAG, gpio_config(&io_conf), return err_rc_, "Failed to configure GPIO");
    ESP_GMF_RET_ON_ERROR(TAG, gpio_wakeup_enable(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM, GPIO_WAKEUP_INTR_TYPE),
                         return err_rc_, "Failed to enable GPIO wakeup");
    ESP_GMF_RET_ON_ERROR(TAG, esp_sleep_enable_gpio_wakeup(), return err_rc_, "Failed to enable sleep GPIO wakeup");
    gpio_sleep_sel_dis(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
    ESP_LOGI(TAG, "Waiting for GPIO%d to become inactive (level=%d)...",
             CONFIG_EXAMPLE_GPIO_WAKEUP_NUM, GPIO_WAKEUP_INACTIVE_LEVEL);
    int count = 0;
    while (gpio_get_level(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM) != GPIO_WAKEUP_INACTIVE_LEVEL) {
        vTaskDelay(pdMS_TO_TICKS(10));
        count++;
        if (count > 100) {
            ESP_LOGW(TAG, "GPIO%d is still active after 1000ms, giving up", CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
            return ESP_ERR_TIMEOUT;
        }
    }
    esp_err_t ret = gpio_install_isr_service(0);
    if (!(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)) {
        return ret;
    }
    ESP_GMF_RET_ON_ERROR(TAG, gpio_isr_handler_add(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM, gpio_wakeup_isr, app_ctx),
                         return err_rc_, "Failed to add GPIO ISR handler");
    ESP_LOGI(TAG, "GPIO wakeup enabled, gpio=%d, active level=%d",
             CONFIG_EXAMPLE_GPIO_WAKEUP_NUM, GPIO_WAKEUP_ACTIVE_LEVEL);
#else
    ESP_LOGI(TAG, "GPIO wakeup disabled by config");
    (void)app_ctx;
#endif  /* CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP */
    return ESP_OK;
}

#if CONFIG_EXAMPLE_ENABLE_UART_WAKEUP
static void uart_wakeup_task(void *arg)
{
    power_save_context_t *app_ctx = (power_save_context_t *)arg;
    uart_event_t event;
    while (xQueueReceive(s_uart_event_queue, &event, portMAX_DELAY)) {
        bool uart_wakeup = false;
#if SOC_UART_SUPPORT_WAKEUP_INT
        if (event.type == UART_WAKEUP && app_ctx->waiting_for_wakeup) {
            uart_wakeup = true;
        }
#endif  /* SOC_UART_SUPPORT_WAKEUP_INT */
        if (event.type == UART_DATA && app_ctx->waiting_for_wakeup) {
            uart_wakeup = true;
        }
        if (uart_wakeup) {
            ESP_LOGI(TAG, "Wakeup event: uart, event=%d", event.type);
            xEventGroupSetBits(app_ctx->wakeup_event, WAKEUP_UART_BIT);
        } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
            uart_flush_input(DEFAULT_UART_NUM);
            xQueueReset(s_uart_event_queue);
        }
    }
    s_uart_wakeup_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif  /* CONFIG_EXAMPLE_ENABLE_UART_WAKEUP */

static esp_err_t configure_uart_wakeup(power_save_context_t *app_ctx)
{
#if CONFIG_EXAMPLE_ENABLE_UART_WAKEUP
    uart_config_t uart_cfg = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif  /* CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 */
    };
    /* Note: DEFAULT_UART_NUM is the console UART port. We intentionally do NOT
     * call uart_driver_delete() on error paths because that would kill the
     * console/log output. Any resources allocated by uart_driver_install() for
     * the console UART will be released by the OS on task exit. */
    ESP_GMF_RET_ON_ERROR(TAG, uart_driver_install(DEFAULT_UART_NUM, UART_RX_BUFFER_SIZE, UART_RX_BUFFER_SIZE,
                                                  UART_EVENT_QUEUE_SIZE, &s_uart_event_queue, 0),
                         return err_rc_, "Failed to install UART driver");
    ESP_GMF_CHECK(TAG, s_uart_event_queue != NULL, return ESP_ERR_INVALID_STATE, "Failed to get UART event queue");

    ESP_GMF_RET_ON_ERROR(TAG, uart_wait_tx_idle_polling(DEFAULT_UART_NUM), return err_rc_,
                         "Failed to wait UART TX idle");
    ESP_GMF_RET_ON_ERROR(TAG, uart_param_config(DEFAULT_UART_NUM, &uart_cfg), return err_rc_,
                         "Failed to configure UART");
    /* Note: U0TXD_GPIO_NUM/U0RXD_GPIO_NUM are UART0-specific pin macros.
     * If DEFAULT_UART_NUM is not UART_NUM_0, the correct pins for that UART
     * must be used instead (e.g., via uart_get_pin() or conditional macros). */
    ESP_GMF_RET_ON_ERROR(TAG, uart_set_pin(DEFAULT_UART_NUM, U0TXD_GPIO_NUM, U0RXD_GPIO_NUM,
                                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                         return err_rc_, "Failed to set UART pins");
    ESP_GMF_RET_ON_ERROR(TAG, gpio_sleep_set_direction(U0RXD_GPIO_NUM, GPIO_MODE_INPUT), return err_rc_,
                         "Failed to set UART RX sleep direction");
    ESP_GMF_RET_ON_ERROR(TAG, gpio_sleep_set_pull_mode(U0RXD_GPIO_NUM, GPIO_PULLUP_ONLY), return err_rc_,
                         "Failed to set UART RX sleep pull mode");
    ESP_GMF_RET_ON_ERROR(TAG, uart_set_wakeup_threshold(DEFAULT_UART_NUM, UART_WAKEUP_THRESHOLD), return err_rc_,
                         "Failed to set UART wakeup threshold");
    ESP_GMF_RET_ON_ERROR(TAG, esp_sleep_enable_uart_wakeup(DEFAULT_UART_NUM), return err_rc_,
                         "Failed to enable UART wakeup");
    gpio_sleep_sel_dis(U0RXD_GPIO_NUM);
    gpio_sleep_sel_dis(U0TXD_GPIO_NUM);
    BaseType_t created = xTaskCreate(uart_wakeup_task, "uart_wakeup_task", 4096, app_ctx, 5,
                                     &s_uart_wakeup_task_handle);
    ESP_GMF_CHECK(TAG, created == pdPASS, return ESP_ERR_NO_MEM, "Failed to create UART wakeup task");
    ESP_LOGI(TAG, "UART wakeup enabled, uart=%d, threshold=%d", DEFAULT_UART_NUM, UART_WAKEUP_THRESHOLD);
#else
    ESP_LOGI(TAG, "UART wakeup disabled by config");
    (void)app_ctx;
#endif  /* CONFIG_EXAMPLE_ENABLE_UART_WAKEUP */
    return ESP_OK;
}

esp_err_t wakeup_mgr_configure_sources(power_save_context_t *app_ctx)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL && app_ctx->wakeup_event != NULL, return ESP_ERR_INVALID_ARG,
                  "Invalid context");
    ESP_GMF_RET_ON_ERROR(TAG, configure_gpio_wakeup(app_ctx), return err_rc_, "Failed to configure GPIO wakeup");
    ESP_GMF_RET_ON_ERROR(TAG, configure_uart_wakeup(app_ctx), return err_rc_, "Failed to configure UART wakeup");
    ESP_GMF_RET_ON_ERROR(TAG, configure_timer_wakeup(app_ctx), return err_rc_, "Failed to configure timer wakeup");
    return ESP_OK;
}

esp_err_t wakeup_mgr_wait(power_save_context_t *app_ctx)
{
    const EventBits_t wait_bits = WAKEUP_TIMER_BIT | WAKEUP_MQTT_BIT | WAKEUP_GPIO_BIT | WAKEUP_UART_BIT;

    ESP_GMF_CHECK(TAG, app_ctx != NULL && app_ctx->wakeup_event != NULL, return ESP_ERR_INVALID_ARG,
                  "Invalid context");
    xEventGroupClearBits(app_ctx->wakeup_event, wait_bits);
    app_ctx->last_wakeup_bits = 0;
    app_ctx->waiting_for_wakeup = true;
    arm_timer_wakeup();
    esp_err_t wifi_ret = network_mgr_set_wifi_power_save_mode(WIFI_PS_MAX_MODEM, "enter idle low power");
    if (wifi_ret != ESP_OK) {
        app_ctx->waiting_for_wakeup = false;
        ESP_LOGE(TAG, "Failed to enter Wi-Fi low power mode");
        return wifi_ret;
    }
    esp_err_t pm_ret = power_mgr_release_audio_lock(app_ctx, "enter idle low power");
    if (pm_ret != ESP_OK) {
        app_ctx->waiting_for_wakeup = false;
        ESP_LOGE(TAG, "Failed to release PM lock");
        return pm_ret;
    }
#if CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP
    /* Re-enable GPIO interrupt disabled by gpio_wakeup_isr() on previous wakeup.
     * Must be done here, after all pre-wait checks passed, to prevent ISR storm
     * if waiting_for_wakeup gets cleared on an early return path. */
    gpio_intr_enable(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
#endif  /* CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP */
    ESP_LOGI(TAG, "Player idle; waiting for wakeup (UART/MQTT/GPIO/timer) in automatic light sleep");
    EventBits_t bits = xEventGroupWaitBits(app_ctx->wakeup_event, wait_bits, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_EXAMPLE_WAKEUP_WAIT_MS));
    app_ctx->waiting_for_wakeup = false;
    ESP_GMF_RET_ON_ERROR(TAG, power_mgr_acquire_audio_lock(app_ctx, "wakeup handled"),
                         return err_rc_, "Failed to acquire PM lock");
    ESP_GMF_RET_ON_ERROR(TAG, network_mgr_set_wifi_power_save_mode(WIFI_PS_NONE, "wakeup handled"),
                         return err_rc_, "Failed to leave Wi-Fi low power mode");
    ESP_GMF_CHECK(TAG, bits != 0, return ESP_ERR_TIMEOUT, "No wakeup event received");
    app_ctx->last_wakeup_bits = bits & wait_bits;
    const char *source_name = "UNKNOWN";
    ESP_GMF_RET_ON_ERROR(TAG, wakeup_mgr_wakeup_source_name(app_ctx->last_wakeup_bits, &source_name),
                         return err_rc_, "Failed to get wakeup source name");
    ESP_LOGI(TAG, "Wakeup handled by %s", source_name);
    return ESP_OK;
}

void wakeup_mgr_deinit(power_save_context_t *app_ctx)
{
    if (app_ctx) {
        app_ctx->waiting_for_wakeup = false;
    }
    if (s_wakeup_timer) {
        esp_timer_stop(s_wakeup_timer);
        esp_timer_delete(s_wakeup_timer);
        s_wakeup_timer = NULL;
    }
#if CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP
    gpio_isr_handler_remove(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
    gpio_wakeup_disable(CONFIG_EXAMPLE_GPIO_WAKEUP_NUM);
#endif  /* CONFIG_EXAMPLE_ENABLE_GPIO_WAKEUP */
#if CONFIG_EXAMPLE_ENABLE_UART_WAKEUP
    if (s_uart_wakeup_task_handle) {
        vTaskDelete(s_uart_wakeup_task_handle);
        s_uart_wakeup_task_handle = NULL;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_UART);
#endif  /* CONFIG_EXAMPLE_ENABLE_UART_WAKEUP */
}
