绘本点读与电子书
====================

:link_to_translation:`en:[English]`

简介
----------

绘本点读与电子书方案在故事机、播放器之上增加摄像头识别：扫描书页上的点阵码或画面后播放对应音频，并支持翻译、跟读与讲解。整机形态包括点读笔、绘本机、词典笔与有声电子书。单芯片完成音频编解码、前端语音处理、摄像头驱动与图像压缩，并提供 2.4 GHz Wi-Fi 与 Bluetooth 5 (LE)。

识别结果可驱动本地资源播放，也可请求云端绘本库与大模型问答，实现点读与讲解连续进行。

.. only:: html

   .. mermaid::

      flowchart LR
          Cam["摄像头"]
          Oid["OID 或图像识别"]
          Player["本地或云端播放"]
          Chat["语音问答"]
          Cam --> Oid
          Oid --> Player
          Chat --> Player

应用场景
----------

- **儿童早教**：点读笔、绘本机、早教故事机，对书页点读、翻读或指读
- **K12 学习**：词典笔、单词机、翻译学具，扫描单词或句子后查词与跟读
- **互动阅读**：有声电子书、互动绘本播放器，结合屏显显示课文或插画

功能特性
----------

- 离线 OID 点阵识别，或云端绘本画面识别
- 点读、翻读、指读，以及本地绘本资源播放
- 在线问答、百科查询与大模型学习辅导
- 音箱级播放控制：打断、断电续播、淡入淡出
- 可选屏显，用于课文、翻译或表情

硬件与软件
----------------

常用 ESP32-S3、ESP32-S31、ESP32-P4，因为产品同时需要摄像头与语音交互。点读笔可选用 SPI 摄像头；词典笔常用 DVP 摄像头。需要经典蓝牙时选用 ESP32-S31。

播放与格式处理见 :doc:`../basic-components/audio-codec/esp-audio-codec`。DVP 摄像头驱动见 `esp-video-components <https://github.com/espressif/esp-video-components>`__。语音对话示例见 `adf_examples/ai_agent <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__。

参考资料
----------

- `ESP Audio Codec <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `ESP-GMF 处理单元 <https://github.com/espressif/esp-gmf/tree/main/elements>`__
- `esp-video-components <https://github.com/espressif/esp-video-components>`__
- `AI Agent 示例 <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__
