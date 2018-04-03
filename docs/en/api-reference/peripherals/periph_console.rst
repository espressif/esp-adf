Console Peripheral
==================

Console Peripheral is used to control the Audio application from the terminal screen. It provides 2 ways do execute command, one sends an event to esp_peripherals (for a command without parameters), another calls a callback function (need parameters). If there is a callback function, no event will be sent.

Please make sure that the lifetime of :cpp:type:`periph_console_cmd_t` must be ensured during console operation, :cpp:func:`periph_console_init` only reference, does not make a copy.

Code example
------------

.. highlight:: c

::

    #include "freertos/FreeRTOS.h"
    #include "esp_log.h"
    #include "esp_peripherals.h"
    #include "periph_console.h"

    static const char *TAG = "ESP_PERIPH_TEST";

    static esp_err_t _periph_event_handle(audio_event_iface_msg_t *event, void *context)
    {
        switch ((int)event->source_type) {
            case PERIPH_ID_CONSOLE:
                ESP_LOGI(TAG, "CONSOLE, command id=%d", event->cmd);
                break;
        }
        return ESP_OK;
    }

    esp_err_t console_test_cb(esp_periph_handle_t periph, int argc, char *argv[])
    {
        int i;
        ESP_LOGI(TAG, "CONSOLE Callback, argc=%d", argc);
        for (i=0; i<argc; i++) {
            ESP_LOGI(TAG, "CONSOLE Args[%d] %s", i, argv[i]);
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

        const periph_console_cmd_t cmd[]= {
            { .cmd = "play", .id = 1, .help = "Play audio" },
            { .cmd = "stop", .id = 2, .help = "Stop audio" },
            { .cmd = "test", .help = "test console", .func = console_test_cb },
        };

        periph_console_cfg_t console_cfg = {
            .command_num = sizeof(cmd)/sizeof(periph_console_cmd_t),
            .commands = cmd,
        };
        esp_periph_handle_t console_handle = periph_console_init(&console_cfg);
        esp_periph_start(console_handle);
        vTaskDelay(30000/portTICK_RATE_MS);
        ESP_LOGI(TAG, "Stopped");
        esp_periph_destroy();

    }

API Reference
-------------

.. include:: /_build/inc/periph_console.inc
