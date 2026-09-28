ESP LLM Service
===============

:link_to_translation:`en:[English]`

简介
----------

`esp_llm_service <https://components.espressif.com/components/espressif/esp_llm_service>`__ 是统一的 AI 会话服务，以 :doc:`/multimedia-services/service-infra/esp-service` 为基类，媒体流使用 :doc:`/multimedia-services/media/esp-media-service` 的链接方式。应用创建采集和播放，再把流链接到 LLM（Large Language Model，大语言模型）。上行帧交给厂商适配器，下行帧交给应用持有的播放端。LLM 不创建、也不销毁采集和播放。

如何创建适配器、链接采集和播放，见 `esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__\ 、\ `esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__\ 、\ `esp_llm_tongyi_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__ 和 `esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__\ 。

功能清单
----------

- 四条媒体流：``ESP_LLM_MEDIA_STREAM_AUDIO_UP`` 送麦克风，``ESP_LLM_MEDIA_STREAM_VIDEO_UP`` 送摄像头，``ESP_LLM_MEDIA_STREAM_AUDIO_DN`` 送扬声器，``ESP_LLM_MEDIA_STREAM_VIDEO_DN`` 送显示。创建前在 ``streams[]`` 中启用需要的流，其余保持关闭
- 先链接媒体，再按播放端、LLM、采集的顺序调用 ``esp_service_start()``\ 。停止时顺序相反。先 ``esp_service_stop()``\ ，再 ``esp_llm_service_destroy()``
- ``esp_llm_service_send_text()`` 在会话启动后发送文本
- ``esp_llm_service_send_frame()`` 发送一帧。采集链接已经在送的帧不要再用它发送
- ``esp_llm_service_send_custom_data()`` 发送厂商原生命令
- ``esp_llm_service_cancel_response()`` 取消当前回答，会话保持
- ``esp_llm_service_enable_uplink_track()`` 关闭音频或视频上行。对应帧被丢弃，采集继续运行
- 可选 Audio AI：把应用的采集填入 ``audio_ai_cfg.capture``\ ，用 ``feature_mask`` 启用唤醒。按下调用 ``esp_llm_service_trigger_wakeup()``\ ，按住调用 ``keep_awake()``\ ，松开调用 ``trigger_sleep()``

适配器
----------

.. list-table::
   :header-rows: 1
   :widths: 16 16 68

   * - 适配器
     - 协议
     - 能力
   * - :doc:`esp_llm_baidu <esp-llm-baidu>`
     - 百度 RTC
     - 语音对话，对话中可播放智能体下发的音乐。图像理解把摄像头画面发给云端，图像生成显示云端图像，多模态两者都做。房间对讲与智能体场景分开
   * - :doc:`esp_llm_coze <esp-llm-coze>`
     - 扣子 WebSocket
     - 音频和文本对话。可以直接对话，也可以唤醒后再上行，唤醒时能打断当前回答。不接收摄像头画面，也不显示云端图像
   * - :doc:`esp_llm_tongyi <esp-llm-tongyi>`
     - 通义 WebSocket
     - 纯语音双工对话。视觉时本地预览相机，用 ``esp_llm_service_send_frame()`` 发送一张 JPEG，``frame.user_data`` 中的提示词请求回答。没有连续视频，也不显示云端图像
   * - :doc:`esp_llm_volc <esp-llm-volc>`
     - 火山 RTC
     - 语音对话。``esp_llm_volc_send_text_to_tts()`` 直接朗读文本，不把该文本送入对话模型。发布本机相机和订阅远端画面默认关闭

技术拆解
----------

媒体流与链接
^^^^^^^^^^^^

用适配器的 create 函数创建会话。麦克风、扬声器和摄像头由应用创建、启动和销毁，服务不创建它们。先链接，再按播放端、LLM、采集的顺序启动。停止时顺序相反。

.. code:: c

    esp_media_service_link(capture, APP_AUDIO_CAPTURE_STREAM,
                           llm, ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    esp_media_service_link(llm, ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                           player, APP_AUDIO_PLAYER_STREAM);

    esp_service_start(player);
    esp_service_start(llm);
    esp_service_start(capture);

``APP_AUDIO_CAPTURE_STREAM`` 和 ``APP_AUDIO_PLAYER_STREAM`` 是应用自己的 stream 编号。链接之后，服务从采集读取上行帧并交给适配器，再把下行帧写到播放端。应用不用再送这一路音频。未使用的流不要在 ``streams[]`` 中启用。``esp_llm_service_send_frame()`` 只用于没有采集链接的单帧。已经由采集送出的帧不要再调用它。

控制面
^^^^^^^^^^^^^^^^^^

``send_text()``\ 、\ ``send_frame()``\ 、\ ``send_custom_data()`` 和 ``cancel_response()`` 在服务启动后调用，且不能重叠。``cancel_response()`` 只停止当前回答，会话保持。``enable_uplink_track()`` 可以从事件回调调用：被关闭的那一类帧被丢弃，函数仍返回成功，本地采集和预览继续运行。

Audio AI
^^^^^^^^^^^^^^^^

把应用已创建的音频采集填入 ``audio_ai_cfg.capture``\ ，用 ``feature_mask`` 启用唤醒等能力。唤醒前可以关闭上行，唤醒时可以取消当前回答。按键流程为按下时 ``trigger_wakeup()``\ ，按住期间 ``keep_awake()``\ ，松开时 ``trigger_sleep()``\ 。

应用示例
----------

- `esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__ 在 menuconfig 中选择场景，并把采集、播放链接到百度适配器。语音对话包含音乐 URL 播放
- `esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__ 使用个人访问令牌接入扣子，演示直接对话、唤醒和打断
- `esp_llm_tongyi_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__ 在 menuconfig 中选择纯语音或视觉。视觉模式下 ``snap`` 发送一张图像，``snap <prompt>`` 就该图像提问
- `esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__ 只启用语音对话，视频保持关闭
