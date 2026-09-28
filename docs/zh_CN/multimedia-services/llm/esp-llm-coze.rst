ESP LLM Coze
=============

:link_to_translation:`en:[English]`

简介
----------

`esp_llm_coze <https://components.espressif.com/components/espressif/esp_llm_coze>`__ 是 :doc:`esp-llm-service` 的扣子适配器，通过 :doc:`/multimedia-services/ai/esp-coze` 使用扣子 WebSocket。官方文档：`Coze API 概览 <https://www.coze.cn/docs/developer_guides/coze_api_overview>`__\ 。

功能清单
----------

- 音频与文本对话。不接受视频流
- 直接对话，或启用唤醒词后再上行
- 唤醒时可以打断当前回答
- 音频格式：OPUS 或 PCM 16 kHz，以及 G.711 A-law / mu-law 8 kHz

技术拆解
----------

``esp_llm_coze_create()`` 读取 ``esp_llm_coze_cfg_t``\ 。先用 ``ESP_LLM_COZE_CFG_DEFAULT()`` 填入默认值，再写入 ``bot_id`` 和 ``access_token``\ 。结构在 ``esp_llm_coze.h``\ 。音频编码默认值在 ``esp_llm_coze_defaults.h``\ ，由 Kconfig 选择。``access_token`` 来自个人访问令牌或 OAuth 换票，两种结果写入同一字段。字段说明见 `扣子鉴权文档 <https://www.coze.cn/open/docs/developer_guides/authentication>`__\ 。

``ws_base_url`` 为空时使用 ``wss://ws.coze.cn/v1/chat``\ 。``conv_create_base_url`` 为空时使用 ``https://api.coze.cn/v1/conversation/create``\ 。``user_id`` 为空时使用 ``esp_llm_user``\ 。``voice_id`` 为空时使用 ``7426720361733144585``\ 。``mode`` 默认 ``ESP_LLM_COZE_MODE_SPEECH_INTERRUPT``\ 。另一取值 ``ESP_LLM_COZE_MODE_NORMAL`` 由按键触发对话。``vad_mode`` 在 menuconfig 的 Server VAD silence mode 中选择，默认快速静音检测。

音频在 Audio profile 中选择，默认 OPUS 16 kHz。OPUS 下行码率默认 16000 bit/s，下行帧长默认 60 ms。``downlink_speech_rate`` 默认 20，范围 -50 到 100。``websocket_connect_timeout`` 默认 30000 ms。

个人访问令牌
^^^^^^^^^^^^^^^^^^

PAT（Personal Access Token，个人访问令牌）在扣子开放平台申请后，直接写入 ``access_token``\ 。

OAuth
^^^^^^^^^^^^^^^^^^

应用用 OAuth 私钥签发 JWT，再向扣子换取短期访问令牌。组件不执行换票。

1. ``esp_coze_jwt_create()`` 使用 RS256 签名。``kid`` 是公钥 ID，``payload`` 是 JWT 声明 JSON，``private_key`` 是 PEM 编码的 RSA 私钥，至少 2048 位。返回新分配的 JWT，调用方 ``free()``\ 。``iss``\ 、\ ``aud``\ 、\ ``iat``\ 、\ ``exp``\ 、\ ``jti`` 按扣子 OAuth 文档填写。``iat`` 和 ``exp`` 使用设备时钟，创建前需要同步时间。
2. ``esp_coze_http_post()`` 把 JWT 放在 ``Authorization: Bearer`` 中，POST 到 ``https://api.coze.cn/api/permission/oauth2/token``\ 。从响应 JSON 的 ``access_token`` 取出令牌，再写入 ``cfg.access_token``\ 。响应体由调用方 ``free()``\ 。

.. code:: c

    esp_llm_coze_cfg_t cfg = ESP_LLM_COZE_CFG_DEFAULT();
    cfg.bot_id = CONFIG_COZE_BOT_ID;
    cfg.access_token = CONFIG_COZE_ACCESS_TOKEN;  /* PAT，或 OAuth 换取的访问令牌 */

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_coze_create(&cfg, &llm));

启用唤醒词后，唤醒前上行保持关闭。链接和启动顺序见 :doc:`esp-llm-service`\ 。

应用示例
----------

`esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__ 使用个人访问令牌。
