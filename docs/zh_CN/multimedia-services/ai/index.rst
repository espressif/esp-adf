AI 对接
========

:link_to_translation:`en:[English]`

AI 接入组件包括 ``esp_llm_service`` 与 Coze。云端语音例程见下表与 :doc:`/multimedia-examples/index`。

.. toctree::
   :maxdepth: 1

   esp-coze

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程
     - 简介
     - 链接
   * - esp_llm_baidu_agent_app
     - 百度 RTC 智能体，基于 ``esp_llm_baidu``
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__
   * - esp_llm_coze_agent_app
     - 扣子 WebSocket 语音对话，基于 ``esp_llm_coze``
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__
   * - esp_llm_tongyi_agent_app
     - 通义多模态对话，基于 ``esp_llm_tongyi``
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__
   * - esp_llm_volc_agent_app
     - 火山 RTC 语音对话，基于 ``esp_llm_volc``
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__
   * - openai_demo
     - 通过 WebRTC 连接 OpenAI Realtime，并演示函数调用
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo>`__
