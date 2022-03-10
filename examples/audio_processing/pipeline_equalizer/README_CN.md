# 使用均衡器处理 WAV 播放示例

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了使用 ADF 的均衡器 (equalizer) 处理 WAV 音频文件播放的过程，其处理过程的管道数据流如下：

```
sdcard ---> fatfs_stream ---> wav_decoder ---> equalizer ---> i2s_stream ---> codec_chip
```

首先 fatfs_stream 读取位于 microSD 卡中的名为 `test.wav` 音频文件（此文件需用户自备，并预先放置于 microSD 卡中)，然后 WAV 文件数据经过 wav_decoder 解码器解码，解码后的数据再经过均衡器处理，最后数据通过 I2S 发送到 codec 芯片播放出来。


## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，并自备一首 WAV 格式的音频文件，命名为 `test.wav`，然后把 microSD 卡插入开发板备用。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你需要修改录音文件名，并且文件名超过 8 个字符的，那么本例程建议同时打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程


### 功能和用法

例程开始运行后，读取 microSD 卡中的 `test.wav` 文件，并自动使用均衡器处理，然后播放处理后的音频，完整打印详见[日志输出](#日志输出)。

如果要更改均衡器的参数，请编辑 `equalizer_example.c` 中的 `set_gain[]` 表。均衡器的中心频率为 31 Hz、62 Hz、125 Hz、250 Hz、500 Hz、1 kHz、2 kHz、4 kHz、8 kHz 和 16 kHz。

- 本例程测试的 `test.wav` 是采样率为 44100 Hz、16 位、单通道的音频文件。
- 本例程位于 `document/` 目录下的 `spectrum_before.png` 图片是音频文件 `test.wav` 的原始频谱图像。
  <div  align="center"><img src="document/spectrum_before.png" width="700" alt ="spectrum_before" align=center /></div>
- 本例程位于 `document/` 目录下的 `spectrum_after.png` 图片是音频文件 `test.wav` 经过均衡器处理后的频谱图，均衡器增益为 -13 dB。
  <div align="center"><img src="document/spectrum_after.png" width="700" alt ="spectrum_after" align=center /></div>
- 本例程位于 `document/` 目录下的 `amplitude_frequency.png` 图片是当均衡器的增益为 0 dB 时的频率响应图。
  <div align="center"><img src="document/amplitude_frequency.png" width="700" alt ="amplitude_frequency" align=center /></div>


### 日志输出
以下为本例程的完整日志。

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 10:23:02
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00100000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0fdf0 ( 65008) map
I (135) esp_image: segment 1: paddr=0x0001fe18 vaddr=0x3ffb0000 size=0x00200 (   512) load
I (135) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2e0a0 (188576) map
0x400d0020: _stext at ??:?

I (213) esp_image: segment 3: paddr=0x0004e0c8 vaddr=0x3ffb0200 size=0x02050 (  8272) load
I (217) esp_image: segment 4: paddr=0x00050120 vaddr=0x40080000 size=0x0daf0 ( 56048) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (252) boot: Loaded app from partition at offset 0x10000
I (252) boot: Disabling RNG early entropy source...
I (252) cpu_start: Pro cpu up.
I (256) cpu_start: Application information:
I (261) cpu_start: Project name:     equalizer_app
I (266) cpu_start: App version:      v2.2-202-g020b5c49-dirty
I (273) cpu_start: Compile time:     Sep 30 2021 10:22:56
I (279) cpu_start: ELF file SHA256:  2f6692ee6bcc59a3...
I (285) cpu_start: ESP-IDF:          v4.2.2
I (290) cpu_start: Starting app cpu, entry point is 0x40081988
0x40081988: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (300) heap_init: Initializing. RAM available for dynamic allocation:
I (307) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (313) heap_init: At 3FFB2B98 len 0002D468 (181 KiB): DRAM
I (319) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (326) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (332) heap_init: At 4008DAF0 len 00012510 (73 KiB): IRAM
I (338) cpu_start: Pro cpu start user code
I (356) spi_flash: detected chip: gd
I (357) spi_flash: flash io: dio
W (357) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (367) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (378) EQUALIZER_EXAMPLE: [1.0] Mount sdcard
I (888) EQUALIZER_EXAMPLE: [2.0] Start codec chip
E (888) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (908) EQUALIZER_EXAMPLE: [3.0] Create audio pipeline
I (938) EQUALIZER_EXAMPLE: [3.1] Register all elements to audio pipeline
I (938) EQUALIZER_EXAMPLE: [3.2] Link it together [sdcard]-->fatfs_stream-->wav_decoder-->equalizer-->i2s_stream
I (938) EQUALIZER_EXAMPLE: [3.3] Set up  uri (file as fatfs_stream, wav as wav decoder, and default output is i2s)
I (948) EQUALIZER_EXAMPLE: [4.0] Set up  event listener
I (958) EQUALIZER_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (968) EQUALIZER_EXAMPLE: [4.2] Listening event from peripherals
I (968) EQUALIZER_EXAMPLE: [5.0] Start audio_pipeline
I (988) EQUALIZER_EXAMPLE: [6.0] Listen for all pipeline events
I (998) EQUALIZER_EXAMPLE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=1
W (91708) FATFS_STREAM: No more data, ret:0
W (91708) EQUALIZER_EXAMPLE: [ * ] Stop event received
I (91708) EQUALIZER_EXAMPLE: [7.0] Stop audio_pipeline and release all resources
E (91718) AUDIO_ELEMENT: [file_read] Element already stopped
W (91728) AUDIO_ELEMENT: OUT-[wavdec] AEL_IO_ABORT
W (91728) AUDIO_ELEMENT: OUT-[equalizer] AEL_IO_ABORT
W (91758) AUDIO_PIPELINE: There are no listener registered
W (91758) AUDIO_ELEMENT: [file_read] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91768) AUDIO_ELEMENT: [wavdec] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91778) AUDIO_ELEMENT: [equalizer] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91788) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## 故障排除

运行播放示例，需要满足以下条件：

* 音频文件应采用均衡器支持的格式：

    * 采样率为 11025 Hz、22050 Hz、44100 Hz 或 48000 Hz。
    * 通道数：1 或 2。
    * 音频文件格式为 WAV。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
