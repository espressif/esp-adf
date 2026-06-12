可视对讲与门铃
==================

:link_to_translation:`en:[English]`

简介
----------

可视对讲与门铃方案在 ESP32-S3 / ESP32-P4 上完成摄像、编解码、双向通话与屏显，做成猫眼、门锁、门铃或看护设备。设备采集门口或室内画面，经 WebRTC、RTMP、RTSP 或 SIP 传到手机或室内屏，按所选协议提供远程视频预览、视频推流或双向可视对讲。音频侧使用 1 至 3 麦阵列，并做降噪与回声消除。屏幕接口支持 SPI、8080、RGB 等，UI 可基于 LVGL。

整机差异主要在摄像头接口（USB、DVP、MIPI）与屏幕尺寸；门锁、门铃与看护摄像头的软件结构相近。

.. only:: html

   .. mermaid::

      flowchart LR
          Cam["摄像头"]
          Enc["视频编解码"]
          Rtc["WebRTC / RTMP / RTSP / SIP"]
          Lcd["视频预览/双向可视对讲"]
          Cam --> Enc
          Enc --> Rtc
          Rtc --> Lcd

应用场景
----------

- **家庭安防**：智能猫眼、可视门铃、带屏门锁与门禁
- **看护设备**：婴幼儿或老人看护终端，远程视频预览与可视对讲
- **家居设备**：宠物喂食器、室内监控器等设备的远程视频预览与可视对讲
- **互动玩具**：带视频通话的玩具或桌面机器人

功能特性
----------

- 双向视频对讲与语音通话
- WebRTC、RTMP、RTSP、SIP 等传输
- 1 至 3 麦阵列，带降噪和回声消除
- SPI、8080、RGB 等屏接口，用于预览和简单 UI
- Wi-Fi 保活：ESP32-S3 待机电流可到约 600 µA；ESP32-P4 搭配不同 Wi-Fi 芯片时电流不同
- 手机 App 或小程序发起通话

硬件与软件
----------------

按分辨率和编码格式选择主控：H.264 对讲使用 ESP32-P4；MJPEG 在 480×800 及以下可用 ESP32-S3，更高分辨率可用 ESP32-S31。摄像头接口可以是 USB、DVP 或 MIPI。

实时通话见 :doc:`esp-webrtc-solution`。H.264 与 JPEG 见 :doc:`../basic-components/video-codec/esp-h264` 与 :doc:`../basic-components/video-codec/esp-new-jpeg`。语音前端见 `ESP-SR <https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/audio_front_end/README.html>`__。门铃示例见 `doorbell_demo <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__。

参考资料
----------

- :doc:`esp-webrtc-solution`
- `doorbell_demo <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__
- :doc:`../basic-components/video-codec/esp-h264`
- :doc:`../basic-components/video-codec/esp-new-jpeg`
