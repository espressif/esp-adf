# Voice Wake-up and Voice Command Word Detection (WWE + MN)

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example demonstrates how to read ambient sound data from a microphone, process it through the audio front-end (AFE) algorithm and MultiNet model, and finally output the wake-up status or command word index.

The pipeline is as follows:

```
mic ---> codec_chip ---> i2s_driver ---> afe ---> multinet ---> audio_recorder ---> output
```

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

### Flash Partition Requirement

The default board for this example is `ESP32-S3-Korvo-2 v3.0` that is based on ESP32-S3. Since the component `esp-sr` used on ESP32-S3 requires a flash or microSD card to access the model data and the default storage medium used in this example is flash, a fixed partition needs to be added to the flash partition table:

```
model, data, spiffs,  , 4152K,
```

Size of this partition can be determined based on the prompts generated inside the compiled log as follows:

```
Recommended model partition size:  4152KB
```

If you choose to store the data in microSD card or choose ESP32 based development board to test this example, there is no need to include this item in the flash partition. For more information, please see [Model loading method](https://github.com/espressif/esp-sr/blob/master/docs/flash_model/README.md).

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### IDF Branch

- The command to switch to IDF release/v5.0 branch is as follows:

```
cd $IDF_PATH
git checkout master
git pull
git checkout release/v5.0
git submodule update --init --recursive
```

This example also needs to merge `idf_v4.4_freertos.patch` into IDF. The merge command is as follows:

```
cd $IDF_PATH
git apply $ADF_PATH/idf_patches/idf_v4.4_freertos.patch
```

### Configuration

The default board for this example is `ESP32-S3-Korvo-2 v3.0`. Please set target to ESP32S3 with `idf.py set-target esp32s3` first. If you need to run this example on another board, select the board configuration in menuconfig, e.g. select `ESP32-Lyrat-Mini V1.1`, and set target to ESP32 with `idf.py set-target esp32`. For esp32, `Multinet` is no long supported, and it's disabled by default.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example

### Example Functionality

- After the example starts running, it automatically starts detecting background ambient sounds. The log is as follows:

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

- If the wake word is said at this point ("hi, lexin" by default), the device will be woken up and the tone "ding" will be played:

```
I (93419) wwe_example: rec_engine_cb - REC_EVENT_WAKEUP_START
I (94099) wwe_example: rec_engine_cb - REC_EVENT_VAD_START
W (94099) wwe_example: voice read begin
I (94119) AMRNB_ENCODER: amrnb open
```

- If you say one of the command words (e.g., "打开空调") after saying the wake-up word and hearing the "ding" sound, the command word index (e.g., `wwe_example: command 0`) will be printed out in the log and the audio "好的" will be played.

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

### Example Log

A complete log is as follows:

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

## Troubleshooting

1. This application may trigger a task watchdog when running on an ESP32-based development board.
2. For development boards using `lyrat_msc`, please make sure that you have the latest version of firmware running on the zl38063. You can enforce a download of the latest firmware once by modifying the function `zl38063_ codec_init` in [zl38063.c](https://github.com/espressif/esp-adf/blob/master/components/audio_hal/driver/zl38063/zl38063.c). After the development board is booted and the zl38063 firmware is downloaded, you can revert to the previous code and replace the code as follows:

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

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
