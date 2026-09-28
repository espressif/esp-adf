ESP LLM Baidu
==============

:link_to_translation:`en:[English]`

简介
----------

`esp_llm_baidu <https://components.espressif.com/components/espressif/esp_llm_baidu>`__ 是 :doc:`esp-llm-service` 的百度适配器，使用百度 RTC（BRTC）。创建会话前需要先同步时间。官方文档：`百度 RTC <https://cloud.baidu.com/doc/RTC/index.html>`__\ 。

功能清单
----------

- 语音对话：``ESP_LLM_BAIDU_SCENE_VOICE_CHAT``\ ，只使用音频，不创建摄像头和显示。对话中智能体可以下发音乐 HTTP URL，应用在独立播放流上播放，并与语音混音
- 图像理解：``ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING``\ ，把摄像头画面发给云端。云端请求时抓拍一张，也可以连续发送
- 图像生成：``ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION``\ ，接收并显示云端生成的图像
- 多模态：``ESP_LLM_BAIDU_SCENE_MULTI_MODAL``\ ，同时发送摄像头画面并显示云端生成的图像
- 房间对讲：``ESP_LLM_BAIDU_MODE_INTERCOM``\ ，与智能体场景分开

技术拆解
----------

``esp_llm_baidu_create()`` 读取 ``esp_llm_baidu_cfg_t``\ 。先用 ``ESP_LLM_BAIDU_CFG_AGENT_DEFAULT()`` 填入默认值，再写入账号和场景。结构在 ``esp_llm_baidu.h``\ 。平台字符串默认值在 ``esp_llm_baidu_config.h``\ ，包括 ``workflow``\ 、\ ``llm``\ 、语言和引擎 JSON。音频、视频编码默认值在 ``esp_llm_baidu_defaults.h``\ ，由 Kconfig 选择。

智能体模式填写 ``cfg.agent.account``\ 。``app_id``\ 、\ ``user_id``\ 、\ ``server_url`` 和 ``instance_id`` 需要设置。``workflow`` 默认 ``VoiceChat``\ ，\ ``llm`` 默认 ``ernie-4.0``\ ，\ ``lang`` 默认 ``zh``\ 。``license_key`` 可以为空字符串。场景写入 ``cfg.agent.scene``\ 。图像理解还要在 ``cfg.service.streams[]`` 中打开摄像头项，图像生成打开显示项，多模态两者都打开。摄像头画面默认抓拍一张。连续发送时把 ``cfg.vision.vision_mode`` 设为 ``ESP_LLM_BAIDU_VISION_MODE_STREAM``\ 。

音频在 menuconfig 的 Agent audio profile 中选择，默认 OPUS 16 kHz。选择 PCM 16 kHz 时，应用收发 PCM，百度库在线上使用 G.722。视频在 Video profile 中选择 JPEG 或 H.264，默认 JPEG。语音打断由 ``ESP_LLM_BAIDU_VOICE_INTERRUPT_ENABLE`` 控制，默认开启。设置 ``cfg.agent.engine.config_json_raw`` 时，用这份 JSON 替换适配器生成的引擎配置。房间对讲改用 ``ESP_LLM_BAIDU_CFG_INTERCOM_DEFAULT()``\ ，填写 ``cfg.intercom`` 的 ``app_id``\ 、\ ``server_url``\ 、\ ``room_name`` 和证书路径。

.. code:: c

    esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
    cfg.agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
    cfg.agent.account.app_id = CONFIG_BAIDU_APP_ID;
    cfg.agent.account.user_id = CONFIG_BAIDU_USER_ID;
    cfg.agent.account.server_url = CONFIG_BAIDU_SERVER_URL;
    cfg.agent.account.instance_id = CONFIG_BAIDU_INSTANCE_ID;

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_baidu_create(&cfg, &llm));

图像生成、图像理解和多模态分别使用 ``ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION_CFG_DEFAULT()``\ 、\ ``ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING_CFG_DEFAULT()`` 和 ``ESP_LLM_BAIDU_SCENE_MULTI_MODAL_CFG_DEFAULT()``\ 。链接和启动顺序见 :doc:`esp-llm-service`\ 。

应用示例
----------

`esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__ 在 menuconfig 中选择场景。语音对话包含音乐 URL 播放。
