| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# ESP Audio Analyzer APP

- [中文版](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")

## Introduction

With the rapid development of smart audio devices, audio application scenarios continue to expand, placing higher demands on audio quality and system performance. Espressif has launched **ESP Audio Analyzer**, providing a convenient and efficient testing approach with a standardized process, combining an intuitive web interface with supporting test firmware to enable standardized audio testing.

Using this tool, users can comprehensively test microphones, speakers, and AEC functionality and performance, quickly identify and troubleshoot issues in audio systems, thereby effectively improving development efficiency and product quality. Key features include:

- Comprehensive functionality: Covers microphones, speakers, and AEC — 11 audio tests in total
- Easy adaptation: Based on ESP Board Manager, supports rapid adaptation to different hardware
- Performance analysis: Provides audio metrics such as THD and SNR for evaluating system performance
- Standardized process: Unified test methods and pass/fail criteria
- Source data download: Supports exporting original recording files for retesting and in-depth analysis
- Visual reports: Generates structured test reports with data, charts, and test results

## Example Set Up

### IDF Default Branch

This project is compatible with ESP-IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2) branches.

## Prerequisites

### Environment Requirements

To ensure the accuracy of test results, the testing environment should be an anechoic chamber or a quiet environment (background noise less than 30 dB).

### Equipment Requirements

The ESP Audio Analyzer testing project uses a computer as the testing platform. The following equipment is required:

- Computer and router: The computer is connected to the router and can access the ESP Audio Analyzer web page (`https://audio-tools.espressif.com.cn`)
- Device under test: Requires at least 4MB Flash, PSRAM recommended, supports playback and recording functions, and can connect to the router via Wi-Fi
- Playback device: Use high-quality audio equipment such as speakers, requiring no distortion during playback and a flat frequency response curve in the 100Hz-8KHz range
- Recording device: Use a computer microphone or external microphone for recording
- Testing tools: Sound level meter, modeling clay (for blocking microphone pickup holes)

### Configuration

1. Wi-Fi Configuration

    Configure Wi-Fi SSID `WiFi STA SSID` and password `WiFi STA Password` in `menuconfig -> Example WIFI Configuration`.

2. Test Function Configuration

    Configure AFE functionality in `menuconfig -> Audio Test Solution Configuration`. Note: When enabling wake word and voice command recognition, at least 8MB Flash is required, and the `partitions_sr_model.csv` partition table must be used.

    - AEC enabled by default: `Enable AEC`
    - Wake word recognition disabled by default: `Enable Wakeup`
    - Voice command recognition disabled by default: `Enable Voice Command`

    After enabling AFE functionality, further configure AFE-related settings according to the hardware design: `ESP GMF Loader -> GMF Audio Configurations -> GMF AI Audio -> Initialize GMF AI Audio AFE -> Audio Front-End (AFE) Configuration`

    - `Feed Task Setting` and `Fetch Task Setting`: Task-related settings
    - `AFE Channel Allocation`: Configure microphone channel information according to the hardware design, using `M`, `R`, and `N` characters to represent microphone, reference signal, and unused channels respectively

    The `Enable store test audio in embed flash` option is enabled by default, embedding necessary audio files to be played into the firmware to reduce dependence on network bandwidth.

3. Partition Configuration

    Configure the partition table in `menuconfig -> Partition Table`.

    - Default uses `partitions.csv` partition table
    - When enabling wake word and voice command recognition, the `partitions_sr_model.csv` partition table must be used, or customize the partition table size according to log information

### Build and Flash

Before building this example, ensure that the ESP-IDF environment is configured. If already configured, skip to the next configuration step. If not configured, first run the following scripts in the ESP-IDF root directory to set up the build environment. For complete steps on configuring and using ESP-IDF, please refer to the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html):

```bash
./install.sh
. ./export.sh
```

- Execute the prebuild script, select the target chip, automatically setup IDF Action Extension, use `esp_board_manager` to select supported board and custom board

    On Linux / macOS, run following command:

    ```bash
    source prebuild.sh
    ```

    On Windows, run following command:

    ```powershell
    .\prebuild.ps1
    ```

