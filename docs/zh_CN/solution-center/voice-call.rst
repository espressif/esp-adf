语音对讲与网络电话
======================

:link_to_translation:`en:[English]`

简介
----------

语音对讲与网络电话方案在 ESP32 / ESP32-S3 上完成拾音、3A 处理与通话协议，无需外挂独立 DSP。链路可以是局域网或互联网上的 SIP/VoIP，也可以对接运营商号码完成真实来去电。支持单人全双工通话与多人群组会议；增加摄像头后可扩展为视频对话。

近场对讲对拾音距离要求不高，单麦克风外加一颗音频编解码芯片即可。会议或远场场景可选用 1 至 3 麦阵列。

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["麦克风"]
          Afe["降噪与回声消除"]
          Sip["SIP 或 VoIP"]
          Spk["扬声器"]
          Mic --> Afe
          Afe --> Sip
          Sip --> Spk

应用场景
----------

- **智能终端**：音箱、家电或语音门锁内的免提通话
- **看护与求助**：一键求助终端、看护报警器，接通看护端或指定号码
- **会议与作业**：会议话机；骑行、配送等需要免提通话的终端
- **儿童看护**：故事机或看护设备上的语音通话，可再叠加视频

功能特性
----------

- SIP/VoIP 网络通话，以及对接运营商的真实号码拨打与接听
- 全双工通话，带降噪（NS）与回声消除（AEC）
- 单人通话或多人会议
- 近场单麦克风，或 1 至 3 麦阵列
- 可叠加语音助手或简单屏显（HMI）

硬件与软件
----------------

常用 ESP32-S3；需要经典蓝牙时选用 ESP32-S31。语音前端见 `ESP-SR <https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/audio_front_end/README.html>`__。SIP 等协议见 :doc:`../basic-components/media-protocol/esp-media-protocols`。示例见 `adf_examples/protocols/voip <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/voip>`__。

参考资料
----------

- `ESP-SR 声学前端 <https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/audio_front_end/README.html>`__
- :doc:`../basic-components/media-protocol/esp-media-protocols`
- `VoIP 示例 <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/voip>`__
