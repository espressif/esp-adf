智能音响与闹钟
==================

:link_to_translation:`en:[English]`

简介
----------

智能音响与 AI 闹钟共用一套离在线语音方案：唤醒、对话、多音源播放与日程管理部署在同一套软件上，整机可以做成音箱、床头闹钟、故事机或桌面语音助手。在线链路使用 RTC 或 WebSocket 对接大模型与语音云；早期 ASR + NLU 也可使用 MQTT 或 HTTP。离线模式仅保留唤醒词与本地命令词，用于不依赖网络的家居控制。

单芯片完成前端语音算法、音频编解码、流媒体播放，以及 Mesh、红外、电机、屏幕等外设控制，无需再配独立语音 DSP。需要经典蓝牙时，可选用 ESP32-S31；也可在 ESP32 上外接语音处理芯片。

.. only:: html

   .. mermaid::

      flowchart LR
          Wake["语音唤醒"]
          Chat["云端或本地对话"]
          Player["多音源播放"]
          Alarm["闹钟与日程"]
          Wake --> Chat
          Chat --> Player
          Alarm --> Player

应用场景
----------

- **家庭娱乐**：智能音箱、网络收音机，播放在线电台与本地曲库
- **儿童教育**：故事机、早教终端，结合唤醒与云端内容
- **智能家居**：语音助手、带语音的电器，通过 BLE Mesh 或红外控制灯与家电
- **办公辅助**：床头或桌面 AI 闹钟，提供日程、天气查询与语音提醒

功能特性
----------

- 语音唤醒，以及降噪、回声消除、自动增益与麦克风阵列等前端处理；唤醒词可定制
- 在线多轮对话，或离线命令词控制；在线侧可对接主流语音云与大模型服务
- 播放来自 Wi-Fi（HTTP、HLS、DLNA、AirPlay）、蓝牙和本地存储的音频；支持打断、断电续播与淡入淡出
- 闹钟、日程，以及天气、表盘、表情一类屏显
- 音频编解码与 EQ 调音；红外或 Mesh 家居控制可作为整机扩展

硬件与软件
----------------

语音交互产品常用 ESP32-S3、ESP32-S31、ESP32-P4。需要经典蓝牙时选用 ESP32-S31。带屏产品按屏幕接口与分辨率选择芯片。

语音前端见 `ESP-SR <https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/index.html>`__。播放与格式处理见 :doc:`../basic-components/audio-codec/esp-audio-codec` 与 :doc:`../basic-components/effects/esp-audio-effects`。实时对话可参考 :doc:`esp-webrtc-solution`。工程示例见 `adf_examples/ai_agent <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__。

参考资料
----------

- `ESP-SR <https://github.com/espressif/esp-sr>`__
- `ESP Audio Codec <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `ESP-GMF 处理单元 <https://github.com/espressif/esp-gmf/tree/main/elements>`__
- `AI Agent 示例 <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__
