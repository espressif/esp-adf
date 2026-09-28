ESP LLM Volc
=============

:link_to_translation:`en:[English]`

简介
----------

`esp_llm_volc <https://components.espressif.com/components/espressif/esp_llm_volc>`__ 是 :doc:`esp-llm-service` 的火山适配器，使用火山引擎 RTC。设备以火山 IoT 身份接入。创建会话前需要先同步时间。官方文档：`火山引擎 RTC <https://www.volcengine.com/docs/6348>`__\ 。硬件对话智能体见 `快速入门 <https://www.volcengine.com/docs/6348/1806625>`__\ 。

功能清单
----------

- 语音对话：以 IoT 身份加入 RTC 房间，麦克风上行，扬声器播放智能体回复。不依赖相机和显示
- 直接朗读：``esp_llm_volc_send_text_to_tts()`` 朗读文本，不把该文本送入对话模型。用于本地提示，例如音量确认。会话启动后调用
- 视频：``ESP_LLM_VOLC_VIDEO_PUBLISH`` 发布本机相机，``ESP_LLM_VOLC_VIDEO_SUBSCRIBE`` 订阅房间内远端画面，编码为 JPEG 或 H.264。二者默认关闭。没有相机或显示时保持关闭

技术拆解
----------

``esp_llm_volc_create()`` 读取 ``esp_llm_volc_cfg_t``\ 。先用 ``ESP_LLM_VOLC_CFG_DEFAULT()`` 填入默认值，再写入 IoT 身份。结构在 ``esp_llm_volc.h``\ 。音频、视频编码和引擎 JSON 模板在 ``esp_llm_volc_defaults.h``\ ，由 Kconfig 选择。

``cfg.iot`` 的 ``instance_id``\ 、\ ``product_key``\ 、\ ``product_secret``\ 、\ ``device_name`` 和 ``bot_id`` 都需要设置。``bot_id`` 为空时，启动会话失败。``cfg.engine.log_level`` 默认 3，写入生成的引擎 JSON。设置 ``cfg.engine.config_json_raw`` 时，用这份 JSON 替换适配器生成的引擎配置。``connect_timeout_ms`` 为 0 时使用 30000 ms。

音频在 menuconfig 的 Audio profile 中选择，默认 G.711 A-law 8 kHz，也可选择 G.711 mu-law 8 kHz 或 OPUS 16 kHz。``ESP_LLM_VOLC_VIDEO_PUBLISH`` 打开发布本机相机，``ESP_LLM_VOLC_VIDEO_SUBSCRIBE`` 打开订阅远端画面，二者默认关闭。视频编码在 Video RTC profile 中选择 JPEG 或 H.264，默认 JPEG。

.. code:: c

    esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
    cfg.iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
    cfg.iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
    cfg.iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
    cfg.iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
    cfg.iot.bot_id = CONFIG_VOLC_BOT_ID;

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));

链接和启动顺序见 :doc:`esp-llm-service`\ 。

应用示例
----------

`esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__ 只启用语音对话，视频保持关闭。
