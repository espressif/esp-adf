ESP Coze
=========

:link_to_translation:`en:[English]`

简介
----------

`ESP Coze <https://components.espressif.com/components/espressif/esp_coze>`__ 是基于 Coze 平台、面向 ESP-IDF 的开源组件，通过 WebSocket 与 Coze 服务通信，提供流式对话、文本转语音与语音识别三条独立入口，适合 ESP32 类设备的语音助手与智能语音问答。

功能清单
----------

- 流式对话（Chat）：全双工语音 + 文本交互，支持字幕、自定义参数、会话 ID 管理
- 文本转语音（TTS）：提交文本，通过回调接收解码后的音频
- 语音识别（ASR）：持续上行音频，支持服务端 VAD 或应用驱动两种断句方式
- 三条入口彼此独立，各自维护一个 WebSocket 连接，可同时并行使用
- 统一音频编码类型（OPUS、G.711A、G.711U、PCM），三个模块共用同一套编码常量
- PAT 或 OAuth/JWT 两种鉴权方式，提供 JWT 签名与 HTTP POST 辅助接口
- Kconfig 可调 WebSocket 任务优先级、绑核、buffer 大小与各类超时时间

技术拆解
----------

Chat（流式对话）
^^^^^^^^^^^^^^^^^^^^^^

``esp_coze_chat_init`` 用 ``esp_coze_chat_config_t`` 创建一个会话句柄，此时并未建立连接；调用 ``esp_coze_chat_start`` 才会连接 WebSocket、等待连接建立，并推送初始的 ``chat.update``\ 。会话结束时依次调用 ``esp_coze_chat_stop`` 和 ``esp_coze_chat_deinit``\ 。

.. code:: c

    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id          = COZE_BOT_ID;
    cfg.access_token    = COZE_ACCESS_TOKEN;
    cfg.enable_subtitle = true;
    cfg.audio_callback  = on_downlink_audio;
    cfg.event_callback  = on_chat_event;

    esp_coze_chat_handle_t chat = NULL;
    esp_coze_chat_init(&cfg, &chat);
    esp_coze_chat_start(chat);
    /* 用 esp_coze_chat_send_audio_data() 持续送入麦克风编码帧 */
    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);

上行音频通过 ``esp_coze_chat_send_audio_data`` 发送，内部按 base64 编码后以 ``input_audio_buffer.append`` 事件转发；``esp_coze_chat_send_audio_complete`` 在打断模式下标记一段语音结束，``esp_coze_chat_audio_data_clearup`` 清空服务端已缓存的音频。下行内容通过两类回调获取：\ ``audio_callback`` 只接收解码后的音频字节，其余通知（字幕、错误、原始事件等）都经 ``esp_coze_chat_event_callback_t`` 回调按 ``esp_coze_chat_event_t`` 分发；若不设置 ``audio_callback``\ ，下行音频会改为通过事件回调的 ``ESP_COZE_CHAT_EVENT_AUDIO_DATA`` 传出。

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - 事件
     - event_data
   * - ``ESP_COZE_CHAT_EVENT_CHAT_COMPLETED``
     - 无（一轮对话结束）
   * - ``ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED`` / ``_STOPED``
     - 无（服务端 VAD 判定语音起止）
   * - ``ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT``
     - 字幕文本（需 ``enable_subtitle = true``\ ）
   * - ``ESP_COZE_CHAT_EVENT_CHAT_ERROR``
     - 错误 JSON 文本
   * - ``ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA``
     - 未识别 ``event_type`` 的原始 JSON，供应用自行解析

会话进行中如需调整语音音色、自定义参数或会话 ID，需在调用 ``esp_coze_set_chat_config_voice_id``\ 、\ ``esp_coze_set_chat_config_parameters`` 或 ``esp_coze_set_chat_config_conversation_id`` 之后，再调用 ``esp_coze_chat_update_chat`` 把新配置推送到服务端生效。

TTS（文本转语音）
^^^^^^^^^^^^^^^^^^^^^^

TTS 是只做语音合成的精简入口，接口形态与 Chat 类似，但配置更少：``esp_coze_tts_init`` 创建句柄，``esp_coze_tts_start`` 建立连接并推送初始 ``chat.update``\ ，之后每调用一次 ``esp_coze_tts_send_text`` 即提交一段待合成文本，解码后的音频通过 ``audio_callback`` 持续回调，一轮合成完成会收到 ``ESP_COZE_TTS_EVENT_CHAT_COMPLETED``\ 。

