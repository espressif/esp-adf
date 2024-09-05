# 语音唤醒及语音命令词检测 (WWE + MN) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

本例程演示了从麦克风读取环境声音数据，经过声学前端 (audio front-end, AFE) 算法和 MultiNet 模型处理分析，最后输出唤醒状态或者命令词索引。

管道的数据流向，如下图所示：

```
mic ---> codec_chip ---> i2s_driver ---> afe ---> multinet ---> audio_recorder ---> output
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

### Flash 分区要求

本例程默认选择的开发板是基于 ESP32-S3 的 `ESP32-S3-Korvo-2 v3.0`，因使用到的组件 `esp-sr` 在 ESP32-S3 上需要使用 flash 或者 microSD 卡来存取模型数据，而本例程默认使用的存储介质是 flash，所以需要在 flash 分区表中添加固定的分区：

```
model, data, spiffs,  , 4152K,
```

该分区的大小可以根据编译时候产生的日志里面的提示来确定，对应的提示为：

```
Recommended model partition size:  4152KB
```

如果选择的是将数据存储在 microSD 卡中，或者是选择基于 ESP32 的开发板来测试本例程，则 flash 分区中不用包含此项。请参阅 [《模型加载方式》](https://github.com/espressif/esp-sr/blob/master/docs/flash_model/README_CN.md)。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.0 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### IDF 分支

- IDF release/v5.0 分支切换命令如下：

```
cd $IDF_PATH
git checkout master
git pull
git checkout release/v5.0
git submodule update --init --recursive
```

本例程还需给 IDF 合入 `idf_v4.4_freertos.patch`，合入命令如下：

```
cd $IDF_PATH
git apply $ADF_PATH/idf_patches/idf_v4.4_freertos.patch
```

### 配置

本例程默认选择的开发板是 `ESP32-S3-Korvo-2 v3.0`，请先使用 `idf.py set-target esp32s3` 将目标设定为 ESP32S3。如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`，并使用 `idf.py set-target esp32` 将目标设定为 ESP32。在 ESP32 上，`Multinet` 已不再支持，并默认关闭。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

### 功能和用法

- 例程开始运行后，程序自动开始检测周围的背景环境声音，打印如下：

```
I (0) cpu_start: App cpu up.
I (726) spiram: SPI SRAM memory test OK
W (726) rtcinit: efuse read fail, set default blk2_version: 1, blk1_version:2

I (739) cpu_start: Pro cpu start user code
I (739) cpu_start: cpu freq: 240000000
I (739) cpu_start: Application information:
I (741) cpu_start: Project name:     test_recorder
I (747) cpu_start: App version:      v2.2-303-g80ec082b-dirty
I (753) cpu_start: Compile time:     Jan  4 2022 11:12:02
I (759) cpu_start: ELF file SHA256:  44262c7d4b164866...
I (765) cpu_start: ESP-IDF:          v4.4-dev-3622-g08414946b7-dirty
I (772) heap_init: Initializing. RAM available for dynamic allocation:
I (780) heap_init: At 3FCA4408 len 0003BBF8 (238 KiB): D/IRAM
I (786) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (793) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (799) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (807) spi_flash: detected chip: gd
I (811) spi_flash: flash io: qio
I (816) sleep: Configure to isolate all GPIO pins in sleep state
I (822) sleep: Enable automatic switching of GPIO sleep configuration
I (829) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (859) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (859) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (869) SDCARD: Using 1-line SD mode
I (879) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (879) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (889) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (929) SDCARD: CID name SD!

I (1399) DRV8311: ES8311 in Slave mode
I (1409) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1419) ES7210: ES7210 in Slave mode
I (1429) ES7210: Enable ES7210_INPUT_MIC1
I (1429) ES7210: Enable ES7210_INPUT_MIC2
W (1429) AUDIO_BOARD: The board has already been initialized!

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:03:30                     |
------------------------------------------------------------------------------
I (1469) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (1469) wwe_example: Func:setup_player, Line:110, MEM Total:8596655 Bytes, Inter:259939 Bytes, Dram:259939 Bytes

I (1479) wwe_example: esp_audio instance is:0x3d808520

Initializing SPIFFS
Partition size: total: 3900791, used: 3775542
E (1729) I2S: register I2S object to platform failed
I (1729) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (1739) wwe_example: Recorder has been created
model_name: hilexin7q8 model_data: /srmodel/hilexin7q8/wn7q8_data
MC Quantized-8 wakeNet7: wakeNet7Q8_v2_hilexin_5_0.97_0.90, mode:2, p:3, (Dec 10 2021 20:59:49)
Initial TWO-MIC auido front-end for speech recognition, mode:0, (Dec 10 2021 11:08:00)
model_name: mn3cn model_data: /srmodel/mn3cn/mn3cn_data
SINGLE_RECOGNITION: V3.0 CN; core: 0; (Dec 16 2021 17:35:04)
I (4309) MN: ---------------------SPEECH COMMANDS---------------------
I (4309) MN: Command ID0, phrase 0: da kai kong tiao
I (4309) MN: Command ID1, phrase 1: guan bi kong tiao
I (4319) MN: Command ID2, phrase 2: zeng da feng su
I (4319) MN: Command ID3, phrase 3: jian xiao feng su
I (4329) MN: Command ID4, phrase 4: sheng gao yi du
I (4339) MN: Command ID5, phrase 5: jiang di yi du
I (4339) MN: Command ID6, phrase 6: zhi re mo shi
I (4349) MN: Command ID7, phrase 7: zhi leng mo shi
I (4349) MN: Command ID8, phrase 8: song feng mo shi
I (4359) MN: Command ID9, phrase 9: jie neng mo shi
I (4359) MN: Command ID10, phrase 10: chu shi mo shi
I (4369) MN: Command ID11, phrase 11: jian kang mo shi
I (4369) MN: Command ID12, phrase 12: shui mian mo shi
I (4379) MN: Command ID13, phrase 13: da kai lan ya
I (4389) MN: Command ID14, phrase 14: guan bi lan ya
I (4389) MN: Command ID15, phrase 15: kai shi bo fang
I (4399) MN: Command ID16, phrase 16: zan ting bo fang
I (4399) MN: Command ID17, phrase 17: ding shi yi xiao shi
I (4409) MN: Command ID18, phrase 18: da kai dian deng
I (4419) MN: Command ID19, phrase 19: guan bi dian deng
I (4429) MN: ---------------------------------------------------------
```

