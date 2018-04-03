Audio Peripheral
================

Example
-------

::

    esp_periph_config_t periph_cfg = {
        .event_handle = _periph_event_handle,
        .user_context = NULL,
    };
    esp_periph_init(&periph_cfg);

    // Initialize button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_36 | GPIO_SEL_39
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);


    esp_periph_start(button_handle);

    ESP_LOGI(TAG, "wait for button Pressed or touched");

    ESP_LOGI(TAG, "running...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    esp_periph_stop(button_handle);
    ESP_LOGI(TAG, "stop button...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    esp_periph_start(button_handle);
    ESP_LOGI(TAG, "start button...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "destroy...");
    esp_periph_destroy();

