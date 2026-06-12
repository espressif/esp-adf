ESP-GMF 通用多媒体框架
======================

:link_to_translation:`en:[English]`

`ESP-GMF <https://github.com/espressif/esp-gmf>`__\ （Espressif General Multimedia Framework）是乐鑫面向 IoT 多媒体应用的轻量级处理框架。框架以处理单元（element）为基本单元，由处理链（pipeline）串接，经数据端口（port）与数据总线（data bus）传递音频、视频与通用数据流。

架构、API 与示例见 `ESP-GMF 文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/>`__。

.. list-table::
   :header-rows: 1
   :widths: 22 12 46 20

   * - 组件
     - 分层
     - 简介
     - 相关链接
   * - ``gmf_core``
     - Core
     - 定义处理链、处理单元、数据端口、数据总线与 FOURCC
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-core/index.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_core>`__
   * - ``gmf_io``
     - Elements
     - 提供文件、HTTP、嵌入式 Flash、I2S PDM 与编解码器设备的外部接口
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-elements/gmf-io.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_io>`__
   * - ``gmf_audio``
     - Elements
     - 提供音频编解码，以及采样率、声道与位深转换，并包含均衡、混音、淡入淡出、自动电平控制与动态范围控制
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-elements/gmf-audio.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_audio>`__
   * - ``gmf_video``
     - Elements
     - 提供视频编解码，以及缩放、旋转、叠加与帧率转换，可使用 PPA 加速
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-elements/gmf-video.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_video>`__
   * - ``gmf_ai_audio``
     - Elements
     - 提供回声消除、噪声抑制、自动增益、语音活动检测，以及唤醒词与命令词识别
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-elements/gmf-ai-audio.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_ai_audio>`__
   * - ``gmf_misc``
     - Elements
     - 将一路输入复制到多路输出，用于分流与并行处理
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-elements/gmf-misc.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_misc>`__
   * - ``gmf_loader``
     - Packages
     - 根据 menuconfig 将处理单元与外部接口注册到框架注册池，并设置默认参数
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/gmf-loader.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_loader>`__
   * - ``gmf_app_utils``
     - Packages
     - 提供编解码器、I2C、SD 卡、Wi-Fi 与命令行等应用初始化与调试接口
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/gmf-app-utils.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_app_utils>`__
   * - ``esp_capture``
     - Packages
     - 按采集源、处理路径与输出端采集音视频，支持编码、叠加、多路输出与本地存储
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-capture.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_capture>`__
   * - ``esp_player``
     - Packages
     - 在同一实例中完成解封装、解码与音视频渲染；输入支持本地文件、HTTP(S)、HLS 与外部帧
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-player.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_player>`__
   * - ``esp_audio_simple_player``
     - Packages
     - 根据 URI 方案与文件扩展名选择外部接口与解码器，用于音频播放
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-audio-simple-player.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_audio_simple_player>`__
   * - ``esp_audio_render``
     - Packages
     - 将多路 PCM 混音后输出；混音前后可连接处理链，并通过写入回调写出
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-audio-render.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_audio_render>`__
   * - ``esp_video_render``
     - Packages
     - 将视频与界面合成到液晶屏、LVGL 或帧缓冲，支持多路输入与脏区域刷新
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-video-render.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_video_render>`__
   * - ``esp_bt_audio``
     - Packages
     - 提供经典蓝牙与低功耗音频接口，覆盖 A2DP、HFP、AVRCP 及可选的广播角色
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-bt-audio.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_bt_audio>`__
   * - ``esp_asrc``
     - Packages
     - 转换采样率、位深与声道；芯片含 ASRC 外设时使用硬件实现，否则使用软件实现
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-asrc.html>`__ · `组件 <https://components.espressif.com/components/espressif/esp_asrc>`__
   * - ``gmf_fft``
     - Packages
     - 提供定点 Q15 实数 FFT 与 IFFT，长度范围为 32 至 8192，包含 PIE 向量优化与 C 实现
     - `文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/gmf-fft.html>`__ · `组件 <https://components.espressif.com/components/espressif/gmf_fft>`__