- 如果此时说了唤醒词（默认为 “hi, lexin”），设备则会被唤醒，并播放提示音“叮”：

```
I (93419) wwe_example: rec_engine_cb - REC_EVENT_WAKEUP_START
I (94099) wwe_example: rec_engine_cb - REC_EVENT_VAD_START
W (94099) wwe_example: voice read begin
I (94119) AMRNB_ENCODER: amrnb open
```

- 如果在说了唤醒词且听到“叮”的声音之后，说了命令词之一（如“打开空调”），则会通过日志打印出命令词索引（如 `wwe_example: command 0`），并播放语音“好的”：

```
phrase_id = 0, the prob = -7.536505
I (139389) wwe_example: rec_engine_cb - AUDIO_REC_COMMAND_DECT
W (139389) wwe_example: command 0
I (141199) wwe_example: rec_engine_cb - REC_EVENT_VAD_STOP
W (141199) wwe_example: voice read stopped
I (141199) wwe_example: File closed
I (141209) AMRNB_ENCODER: amrnb close
I (142099) wwe_example: rec_engine_cb - REC_EVENT_WAKEUP_END
```

### 日志输出

以下是本例程的完整日志。

```
I (0) cpu_start: App cpu up.
I (693) spiram: SPI SRAM memory test OK
W (693) rtcinit: efuse read fail, set default blk2_version: 1, blk1_version:2

I (706) cpu_start: Pro cpu start user code
I (706) cpu_start: cpu freq: 240000000
I (706) cpu_start: Application information:
I (708) cpu_start: Project name:     test_recorder
I (714) cpu_start: App version:      v2.2-303-g80ec082b-dirty
I (720) cpu_start: Compile time:     Jan  4 2022 11:12:02
I (726) cpu_start: ELF file SHA256:  44262c7d4b164866...
I (732) cpu_start: ESP-IDF:          v4.4-dev-3622-g08414946b7-dirty
I (740) heap_init: Initializing. RAM available for dynamic allocation:
I (747) heap_init: At 3FCA4408 len 0003BBF8 (238 KiB): D/IRAM
I (753) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (760) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (766) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (774) spi_flash: detected chip: gd
I (778) spi_flash: flash io: qio
I (783) sleep: Configure to isolate all GPIO pins in sleep state
I (789) sleep: Enable automatic switching of GPIO sleep configuration
I (796) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (826) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (826) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (836) SDCARD: Using 1-line SD mode
I (846) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (846) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (856) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (896) SDCARD: CID name SD!

I (1366) DRV8311: ES8311 in Slave mode
I (1376) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1386) ES7210: ES7210 in Slave mode
I (1396) ES7210: Enable ES7210_INPUT_MIC1
I (1396) ES7210: Enable ES7210_INPUT_MIC2
W (1396) AUDIO_BOARD: The board has already been initialized!

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:03:30                     |
------------------------------------------------------------------------------
I (1436) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (1436) wwe_example: Func:setup_player, Line:110, MEM Total:8596655 Bytes, Inter:259939 Bytes, Dram:259939 Bytes

I (1446) wwe_example: esp_audio instance is:0x3d808520

Initializing SPIFFS
Partition size: total: 3900791, used: 3775542
E (1696) I2S: register I2S object to platform failed
I (1696) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (1706) wwe_example: Recorder has been created
model_name: hilexin7q8 model_data: /srmodel/hilexin7q8/wn7q8_data
MC Quantized-8 wakeNet7: wakeNet7Q8_v2_hilexin_5_0.97_0.90, mode:2, p:3, (Dec 10 2021 20:59:49)
Initial TWO-MIC auido front-end for speech recognition, mode:0, (Dec 10 2021 11:08:00)
model_name: mn3cn model_data: /srmodel/mn3cn/mn3cn_data
SINGLE_RECOGNITION: V3.0 CN; core: 0; (Dec 16 2021 17:35:04)
I (4276) MN: ---------------------SPEECH COMMANDS---------------------
I (4276) MN: Command ID0, phrase 0: da kai kong tiao
I (4276) MN: Command ID1, phrase 1: guan bi kong tiao
I (4286) MN: Command ID2, phrase 2: zeng da feng su
I (4286) MN: Command ID3, phrase 3: jian xiao feng su
I (4296) MN: Command ID4, phrase 4: sheng gao yi du
I (4306) MN: Command ID5, phrase 5: jiang di yi du
I (4306) MN: Command ID6, phrase 6: zhi re mo shi
I (4316) MN: Command ID7, phrase 7: zhi leng mo shi
I (4316) MN: Command ID8, phrase 8: song feng mo shi
I (4326) MN: Command ID9, phrase 9: jie neng mo shi
I (4326) MN: Command ID10, phrase 10: chu shi mo shi
I (4336) MN: Command ID11, phrase 11: jian kang mo shi
I (4336) MN: Command ID12, phrase 12: shui mian mo shi
I (4346) MN: Command ID13, phrase 13: da kai lan ya
I (4356) MN: Command ID14, phrase 14: guan bi lan ya
I (4356) MN: Command ID15, phrase 15: kai shi bo fang
I (4366) MN: Command ID16, phrase 16: zan ting bo fang
I (4366) MN: Command ID17, phrase 17: ding shi yi xiao shi
I (4376) MN: Command ID18, phrase 18: da kai dian deng
I (4386) MN: Command ID19, phrase 19: guan bi dian deng
I (4396) MN: ---------------------------------------------------------

I (6606) wwe_example: rec_engine_cb - REC_EVENT_WAKEUP_START
I (7346) wwe_example: rec_engine_cb - REC_EVENT_VAD_START
W (7346) wwe_example: voice read begin
I (7346) AMRNB_ENCODER: amrnb open
phrase_id = 0, the prob = -12.604690
I (8436) wwe_example: rec_engine_cb - AUDIO_REC_COMMAND_DECT
W (8436) wwe_example: command 0
I (10176) wwe_example: rec_engine_cb - REC_EVENT_VAD_STOP
W (10186) wwe_example: voice read stopped
I (10186) wwe_example: File closed
I (10196) AMRNB_ENCODER: amrnb close
I (11076) wwe_example: rec_engine_cb - REC_EVENT_WAKEUP_END
```