- Use `esp_board_manager` to select supported board and custom board, be sure to see [ESP Board Manager](https://components.espressif.com/components/espressif/esp_board_manager), taking ESP32-S3-Korvo2 V3.1 as an example:

```bash
idf.py gen-bmgr-config -b esp32_s3_korvo2_v3
```

- Configure the project using menuconfig, refer to the [Configuration](#configuration) section to modify configuration items

```bash
idf.py menuconfig
```

- Build the example program

```bash
idf.py build
```

- Flash the program and run the monitor tool to view serial output (replace PORT with the port name):

```bash
idf.py -p PORT flash monitor
```

- Use `Ctrl-]` to exit the debug interface

## How to Use the Example

### Features and Usage

- After the example starts running, it will automatically connect to Wi-Fi and start the WebSocket service, waiting for the computer to connect

```plaintext
I (1059) main_task: Calling app_main()
I (1062) MAIN: [1/4] Initializing board...
I (1066) PERIPH_I2C: I2C master bus initialized successfully
I (1072) PERIPH_I2S: I2S[0] TDM,  TX, ws: 45, bclk: 9, dout: 8, din: 10
I (1078) PERIPH_I2S: I2S[0] initialize success: 0x3c2e4644
I (1083) PERIPH_I2S: I2S[0] TDM, RX, ws: 45, bclk: 9, dout: 8, din: 10
I (1089) PERIPH_I2S: I2S[0] initialize success: 0x3c2e4800
I (1095) PERIPH_GPIO: Initialize success, pin: 48, set the default level: 1
I (1101) BOARD_MANAGER: All peripherals initialized
I (1106) BOARD_PERIPH: Reuse periph: i2s_audio_out, ref_count=2
I (1111) DEV_AUDIO_CODEC: DAC is ENABLED
I (1115) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=2
I (1126) ES8311: Work in Slave mode
I (1129) DEV_AUDIO_CODEC: Successfully initialized codec: audio_dac
I (1130) DEV_AUDIO_CODEC: Create esp_codec_dev success, dev:0x3fcece84, chip:es8311
I (1137) BOARD_PERIPH: Reuse periph: i2s_audio_in, ref_count=2
I (1143) DEV_AUDIO_CODEC: ADC over I2S is enabled
I (1147) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=3
I (1155) ES7210: Work in Slave mode
I (1162) ES7210: Enable ES7210_INPUT_MIC1
I (1165) ES7210: Enable ES7210_INPUT_MIC2
I (1167) ES7210: Enable ES7210_INPUT_MIC3
I (1170) ES7210: Enable TDM mode
I (1173) DEV_AUDIO_CODEC: Successfully initialized codec: audio_adc
I (1176) DEV_AUDIO_CODEC: Create esp_codec_dev success, dev:0x3fcecfbc, chip:es7210
I (1183) BOARD_MANAGER: Board manager initialized
I (1188) BOARD_MANAGER: Board Information:
I (1191) BOARD_MANAGER: =================
I (1195) BOARD_MANAGER: Name: esp32_s3_korvo2_v3
I (1199) BOARD_MANAGER: Chip: esp32s3
I (1203) BOARD_MANAGER: Version: 1.3.0
I (1206) BOARD_MANAGER: Description: ESP32-S3 Korvo2 V3 Development Board
I (1213) BOARD_MANAGER: Manufacturer: ESPRESSIF
I (1217) BOARD_MANAGER: =================

I (1221) MAIN: [2/4] Initializing audio system...
I (1225) APP_AUDIO: Initializing audio manager...
I (1230) BOARD_DEVICE: Device handle audio_dac found, Handle: 0x3fceccd8 TO: 0x3fceccd8
I (1238) BOARD_DEVICE: Device handle audio_adc found, Handle: 0x3fceceb4 TO: 0x3fceceb4
E (1247) MODEL_LOADER: Can not find model in partition table
W (1251) AFE_CONFIG: wakenet model not found. please load wakenet model...

/********** General AFE (Audio Front End) Parameter **********/
pcm_config.total_ch_num: 4
pcm_config.mic_num: 2: [ ch0, ch2 ]
pcm_config.ref_num: 1: [ ch1 ]
pcm_config.sample_rate: 16000
afe_type: SR
afe_mode: HIGH PERF
afe_perferred_core: 0
afe_perferred_priority: 5
afe_ringbuf_size: 50
memory_alloc_mode: 3
afe_linear_gain: 1.0
debug_init: false
fixed_first_channel: true
fixed_output_channel: false

/********** AEC (Acoustic Echo Cancellation) **********/
aec_init: true
aec mode: SR_HIGH_PERF
aec_filter_length: 4

/********** SE (Speech Enhancement, Microphone Array Processing) **********/
se_init: true, model: BSS

/********** NS (Noise Suppression) **********/
ns_init: false
ns model name: WEBRTC

/********** VAD (Voice Activity Detection) **********/
vad_init: false
vad_mode: 0
vad_model_name: NULL
vad_min_speech_ms: 128
vad_min_noise_ms: 1000
vad_delay_ms: 128
vad_mute_playback: false
vad_enable_channel_trigger: false

/********** WakeNet (Wake Word Engine) **********/
wakenet_init: false
wakenet_model_name: NULL
wakenet_model_name_2: NULL
wakenet_mode: 0

/********** AGC (Automatic Gain Control) **********/
agc_init: false
agc_mode: WAKENET
agc_compression_gain_db: 9
agc_target_level_dbfs: 9

/**************************************************/
I (1458) AFE: AFE Version: (2MIC_V251128)
I (1458) AFE: Input PCM Config: total 4 channels(2 microphone, 1 playback), sample rate:16000
I (1459) AFE: AFE Pipeline: [input] -> |AEC(SR_HIGH_PERF)| -> |SE(BSS)| -> [output]
I (1466) AFE_MANAGER: Feed task, ch 4, chunk 1024, buf size 8192
I (1472) GMF_AFE: Create AFE, ai_afe-0x3c372f50
I (1476) ESP_GMF_POOL: Registered items on pool:0x3c2e4a74, app_audio_init-496
I (1483) ESP_GMF_POOL: IO, Item:0x3c2e4b84, H:0x3c2e4a88, TAG:io_codec_dev
I (1490) ESP_GMF_POOL: IO, Item:0x3c2e4c90, H:0x3c2e4b94, TAG:io_codec_dev
I (1496) ESP_GMF_POOL: IO, Item:0x3c2e4dac, H:0x3c2e4ca0, TAG:io_file
I (1502) ESP_GMF_POOL: IO, Item:0x3c2e4ec8, H:0x3c2e4dbc, TAG:io_file
I (1508) ESP_GMF_POOL: IO, Item:0x3c2e4fe8, H:0x3c2e4ed8, TAG:io_http
I (1515) ESP_GMF_POOL: IO, Item:0x3c2e50f8, H:0x3c2e4ff8, TAG:io_embed_flash
I (1521) ESP_GMF_POOL: EL, Item:0x3c2e5200, H:0x3c2e5108, TAG:aud_enc
I (1528) ESP_GMF_POOL: EL, Item:0x3c2e5320, H:0x3c2e5210, TAG:aud_dec
I (1534) ESP_GMF_POOL: EL, Item:0x3c2e541c, H:0x3c2e5330, TAG:aud_alc
I (1540) ESP_GMF_POOL: EL, Item:0x3c2e5504, H:0x3c2e542c, TAG:aud_ch_cvt
I (1546) ESP_GMF_POOL: EL, Item:0x3c2e55e8, H:0x3c2e5514, TAG:aud_bit_cvt
I (1553) ESP_GMF_POOL: EL, Item:0x3c2e56d4, H:0x3c2e55f8, TAG:aud_rate_cvt
I (1559) ESP_GMF_POOL: EL, Item:0x3c2e57cc, H:0x3c2e56e4, TAG:ai_aec
I (1565) ESP_GMF_POOL: EL, Item:0x3c36f324, H:0x3c372f50, TAG:ai_afe
I (1572) APP_AUDIO: Audio manager initialized successfully
I (1577) MAIN: [3/4] Connecting to WiFi...
I (1599) pp: pp rom version: e7ae62f
I (1600) net80211: net80211 rom version: e7ae62f
I (1601) wifi:wifi driver task: 3fcbda4c, prio:23, stack:6656, core=0
I (1611) wifi:wifi firmware version: 4df78f2
I (1611) wifi:wifi certification version: v7.0
I (1611) wifi:config NVS flash: enabled
I (1614) wifi:config nano formatting: disabled
I (1618) wifi:Init data frame dynamic rx buffer num: 32
I (1623) wifi:Init static rx mgmt buffer num: 5
I (1627) wifi:Init management short buffer num: 32
I (1632) wifi:Init dynamic tx buffer num: 64
I (1636) wifi:Init static tx FG buffer num: 2
I (1640) wifi:Init static rx buffer size: 1600
I (1644) wifi:Init static rx buffer num: 10
I (1648) wifi:Init dynamic rx buffer num: 32
I (1652) wifi_init: rx ba win: 6
I (1655) wifi_init: accept mbox: 6
I (1658) wifi_init: tcpip mbox: 32
I (1661) wifi_init: udp mbox: 6
I (1664) wifi_init: tcp mbox: 6
I (1667) wifi_init: tcp tx win: 5760
I (1670) wifi_init: tcp rx win: 5760
I (1673) wifi_init: tcp mss: 1440
I (1676) wifi_init: WiFi IRAM OP enabled
I (1680) wifi_init: WiFi RX IRAM OP enabled
I (1684) wifi_init: LWIP IRAM OP enabled
W (1688) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (1696) APP_WIFI: wifi_init_sta finished.
I (1700) APP_WIFI: connect to ap SSID:PLTest password:esp123456
I (1706) phy_init: phy_version 711,97bcf0a2,Aug 25 2025,19:04:10
I (1786) phy_init: Saving new calibration data due to checksum failure or outdated calibration data, mode(0)
I (1796) wifi:mode : sta (dc:b4:d9:0d:99:bc)
I (1797) wifi:enable tsf
I (1798) wifi:Set ps type: 0, coexist: 0

I (1798) APP_WIFI: wifi init finished.
I (1865) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1, snd_ch_cfg:0x0
I (1865) wifi:state: init -> auth (0xb0)
I (1926) wifi:state: auth -> assoc (0x0)
I (1972) wifi:state: assoc -> run (0x10)
I (2030) wifi:connected with PLTest, aid = 2, channel 1, BW20, bssid = cc:2d:21:9a:05:94
I (2031) wifi:security: WPA2-PSK, phy: bgn, rssi: -19
I (2034) wifi:pm start, type: 0

I (2035) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (2043) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (2073) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (2074) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3081) APP_WIFI: got ip:192.168.0.179
I (3081) esp_netif_handlers: sta ip: 192.168.0.179, mask: 255.255.255.0, gw: 192.168.0.1
I (3081) MAIN: [4/4] Starting WebSocket server...
I (3086) APP_WS: Initializing WebSocket server
I (3090) esp_https_server: Starting server
I (3095) esp_https_server: Server listening on port 443
I (3099) APP_WS: WebSocket server started
I (3102) APP_WS: ========================================
I (3107) APP_WS:   WSS Server Ready
I (3111) APP_WS:   Step 1 - Trust the device:
I (3115) APP_WS:     Open in browser: https://192.168.0.179:443
I (3120) APP_WS:     Click 'Advanced' -> 'Continue to 192.168.0.179 (unsafe)'
I (3127) APP_WS:     Browser will then redirect to https://audio-tools.espressif.com.cn/
I (3135) APP_WS:   Step 2 - Connect WebSocket:
I (3139) APP_WS:     wss://192.168.0.179:443/ws
I (3143) APP_WS: ========================================
I (3148) MAIN: ESP Audio Analyzer Started!
I (3152) main_task: Returned from app_main()
```

- Since the ESP device uses a self-signed certificate, browsers will not trust it by default. Therefore, you need to manually trust the device. Please open a new browser tab and directly access the device address printed in the logs, then click "Advanced" and select "Accept the risk and continue" on the page. After completing the trust operation, the browser will automatically redirect to the [ESP Audio Analyzer web page](https://audio-tools.espressif.com.cn/).

    For example, if the address printed in the device logs is `https://192.168.0.179/`.

> [!CAUTION]
> You must manually trust the device, otherwise you will not be able to connect to the device.

- Enter the WebSocket address printed in the device logs (e.g., `wss://192.168.0.179:443/ws`) into the `WebSocket URL` input box on the [ESP Audio Analyzer web page](https://audio-tools.espressif.com.cn/), and click the `Connect Device` button. After successfully connecting to the device, select any test item and follow the prompts to perform the test.

```plaintext
I (12461) APP_WS: Mic gain set to 32 dB
I (12461) APP_WS: Volume set to 15
I (12462) APP_WS: Channel type set to: MRMN
I (12462) APP_WS: Configured: 16000Hz, 16bit, 4ch, mic_gain=32, volume=15, channel_type=MRMN
I (12506) APP_WS: Duplex: recording to ringbuffer (buffer: 1048576 bytes), AFE: disabled
I (12507) APP_AUDIO: Starting recording: sr=16000, ch=4, bits=16, file=NULL, buffer_size=1048576
I (12511) I2S_IF: Paired data: 0x3fcecd04, current mode: record, paired in_enable: 0, paired out_enable: 0
I (12521) I2S_IF: TDM mode, dir: RX, data_bit: 16, slot_bit: 16, total_slot: 4, slot_mask: 0xf
I (12529) I2S_IF: TDM mode, dir: TX, data_bit: 16, slot_bit: 16, total_slot: 4, slot_mask: 0xf
I (12538) ES7210: Bits 16
I (12547) ES7210: Enable ES7210_INPUT_MIC1
I (12549) ES7210: Enable ES7210_INPUT_MIC2
I (12552) ES7210: Enable ES7210_INPUT_MIC3
I (12555) ES7210: Enable TDM mode
I (12560) ES7210: Unmuted
I (12560) Adev_Codec: Open codec device OK
I (12560) APP_AUDIO: Codec device opened: sr=16000, ch=4, bits=16
I (12588) NEW_DATA_BUS: New ringbuffer:0x3c2e49f4, num:1, item_cnt:1048576, db:0x3c376ccc
I (12588) APP_AUDIO: Created record ringbuffer: 1048576 bytes
I (12591) ESP_GMF_TASK: Waiting to run... [tsk:audio_recorder-0x3fcea56c, wk:0x0, run:0]
I (12591) APP_AUDIO: Warming up codec and I2S driver...
I (12598) ESP_GMF_TASK: Waiting to run... [tsk:audio_recorder-0x3fcea56c, wk:0x3c376f08, run:0]
I (12803) APP_WS: Recording started
I (12803) APP_AUDIO: Recording started successfully
I (12803) APP_WS: Cleared record event bits before creating new task
I (12803) ESP_GMF_AENC: Open, type:PCM, acquire in frame: 1280, out frame: 1280
I (12806) APP_WS: Record send task started
I (12813) ESP_GMF_TASK: One times job is complete, del[wk:0x3c376f08, ctx:0x3c376d6c, label:aud_enc_open]
I (12817) APP_WS: Record buffer allocated: 16384 bytes
I (12826) ESP_GMF_PORT: ACQ IN, new self payload:0x3c376f08, port:0x3c376ea4, el:0x3c376d6c-aud_enc
I (12806) APP_WS: Duplex: playing from file: embed://tone/0_embed_0dBFS_1KHz_16K.flac
I (12847) APP_AUDIO: Starting playback: sr=16000, ch=4, bits=16, file=embed://tone/0_embed_0dBFS_1KHz_16K.flac, buffer_size=0
I (12858) I2S_IF: Current mode(playback) and peer mode have same sample_rate 16000, channel 4, bits_per_sample 16
I (12868) I2S_IF: TDM mode, dir: TX, data_bit: 16, slot_bit: 16, total_slot: 4, slot_mask: 0xf
I (12892) Adev_Codec: Open codec device OK
I (12892) APP_AUDIO: Playback codec opened: sr=16000, ch=4, bits=16
I (12893) ESP_GMF_TASK: Waiting to run... [tsk:audio_playback-0x3fccb6b8, wk:0x0, run:0]
I (12899) ESP_GMF_EMBED_FLASH: The read item is 0, embed://tone/0
I (12900) APP_WS: Playback started
I (12905) ESP_GMF_TASK: One times job is complete, del[wk:0x3c478760, ctx:0x3c377550, label:aud_dec_open]
I (12908) APP_AUDIO: Playback started successfully
I (12917) ESP_GMF_PORT: ACQ IN, new self payload:0x3c478760, port:0x3c478574, el:0x3c377550-aud_dec
I (12922) APP_WS: Cleared playback event bits before creating new task
I (12931) ESP_ES_PARSER: The version of es_parser is v1.0.0
I (12942) AUD_Dec_Parse: FLAC sample_rate 16000 duration:10000 max_frame:583
I (12937) APP_WS: Duplex mode started, AFE: disabled
W (12953) ESP_GMF_ASMP_DEC: Not enough memory for out, need:2304, old: 1024, new: 2304
I (12937) APP_WS: Playback control task started
I (12962) ESP_GMF_TASK: One times job is complete, del[wk:0x3c4787e8, ctx:0x3c37763c, label:aud_rate_cvt_open]
I (12975) ESP_GMF_TASK: One times job is complete, del[wk:0x3c47d244, ctx:0x3c377784, label:aud_ch_cvt_open]
I (12985) ESP_GMF_TASK: One times job is complete, del[wk:0x3c4787e8, ctx:0x3c478354, label:aud_bit_cvt_open]
I (13006) wifi:<ba-add>idx:0 (ifx:0, cc:2d:21:9a:05:94), tid:0, ssn:41, winSize:64
I (22506) APP_AUDIO: Stopping playback...
I (22777) ESP_GMF_EMBED_FLASH: Closed, pos: 75776/76843
I (22777) ESP_GMF_CODEC_DEV: CLose, 0x3c4785b4, pos = 1262592/0
I (22777) ESP_GMF_TASK: One times job is complete, del[wk:0x3c47d1b4, ctx:0x3c377550, label:aud_dec_close]
I (22786) ESP_GMF_TASK: One times job is complete, del[wk:0x3c47d1dc, ctx:0x3c37763c, label:aud_rate_cvt_close]
I (22796) ESP_GMF_TASK: One times job is complete, del[wk:0x3c47d21c, ctx:0x3c377784, label:aud_ch_cvt_close]
I (22805) ESP_GMF_TASK: One times job is complete, del[wk:0x3c47d280, ctx:0x3c478354, label:aud_bit_cvt_close]
I (22815) ESP_GMF_TASK: Waiting to run... [tsk:audio_playback-0x3fccb6b8, wk:0x0, run:0]
I (22829) I2S_IF: Pending out channel for in channel running
I (22829) APP_WS: Playback stopped
I (22831) APP_AUDIO: Playback stopped
I (22835) APP_WS: Signaling playback control task to stop...
I (22840) APP_WS: Playback control task received stop signal
I (22845) APP_AUDIO: Stopping playback...
I (23050) APP_WS: Playback stopped
I (23050) APP_AUDIO: Playback stopped
```

## Troubleshooting

### Device Frequently Disconnects

- Ensure that the computer and device are connected to the same Wi-Fi network
- To improve connection stability, perform testing in an environment with minimal interference

### Too Little Audio Data Recorded, or Recording Interrupted Early

- This is often caused by audio data accumulating faster than it can be transmitted. You can try increasing the size of `WS_AUDIO_BUFFER_SIZE` in `app_websocket.c`
- You can also try improving Wi-Fi performance by referring to the [Wi-Fi Performance Optimization](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi-driver/wifi-performance-and-power-save.html#how-to-improve-wi-fi-performance) documentation, or reduce the distance between the device and router

### Cannot Start WebSocket Server

- This is often caused by insufficient internal memory. It is recommended to enable PSRAM to preserve more internal memory space

### Insufficient IRAM During Build/Link

- Please refer to the [IRAM Optimization](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/ram-usage.html#optimizing-iram-usage) documentation to disable unnecessary optimization options

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
