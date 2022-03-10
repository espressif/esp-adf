# 算法例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

本例程的功能是在播放音乐的同时将麦克风收录的声音先进行回声消除，然后存储到 microSD 卡中。

本例程有两条管道，第一条管道读取 flash 中的 MP3 音乐文件并播放；第二条管道是录音的过程，读取到的数据经过 AEC、AGC、NS 算法处理，再编码成 WAV 格式，最后保存在 microSD 卡中。最后我们可以比较原始音频与录制的音频之间的差异。

- 播放 MP3 的管道：

  ```c
  [flash] ---> mp3_decoder ---> filter ---> i2s_stream ---> [codec_chip]
  ```

 - 录制 WAV 的管道：

   ```c
   [codec_chip] ---> i2s_stream ---> filter ---> algorithm ---> wav_encoder ---> fatfs_stream ---> [sdcard]
   ```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

准备好官方音频开发板：

- 把 microSD 卡插入到开发板的卡槽中备用。

烧录固件并运行例程：

- 开发板上电后后自动运行例程。
- 例程完成后，你可以打开 microSD 卡目录 `/sdcard/aec_out.wav` 收听录音文件。


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法
下载运行后，开发板应该输出以下日志：
```
I (10) boot: ESP-IDF v3.3.2-107-g722043f 2nd stage bootloader
I (10) boot: compile time 17:34:10
I (10) boot: Enabling RNG early entropy source...
I (14) boot: SPI Speed      : 80MHz
I (19) boot: SPI Mode       : DIO
I (23) boot: SPI Flash Size : 2MB
I (27) boot: Partition Table:
I (30) boot: ## Label            Usage          Type ST Offset   Length
I (37) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (45) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (52) boot:  2 factory          factory app      00 00 00010000 00100000
I (60) boot: End of partition table
I (64) boot_comm: chip revision: 1, min. application chip revision: 0
I (71) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x44d80 (281984) map
I (162) esp_image: segment 1: paddr=0x00054da8 vaddr=0x3ffb0000 size=0x01f7c (  8060) load
I (165) esp_image: segment 2: paddr=0x00056d2c vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (169) esp_image: segment 3: paddr=0x00057134 vaddr=0x40080400 size=0x08edc ( 36572) load
I (191) esp_image: segment 4: paddr=0x00060018 vaddr=0x400d0018 size=0x3be68 (245352) map
0x400d0018: _flash_cache_start at ??:?

I (263) esp_image: segment 5: paddr=0x0009be88 vaddr=0x400892dc size=0x02bbc ( 11196) load
0x400892dc: xTaskResumeAll at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/tasks.c:3507

I (274) boot: Loaded app from partition at offset 0x10000
I (274) boot: Disabling RNG early entropy source...
I (275) cpu_start: Pro cpu up.
I (278) cpu_start: Application information:
I (283) cpu_start: Project name:     esp-idf
I (288) cpu_start: App version:      v1.0-667-g3cc7193
I (294) cpu_start: Compile time:     Nov 18 2020 17:34:09
I (300) cpu_start: ELF file SHA256:  919997b2271ad260...
I (306) cpu_start: ESP-IDF:          v3.3.2-107-g722043f
I (312) cpu_start: Starting app cpu, entry point is 0x400811fc
0x400811fc: call_start_cpu1 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (322) heap_init: Initializing. RAM available for dynamic allocation:
I (329) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (335) heap_init: At 3FFB3140 len 0002CEC0 (179 KiB): DRAM
I (341) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (348) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (354) heap_init: At 4008BE98 len 00014168 (80 KiB): IRAM
I (360) cpu_start: Pro cpu start user code
I (154) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (155) ALGORITHM_EXAMPLES: [1.0] Mount sdcard
I (661) ALGORITHM_EXAMPLES: [2.0] Start codec chip
W (682) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (692) ALGORITHM_EXAMPLES: [3.0] Create audio pipeline_rec for recording
I (692) ALGORITHM_EXAMPLES: [3.1] Create i2s stream to read audio data from codec chip
I (699) ALGORITHM_EXAMPLES: [3.2] Create mp3 encoder to encode mp3 format
I (705) ALGORITHM_EXAMPLES: [3.3] Create fatfs stream to write data to sdcard
I (713) ALGORITHM_EXAMPLES: [3.4] Register all elements to audio pipeline
I (720) ALGORITHM_EXAMPLES: [3.5] Link it together [codec_chip]-->i2s_stream-->mp3_encoder-->fatfs_stream-->[sdcard]
I (732) ALGORITHM_EXAMPLES: [3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 encoder)
I (740) ALGORITHM_EXAMPLES: [4.0] Create audio pipeline_rec for recording
I (748) ALGORITHM_EXAMPLES: [4.1] Create fatfs stream to read data from sdcard
I (756) ALGORITHM_EXAMPLES: [4.2] Create mp3 decoder to decode mp3 file
I (763) ALGORITHM_EXAMPLES: [4.3] Create i2s stream to write data to codec chip
I (773) ALGORITHM_EXAMPLES: [4.4] Register all elements to audio pipeline
I (778) ALGORITHM_EXAMPLES: [4.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (790) ALGORITHM_EXAMPLES: [4.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)
I (801) ALGORITHM_EXAMPLES: [5.0] Set up event listener
I (807) ALGORITHM_EXAMPLES: [5.1] Listening event from all elements of pipeline
I (815) ALGORITHM_EXAMPLES: [5.2] Listening event from peripherals
I (821) ALGORITHM_EXAMPLES: [6.0] Start audio_pipeline
I (869) ALGORITHM_EXAMPLES: [7.0] Listen for all pipeline events
```

## 故障排除

- 如果 AEC 效果不是很好，你可以打开 `DEBUG_ALGO_INPUT` define，就可以获取到原始的输入数据（左声道是从麦克风获取到的信号，右声道是扬声器端获取到的信号），并使用音频分析工具查看麦克风信号和扬声器信号之间的延迟。

- AEC 内部缓冲机制要求录制信号相对于相应的参考（回放）信号延迟 0 - 10 ms 左右。

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
