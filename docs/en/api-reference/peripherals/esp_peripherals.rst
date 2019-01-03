ESP Peripherals
===============

This library simplifies the management of peripherals, by pooling and monitoring in a single task, adding basic functions to send and receive events. And it also provides APIs to easily integrate new peripherals.

.. note::

    Note that if you do not intend to integrate new peripherals into esp_peripherals, you are only interested in simple api ``esp_periph_init``, ``esp_periph_start``, ``esp_periph_stop`` and ``esp_periph_destroy``.  If you want to integrate new peripherals, please refer to :doc:`Periph Button <./periph_button>` source code

Examples
--------

.. highlight:: c

::

    #include "esp_log.h"
    #include "esp_peripherals.h"
    #include "periph_sdcard.h"
    #include "periph_button.h"
    #include "periph_touch.h"

    static const char *TAG = "ESP_PERIPH_TEST";

    static esp_err_t _periph_event_handle(audio_event_iface_msg_t *event, void *context)
    {
        switch ((int)event->source_type) {
            case PERIPH_ID_BUTTON:
                ESP_LOGI(TAG, "BUTTON[%d], event->event_id=%d", (int)event->data, event->cmd);
                break;
            case PERIPH_ID_SDCARD:
                ESP_LOGI(TAG, "SDCARD status, event->event_id=%d", event->cmd);
                break;
            case PERIPH_ID_TOUCH:
                ESP_LOGI(TAG, "TOUCH[%d], event->event_id=%d", (int)event->data, event->cmd);
                break;
            case PERIPH_ID_WIFI:
                ESP_LOGI(TAG, "WIFI, event->event_id=%d", event->cmd);
                break;
        }
        return ESP_OK;
    }

    void app_main(void)
    {
        // Initialize Peripherals pool
        esp_periph_config_t periph_cfg = {
            .event_handle = _periph_event_handle,
            .user_context = NULL,
        };
        esp_periph_init(&periph_cfg);

        // Setup SDCARD peripheral
        periph_sdcard_cfg_t sdcard_cfg = {
            .root = "/sdcard",
            .card_detect_pin = GPIO_NUM_34,
        };
        esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);

        // Setup BUTTON peripheral
        periph_button_cfg_t btn_cfg = {
            .gpio_mask = GPIO_SEL_36 | GPIO_SEL_39
        };
        esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

        // Setup TOUCH peripheral
        periph_touch_cfg_t touch_cfg = {
            .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
            .tap_threshold_percent = 70,
        };
        esp_periph_handle_t touch_handle = periph_touch_init(&touch_cfg);

        // Start all peripheral
        esp_periph_start(button_handle);
        esp_periph_start(sdcard_handle);
        esp_periph_start(touch_handle);
        vTaskDelay(10*1000/portTICK_RATE_MS);

        //Stop button peripheral
        esp_periph_stop(button_handle);
        vTaskDelay(10*1000/portTICK_RATE_MS);

        //Start button again
        esp_periph_start(button_handle);
        vTaskDelay(10*1000/portTICK_RATE_MS);

        //Stop & destroy all peripherals
        esp_periph_destroy();
    }


API Reference
-------------

.. include:: /_build/inc/esp_peripherals.inc
