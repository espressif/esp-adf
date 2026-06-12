语音律动灯带
================

:link_to_translation:`en:[English]`

简介
----------

语音律动灯带采集环境声音，经降噪、增益与 FFT 频谱分析后，将音量与频率映射到灯珠的颜色、亮度和点亮数量。灯带可作为独立氛围灯，也可作为音箱上的灯环。Wi-Fi 用于配网与切换灯效。片上 ADC 接模拟麦克风即可驱动灯效；需要更高采样精度时可外接 ADC。

除 ESP8266 外，ESP32 系列均可运行，CPU 占用低。独立灯带常用 ESP32-C3；作为音箱附件时随音箱主控选用 ESP32 或 ESP32-S3。

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["麦克风"]
          Fft["降噪与频谱分析"]
          Led["灯珠颜色与亮度"]
          Mic --> Fft
          Fft --> Led

应用场景
----------

- **家居氛围照明**：客厅、卧室、娱乐区的墙面或床边辅助照明
- **商业娱乐**：需要声光电联动的酒吧、KTV、游戏场所
- **屏幕补光**：电视或显示器背面的氛围灯带，按节目声音变化
- **电器灯环**：音箱等产品上的呼吸灯或灯环

功能特性
----------

- 片上 ADC 接模拟麦克风拾音，默认采样率 16 kHz，有效位深约 12 bit
- 对拾音结果做降噪、滤波放大与 FFT 频谱分析
- 将频谱映射到灯珠颜色、亮度与点亮数量，灯珠数量从数颗到数百颗
- 多种灯效模式与参数设置
- 可与音箱整机共用主控，仅作为背景灯环

硬件与软件
----------------

独立灯带推荐 ESP32-C3：律动算法与 Wi-Fi 在同一颗芯片上，外围简单。音箱产品以 ESP32 或 ESP32-S3 为主控，灯带只占用少量 CPU。示例见 `adf_examples/display/led_pixels <https://github.com/espressif/esp-adf/tree/master/adf_examples/display/led_pixels>`__。

参考资料
----------

- `led_pixels 示例 <https://github.com/espressif/esp-adf/tree/master/adf_examples/display/led_pixels>`__
