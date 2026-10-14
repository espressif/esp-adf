ESP LLM Volc
=============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`esp_llm_volc <https://components.espressif.com/components/espressif/esp_llm_volc>`__ is the Volcengine adapter for :doc:`esp-llm-service`. It uses Volcengine RTC. The device joins with a Volcengine IoT identity. Synchronize the clock before creating the session. Official documentation: `Volcengine RTC <https://www.volcengine.com/docs/6348>`__. For a hardware conversational agent, see the `quick start <https://www.volcengine.com/docs/6348/1806625>`__.

Features
--------

- Voice chat: join an RTC room with the IoT identity. The microphone is sent uplink and the speaker plays the agent reply. A camera and a display are not required
- Direct speech: ``esp_llm_volc_send_text_to_tts()`` speaks text and does not send that text through the chat model. Use it for local prompts, such as a volume confirmation. Call it after the session has started
- Video: ``ESP_LLM_VOLC_VIDEO_PUBLISH`` publishes the local camera, and ``ESP_LLM_VOLC_VIDEO_SUBSCRIBE`` subscribes to the remote picture in the room. The codec is JPEG or H.264. Both are off by default. Leave them off when there is no camera or display

How It Works
------------

``esp_llm_volc_create()`` reads ``esp_llm_volc_cfg_t``. Start from ``ESP_LLM_VOLC_CFG_DEFAULT()``, then set the IoT identity. The structure is in ``esp_llm_volc.h``. Audio and video codec defaults and the engine JSON template are in ``esp_llm_volc_defaults.h`` and follow the Kconfig choice.

Set ``instance_id``, ``product_key``, ``product_secret``, ``device_name``, and ``bot_id`` on ``cfg.iot``. Starting the session fails when ``bot_id`` is empty. ``cfg.engine.log_level`` defaults to 3 and is written into the generated engine JSON. When ``cfg.engine.config_json_raw`` is set, that JSON replaces the engine configuration the adapter generates. A ``connect_timeout_ms`` of 0 uses 30000 ms.

Select audio in menuconfig under Audio profile. The default is G.711 A-law at 8 kHz. G.711 mu-law at 8 kHz and OPUS at 16 kHz are the other choices. ``ESP_LLM_VOLC_VIDEO_PUBLISH`` enables publishing the local camera, and ``ESP_LLM_VOLC_VIDEO_SUBSCRIBE`` enables subscribing to the remote picture. Both are off by default. Select JPEG or H.264 under Video RTC profile. The default is JPEG.

.. code:: c

    esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
    cfg.iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
    cfg.iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
    cfg.iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
    cfg.iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
    cfg.iot.bot_id = CONFIG_VOLC_BOT_ID;

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));

Link and start order are in :doc:`esp-llm-service`.

Example
-------

`esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__ enables voice chat only. Video stays off.
