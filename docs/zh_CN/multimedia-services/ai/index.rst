AI 对接
========

:link_to_translation:`en:[English]`

AI 接入组件包括 Coze。云端语音例程见下表与 :doc:`/multimedia-examples/index`。

.. toctree::
   :maxdepth: 1

   esp-coze

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程
     - 简介
     - 链接
   * - coze_ws_app
     - 通过 WebSocket 接入扣子，提供对话、唤醒与按键打断
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app>`__
   * - voice_assistant_app
     - 基于百度 RTC 的语音助手，提供对话与网络音乐混音
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/baidu_rtc/solutions/voice_assistant_app>`__
   * - openai_demo
     - 通过 WebRTC 连接 OpenAI Realtime，并演示函数调用
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo>`__
