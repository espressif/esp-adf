AI 陪伴对讲（喵伴）
======================

:link_to_translation:`en:[English]`

简介
----------

喵伴（ESP-VoCat-S31）是乐鑫发布的桌面 AI 萌宠 Demo，在同一台设备上集成语音交互、视觉感知与蓝牙音频，面向陪伴、对讲与多模态互动类产品。该版本由原 ESP-VoCat 升级而来：主板更换为 ESP32-S31-WROOM-1，并增加 DVP 摄像头，底座、麦克风板、圆形屏、扬声器、电池、触摸按键与外壳装配方式保持兼容。

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["麦克风"]
          Cam["摄像头"]
          Soc["ESP32-S31"]
          Lcd["圆形屏"]
          Spk["扬声器"]
          Bt["蓝牙音频"]
          Mic --> Soc
          Cam --> Soc
          Soc --> Lcd
          Soc --> Spk
          Soc --> Bt

应用场景
----------

- **桌面陪伴**：圆形屏显示表情与画面，触摸按键交互，作为桌面语音助手或萌宠形态的交互终端
- **亲子对讲**：本地或云端语音对话，结合摄像头完成简单的视频互动
- **蓝牙音箱**：利用 ESP32-S31 的经典蓝牙与 BLE Audio，扩展为桌面音箱或广播音频演示
- **端侧视觉**：拍照上传、画面分析、手势识别、颜色识别与物体识别，用于多模态交互演示

功能特性
----------

- 麦克风采集与扬声器播放，可接入云端大模型对话或本地语音流程
- 圆形屏显示表情、预览画面；触摸按键与 IMU、SD 卡、旋转底座通信保留
- DVP 摄像头支持拍照、画面分析；端侧可运行离线手势识别、OpenCV 颜色识别与 YOLO11 物体识别
- 音频编解码芯片 ES8389 负责采集与播放；经典蓝牙音箱之外，可扩展 LE Audio / Auracast
- 硬件 JPEG 编解码与 PPA 图像加速配合更高 PSRAM 带宽，用于 Wi-Fi 6 视频推流与圆形屏高帧率显示

硬件与软件
----------------

主控为 ESP32-S31。音频链路使用 ES8389；视觉链路使用 SC101IOT DVP 摄像头。实时音视频通话可复用 :doc:`esp-webrtc-solution`；语音对话接入可参考 :doc:`../multimedia-services/ai/esp-coze`。硬件设计资料已在立创开源平台发布，软件工程可按公开仓库与示例自行组合上述组件。

参考资料
----------

- 硬件开源：`ESP-VoCat-S31 喵伴 <https://oshwhub.com/esp-college/project_fqjutmff>`__
- 演示视频：`喵伴升级实测 <https://www.bilibili.com/video/BV1azLv6tEuj/>`__
- 芯片资料：`ESP32-S31 技术规格书 <https://documentation.espressif.com/esp32-s31_datasheet_cn.pdf>`__