.. code:: c

    esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
    cfg.bot_id         = COZE_BOT_ID;
    cfg.access_token   = COZE_ACCESS_TOKEN;
    cfg.audio_callback = on_audio;

    esp_coze_tts_handle_t tts = NULL;
    esp_coze_tts_init(&cfg, &tts);
    esp_coze_tts_start(tts);
    esp_coze_tts_send_text(tts, "你好");
    /* 等待若干 ESP_COZE_TTS_EVENT_CHAT_COMPLETED 后 */
    esp_coze_tts_stop(tts);
    esp_coze_tts_deinit(tts);

ASR（语音识别）
^^^^^^^^^^^^^^^^^^^^^^

ASR 只做识别，通过 ``esp_coze_asr_send_audio`` 持续上行编码音频帧，识别结果以 ``esp_coze_asr_event_t`` 事件返回：\ ``ESP_COZE_ASR_EVENT_TRANSCRIPT_UPDATE`` 携带中间转写文本，\ ``ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED`` 携带最终结果。断句方式由 ``esp_coze_asr_turn_detection_t`` 决定：\ ``ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD``\ （默认）由服务端按 ``vad_prefix_padding_ms`` / ``vad_silence_duration_ms`` 判定语音起止并推送 ``SPEECH_STARTED`` / ``SPEECH_STOPPED``\ ；\ ``ESP_COZE_ASR_TURN_DETECTION_NONE`` 则需要应用在每段语音结束后主动调用 ``esp_coze_asr_send_audio_complete``\ 。

.. code:: c

    esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
    cfg.bot_id         = COZE_BOT_ID;
    cfg.access_token   = COZE_ACCESS_TOKEN;
    cfg.event_callback = on_asr;

    esp_coze_asr_handle_t asr = NULL;
    esp_coze_asr_init(&cfg, &asr);
    esp_coze_asr_start(asr);
    /* 持续调用 esp_coze_asr_send_audio(asr, frame, frame_len) */
    esp_coze_asr_send_audio_complete(asr);
    esp_coze_asr_stop(asr);
    esp_coze_asr_deinit(asr);

鉴权
----------

Chat、TTS、ASR 三个配置结构体都要求提供 ``access_token``\ ，即 Coze 的 PAT（Personal Access Token）或 OAuth/JWT 生成的 bearer token，详见 `Coze 鉴权文档 <https://www.coze.cn/open/docs/developer_guides/authentication>`__\ 。若使用 OAuth/JWT 方式，组件提供两个辅助接口：\ ``esp_coze_jwt_create`` 用 RS256 对 JWT 头部与载荷签名并返回可直接使用的 token 字符串；``esp_coze_http_post`` 发起同步 HTTP POST，用于向 Coze 的 OAuth 接口换取最终的 access token。PAT、Bot ID 等业务凭据不属于组件配置范围，建议放在应用侧 Kconfig 中管理，可参考 ``coze_ws_app`` 示例的做法。

应用示例
----------

- `coze_ws_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app>`__ 演示按键触发和唤醒词两种交互模式下的完整 Chat 接入流程，并给出 PAT 与 OAuth/JWT 两种鉴权方式的配置方法

FAQ
------

**Q1：Chat、TTS、ASR 能否同时运行？**

可以。三者各自使用独立的 WebSocket 客户端实例，互不影响；并行开启多路时需注意内存占用和任务优先级。

**Q2：PAT 和 OAuth/JWT 该怎么选？**

PAT 配置简单，适合开发调试和个人项目，直接作为 ``access_token`` 使用；OAuth/JWT 需要额外的私钥签名与换取 token 流程（\ ``esp_coze_jwt_create`` + ``esp_coze_http_post``\ ），适合生产环境的批量设备鉴权。

**Q3：回调里可以做耗时处理吗？**

不可以。所有回调都运行在 WebSocket 客户端任务上下文中，应尽快返回，耗时处理应将数据拷贝或投递到队列后交给其他任务完成。

API 参考
----------

公开接口见组件头文件：

- `esp_coze_chat.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_chat.h>`__
- `esp_coze_tts.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_tts.h>`__
- `esp_coze_asr.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_asr.h>`__
- `esp_coze_common.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_common.h>`__
- `esp_coze_jwt.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_jwt.h>`__
- `esp_coze_http.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_http.h>`__