## 故障排除

1. 此应用程序在以 ESP32 为核心的开发板上运行可能会触发任务看门狗。
2. 使用 `lyrat_msc` 的开发板，请确保 zl38063 上运行的是最新版本的固件，可以通过修改 [zl38063.c](https://github.com/espressif/esp-adf/blob/master/components/audio_hal/driver/zl38063/zl38063.c) 中的函数 `zl38063_codec_init` 来强制下载一次最新固件。开发板启动并下载好 zl38063 固件之后，可以还原到之前的代码，替换代码如下：

```c
esp_err_t zl38063_codec_init(audio_hal_codec_config_t *cfg)
{
    if (zl38063_codec_initialized()) {
        ESP_LOGW(TAG, "The zl38063 codec has been already initialized");
        return ESP_OK;
    }
    tw_upload_dsp_firmware(-1);
    gpio_config_t  borad_conf;
    memset(&borad_conf, 0, sizeof(borad_conf));
    borad_conf.mode = GPIO_MODE_OUTPUT;
    borad_conf.pin_bit_mask = 1UL << (get_reset_board_gpio());
    borad_conf.pull_down_en = 0;
    borad_conf.pull_up_en = 0;

    gpio_config_t  pa_conf;
    memset(&pa_conf, 0, sizeof(pa_conf));
    pa_conf.mode = GPIO_MODE_OUTPUT;
    pa_conf.pin_bit_mask = 1UL << (get_pa_enable_gpio());
    pa_conf.pull_down_en = 0;
    pa_conf.pull_up_en = 0;

    gpio_config(&pa_conf);
    gpio_config(&borad_conf);
    gpio_set_level(get_pa_enable_gpio(), 1);        //enable PA
    gpio_set_level(get_reset_board_gpio(), 0);      //enable DSP
    codec_init_flag = 1;
    return ESP_OK;
}
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
