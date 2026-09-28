ESP LLM Tongyi
===============

:link_to_translation:`en:[English]`

简介
----------

`esp_llm_tongyi <https://components.espressif.com/components/espressif/esp_llm_tongyi>`__ 是 :doc:`esp-llm-service` 的通义适配器，使用阿里云百炼 DashScope 的 TLS WebSocket。创建会话前需要先同步时间。云端应用、消息字段和相关文档见 `多模态交互协议 <https://help.aliyun.com/zh/model-studio/multimodal-interaction-protocol>`__\ 。

功能清单
----------

- 纯语音：与 DashScope 语音应用双工对话，不创建相机和显示
- 视觉：本地全屏预览相机，并用 ``esp_llm_service_send_frame()`` 发送一张 JPEG。``frame.user_data`` 为提示词时请求回答，为 ``NULL`` 时只更新会话图像。没有连续视频，也没有云端图像下行

技术拆解
----------

``esp_llm_tongyi_create()`` 读取 ``esp_llm_tongyi_cfg_t``\ 。先用 ``ESP_LLM_TONGYI_CFG_DEFAULT()`` 填入默认值，再写入 ``api_key``\ 、\ ``workspace_id`` 和 ``app_id``\ 。结构在 ``esp_llm_tongyi.h``\ 。上行、下行和客户端字段的默认值在 ``esp_tongyi.h``\ 。

纯语音保持 ``cfg.upstream.type`` 为 ``AudioOnly``\ 。视觉把它设为 ``ESP_TONGYI_UPSTREAM_TYPE_AUDIO_VIDEO``\ ，值为 ``AudioAndVideo``\ 。``cfg.upstream.mode`` 默认 ``duplex``\ ，音频格式默认 ``raw-opus``\ 。下行音色 ``cfg.downstream.voice`` 默认 ``longanhuan``\ ，音量默认 80。采样率、下行帧长、下行码率和上行码率在 menuconfig 的 Audio profile 中设置，默认 16000 Hz、60 ms、32 kbps 和 24000 bit/s。``connect_timeout_ms`` 默认 30000 ms。``task_id`` 为空时由适配器生成。对话可以立即开始，也可以在唤醒词之后再打开上行。

.. code:: c

    esp_llm_tongyi_cfg_t cfg = ESP_LLM_TONGYI_CFG_DEFAULT();
    cfg.api_key = CONFIG_TONGYI_API_KEY;
    cfg.workspace_id = CONFIG_TONGYI_WORKSPACE_ID;
    cfg.app_id = CONFIG_TONGYI_APP_ID;

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_tongyi_create(&cfg, &llm));

链接和启动顺序见 :doc:`esp-llm-service`\ 。

应用示例
----------

`esp_llm_tongyi_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__ 在 menuconfig 中选择纯语音或视觉。视觉模式下，``snap`` 发送一张图像，``snap <prompt>`` 就该图像提问。
