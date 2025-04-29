# COZE WebSocket Bidirectional Streaming Conversation

- [Chinese](./README_CN.md)|English

## Example Introduction

This example implements the COZE Voice Dialog WebSocket OpenAPI, supporting voice interaction with the agent through direct conversation, voice wake-up, and key-triggered interruption.

## Example creation

### IDF default branch

This example is compatible with IDF release/v5.4 and later branches.

### Prerequisites

First, obtain the `Access Token` and `BOT ID` from the [Coze documentation](https://bytedance.larkoffice.com/docx/Da6qd87pQodvNrxdFYrcnzMxnsh).

For more details on WebSocket integration, refer to the [Bidirectional Streaming Conversation Events documentation](https://www.coze.cn/open/docs/developer_guides/streaming_chat_event).

This example is built on the [ESP-GMF](https://github.com/espressif/esp-gmf) framework and demonstrates audio initialization along with `3A processing` (Automatic Gain Control, Echo Cancellation, and Noise Suppression).

## Preparation

### Hardware Preparation

- This example uses the esp32-s3-korvo-2 development board by default. For hardware details, please refer to the [documentation](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html).
To use a different board version, go to `menuconfig -> Example Configuration → Audio Board` to make your selection.

> If you are using a custom development board, select Custom audio board and modify the corresponding IO configuration in the [esp_gmf_gpio_config.h](components/common/esp_gmf_gpio_config.h#162) file.

### About encoding formats

- Currently, upstream data uses the PCM format, while downstream data is encoded in Opus. More audio codecs will be supported in future updates.

### Operation Modes

> The system currently supports the following operation modes:

- `Continuous Mode`: Users can interact with the device using voice commands without needing a wake word.

- `Voice Wakeup Mode`: Users need to use a wake word to activate the device. After activation, users can interact with the device via voice. The default wake word is `Hi LeXin`, which can be changed in `menuconfig -> ESP Speech Recognition → use wakenet → Select wake words`.

- `Key Press Mode`: Users can activate the device by pressing a button, after which they can interact with the device via voice. The default button is `REC`.

> The default operating mode is `Continuous Mode`.

The support of each operation mode across different chips is as follows:

|  CHIPS   |  Wake Word Mode  |   Continuous Mode  |   Key Press Mode  |
|  ----  | ----  |  ----  | ----  |
| ESP32S3  | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32P4  | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32  | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32C3  | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32C6  | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32C5  | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/no-icon.png "Incompatible") <sup> **1** </sup> | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |
| ESP32S2  | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/no-icon.png "Incompatible") | ![alt text](../../../docs/_static/yes-icon.png "Compatible") |

**Note 1:** Planned to be supported in future versions

When using different modes, you need to adjust corresponding parameters in
`Component config → ESP Audio Simple Player`.
1.When using key press mode, the audio input and output are configured as 16-bit, mono. The configuration is as follows:

```text
CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST=1
CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_16BIT=y
```

2.When using a single ES8311 for both input and output, and enabling functions such as echo cancellation, the audio input and output are configured as 16-bit, stereo. The configuration is as follows:

```text
CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST=2
CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_16BIT=y
```

3.When using ES8311 for audio output and ES7210 for input, the audio input and output are configured as 32-bit, stereo. The configuration is as follows (default mode):

```text
CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST=2
CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_32BIT=y
```

### Configuration Steps

1. Enter the obtained Access Token and BOT ID in `Menuconfig -> Example Configuration`. The `Access Token` typically starts with `pat_`.

2. Fill in the `Wi-Fi` information in `Menuconfig -> Example Configuration`.

Compilation and Download

Before compiling this example, ensure that the ESP-IDF environment is properly set up. If it is already configured, you can skip this step and proceed to the next configuration. If not, run the script below in the root directory of ESP-IDF to set up the build environment. For detailed setup and usage instructions, please refer to the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html).

```bash
./install.sh
. ./export.sh
```

- Set the target chip (using ESP32-S3 as an example):：

```bash
idf.py set-target esp32s3
```

- Compile the example program:

```bash
idf.py build
```

- Flash the program and use the monitor tool to view serial output (replace PORT with the actual port name):

```bash
idf.py -p PORT flash monitor
```

## How to Use the Example

### Features and Usage

- Once the example starts running, if the following log appears, it indicates that a connection has been successfully established with the server, and you can begin the conversation.:

```text
IE (1029) COZE_CHAT_WS: Failed to get SPIFFS partition information (ESP_ERR_INVALID_STATE)
I (1031) example_connect: Start example_connect.
I (1035) pp: pp rom version: e7ae62f
I (1038) net80211: net80211 rom version: e7ae62f
I (1044) wifi:wifi driver task: 3fced994, prio:23, stack:6656, core=0
I (1051) wifi:wifi firmware version: 7d2994f4b
I (1052) wifi:wifi certification version: v7.0
I (1057) wifi:config NVS flash: enabled
I (1060) wifi:config nano formatting: disabled
I (1064) wifi:Init data frame dynamic rx buffer num: 32
I (1069) wifi:Init static rx mgmt buffer num: 5
I (1074) wifi:Init management short buffer num: 32
I (1078) wifi:Init dynamic tx buffer num: 32
I (1082) wifi:Init static tx FG buffer num: 2
I (1086) wifi:Init static rx buffer size: 1600
I (1090) wifi:Init static rx buffer num: 16
I (1094) wifi:Init dynamic rx buffer num: 32
I (1099) wifi_init: rx ba win: 6
I (1101) wifi_init: accept mbox: 6
I (1104) wifi_init: tcpip mbox: 32
I (1107) wifi_init: udp mbox: 6
I (1110) wifi_init: tcp mbox: 6
I (1113) wifi_init: tcp tx win: 65535
I (1117) wifi_init: tcp rx win: 5760
I (1120) wifi_init: tcp mss: 1440
I (1123) wifi_init: WiFi IRAM OP enabled
I (1127) wifi_init: WiFi RX IRAM OP enabled
I (1131) phy_init: phy_version 701,f4f1da3a,Mar  3 2025,15:50:10
I (1178) wifi:mode : sta (74:4d:bd:9d:b6:30)
I (1179) wifi:enable tsf
I (1180) example_connect: Connecting to xtworks...
W (1180) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (1188) example_connect: Waiting for IP(s)
I (3665) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1, snd_ch_cfg:0x0
I (3665) wifi:state: init -> auth (0xb0)
I (3668) wifi:state: auth -> assoc (0x0)
I (3689) wifi:state: assoc -> run (0x10)
I (3714) wifi:connected with xtworks, aid = 58, channel 11, BW20, bssid = ec:56:23:e9:7e:f0
I (3714) wifi:security: WPA2-PSK, phy: bgn, rssi: -38
I (3716) wifi:pm start, type: 1

I (3719) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3727) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3735) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (4830) esp_netif_handlers: example_netif_sta ip: 192.168.3.7, mask: 255.255.255.0, gw: 192.168.3.1
I (4830) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.3.7
I (5031) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:764d:bdff:fe9d:b630, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5034) example_common: Connected to example_netif_sta
I (5039) example_common: - IPv4 address: 192.168.3.7,
I (5044) example_common: - IPv6 address: fe80:0000:0000:0000:764d:bdff:fe9d:b630, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5054) ESP_COZE_CHAT: wss_url: ws://ws.coze.cn/v1/chat?bot_id=7480******************
I (5061) ESP_COZE_CHAT: token: Bearer pat*************************************
I (5071) ESP_COZE_CHAT: WEBSOCKET_EVENT_BEGIN
I (5076) websocket_client: Started
I (5677) ESP_COZE_CHAT: WEBSOCKET_EVENT_CONNECTED
I (5678) ESP_COZE_CHAT: WS connected
I (6491) wifi:<ba-add>idx:0 (ifx:0, ec:56:23:e9:7e:f0), tid:6, ssn:1804, winSize:64
I (6500) ESP_COZE_CHAT: Request conversation_id : 74970793********
I (6500) ESP_COZE_CHAT: WS start updata chat
I (6501) ESP_COZE_CHAT: Update chat: {
        "id":   "0a7c8af3-e729-56e7-47b1-ecb3a8adb149",
        "event_type":   "chat.update",
        "data": {
                "chat_config":  {
                        "auto_save_history":    true,
                        "conversation_id":      "74970793********",
                        "user_id":      "userid_123",
                        "meta_data":    {
                        },
                        "custom_variables":     {
                        },
                        "extra_params": {
                        },
                        "parameters":   {
                                "custom_var_1": "测试"
                        }
                },
                "input_audio":  {
                        "format":       "pcm",
                        "codec":        "pcm",
                        "sample_rate":  16000,
                        "channel":      1,
                        "bit_depth":    16
                },
                "turn_detection":       {
                        "type": "server_vad",
                        "prefix_padding_ms":    600,
                        "silence_duration_ms":  500
                },
                "output_audio": {
                        "codec":        "opus",
                        "opus_config":  {
                                "bitrate":      16000,
                                "frame_size_ms":        60,
                                "limit_config": {
                                        "period":       1,
                                        "max_frame_num":        18
                                }
                        },
                        "speech_rate":  20,
                        "voice_id":     "7426720361733144585"
                },
                "event_subscriptions":  ["conversation.audio.delta", "conversation.chat.completed", "input_audio_buffer.speech_started", "input_audio_buffer.speech_stopped", "chat.created", "error", "conversation.message.delta"]
        }
}
I (6607) ES8311: Work in Slave mode
E (6611) i2s_common: i2s_channel_disable(1216): the channel has not been enabled yet
I (6611) I2S_IF: channel mode 0 bits:32/32 channel:2 mask:3
I (6614) I2S_IF: STD Mode 1 bits:32/32 channel:2 sample_rate:16000 mask:3
I (6635) Adev_Codec: Open codec device OK
I (6638) ES7210: Work in Slave mode
I (6645) ES7210: Enable ES7210_INPUT_MIC1
I (6647) ES7210: Enable ES7210_INPUT_MIC2
I (6650) ES7210: Enable ES7210_INPUT_MIC3
I (6653) ES7210: Enable TDM mode
E (6659) i2s_common: i2s_channel_disable(1216): the channel has not been enabled yet
I (6659) I2S_IF: channel mode 0 bits:32/32 channel:2 mask:3
I (6661) I2S_IF: STD Mode 0 bits:32/32 channel:2 sample_rate:16000 mask:3
I (6668) ES7210: Bits 16
I (6676) ES7210: Enable ES7210_INPUT_MIC1
I (6680) ES7210: Enable ES7210_INPUT_MIC2
I (6683) ES7210: Enable ES7210_INPUT_MIC3
I (6686) ES7210: Enable TDM mode
I (6692) ES7210: Unmuted
I (6693) Adev_Codec: Open codec device OK
I (6693) ESP_GMF_BLOCK: The block buf:0x3c25d5dc, end:0x3c2625dc
I (6695) NEW_DATA_BUS: New block buf, num:1, item_cnt:20480, db:0x3c2625e0
I (6702) ESP_GMF_TASK: Waiting to run... [tsk:http-0x3fcd3bf0, wk:0x0, run:0]
I (6709) ESP_GMF_TASK: Waiting to run... [tsk:http-0x3fcd3bf0, wk:0x3c263e6c, run:0]
I (6717) ESP_GMF_BLOCK: The block buf:0x3c263fb8, end:0x3c268fb8
I (6722) NEW_DATA_BUS: New block buf, num:1, item_cnt:20480, db:0x3c268fbc
I (6729) ESP_GMF_TASK: Waiting to run... [tsk:http-0x3fcc9aa4, wk:0x0, run:0]
I (6735) ESP_GMF_TASK: Waiting to run... [tsk:http-0x3fcc9aa4, wk:0x3c26a848, run:0]
W (6745) AUD_SDEC_REG: Overwrote ES decoder 6
W (6748) AUD_SDEC_REG: Overwrote ES decoder 7
W (6753) AUD_SDEC_REG: Overwrote ES decoder 8
I (6758) ASP_POOL: Dest rate:16000
I (6761) ASP_POOL: Dest channels:2
I (6764) ASP_POOL: Dest bits:32
I (6767) ESP_GMF_TASK: Waiting to run... [tsk:TSK_0x3fcca54c-0x3fcca54c, wk:0x0, run:0]
I (6775) MODEL_LOADER: The storage free size is 22208 KB
I (6780) MODEL_LOADER: The partition size is 5168 KB
I (6785) MODEL_LOADER: Successfully load srmodels
I (6789) AFE_CONFIG: Set WakeNet Model: wn9_hilexin

/********** General AFE (Audio Front End) Parameter **********/
pcm_config.total_ch_num: 4
pcm_config.mic_num: 2: [ ch1, ch3 ]
pcm_config.ref_num: 1: [ ch0 ]
pcm_config.sample_rate: 16000
afe_type: SR
afe_mode: HIGH PERF
afe_perferred_core: 0
afe_perferred_priority: 5
afe_ringbuf_size: 50
memory_alloc_mode: 3
afe_linear_gain: 1.0
debug_init: false
fixed_first_channel: false

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
vad_init: true
vad_mode: 3
vad_model_name: NULL
vad_min_speech_ms: 64
vad_min_noise_ms: 1000
vad_delay_ms: 128
vad_mute_playback: false
vad_enable_channel_trigger: false

/********** WakeNet (Wake Word Engine) **********/
wakenet_init: false
wakenet_model_name: wn9_hilexin
wakenet_model_name_2: NULL
wakenet_mode: 0

/********** AGC (Automatic Gain Control) **********/
agc_init: false
agc_mode: WAKENET
agc_compression_gain_db: 9
agc_target_level_dbfs: 9

/**************************************************/
I (6993) AFE: AFE Version: (2MIC_V250113)
I (6994) AFE: Input PCM Config: total 4 channels(2 microphone, 1 playback), sample rate:16000
I (6997) AFE: AFE Pipeline: [input] -> |AEC(SR_HIGH_PERF)| -> |SE(BSS)| -> |VAD(WebRTC)| -> [output]
I (7007) AFE_MANAGER: Feed task, ch 4, chunk 1024, buf size 8192
I (7012) GMF_AFE: Create AFE, gmf_afe-0x3c2ec720
I (7017) GMF_AFE: Create AFE, gmf_afe-0x3c2ec864
I (7021) GMF_AFE: New an object,gmf_afe-0x3c2ec864
I (7026) ESP_GMF_TASK: Waiting to run... [tsk:TSK_0x3fce1714-0x3fce1714, wk:0x0, run:0]
I (7033) ESP_GMF_TASK: Waiting to run... [tsk:TSK_0x3fce1714-0x3fce1714, wk:0x3c2ec9e4, run:0]
Build fst from commands.
Quantized MultiNet6:rnnt_ctc_1.0, name:mn6_cn, (Feb 18 2025 12:00:53)
Quantized MultiNet6 search method: 2, time out:5.8 s
I (8298) NEW_DATA_BUS: New ringbuffer:0x3c4cdb30, num:2, item_cnt:8192, db:0x3c7176f4
I (8301) NEW_DATA_BUS: New ringbuffer:0x3c4c95e0, num:1, item_cnt:20480, db:0x3c4cd9e8
I (8308) AFE_MANAGER: AFE manager suspend 1
I (8312) AFE_MANAGER: AFE manager suspend 0
I (8316) ESP_GMF_TASK: One times job is complete, del[wk:0x3c2ec9e4,ctx:0x3c2ec864, label:gmf_afe_open]
I (8325) ESP_GMF_PORT: ACQ IN, new self payload:0x3c70a0e4, port:0x3c2ec9a4, el:0x3c2ec864-gmf_afe
I (8350) ASP_POOL: Dest rate:16000
I (8350) ASP_POOL: Dest channels:2
I (8351) ASP_POOL: Dest bits:32
I (8351) ESP_GMF_TASK: Waiting to run... [tsk:TSK_0x3fce5534-0x3fce5534, wk:0x0, run:0]
I (8352) AUD_SIMP_PLAYER: Reconfig decoder by music info, rate:16000, channels:1, bits:16, bitrate:0
E (8365) ESP_GMF_AUDIO_HELPER: IS HERE, /coze.opus
I (8370) ESP_GMF_TASK: Waiting to run... [tsk:TSK_0x3fce5534-0x3fce5534, wk:0x3c728b54, run:0]
W (8378) AUD_SDEC: Not find default parser for 9
I (8378) main_task: Returned from app_main()
W (8384) ESP_OPUS_DEC: Frame duration is not set, out pcm buffer size is counted as the length of 60ms.
I (8397) ESP_GMF_TASK: One times job is complete, del[wk:0x3c728b54,ctx:0x3c71ffe8, label:aud_simp_dec_open]
I (8407) ESP_GMF_PORT: ACQ IN, new self payload:0x3c728b54, port:0x3c72f554, el:0x3c71ffe8-aud_simp_dec
I (8527) ESP_GMF_PORT: ACQ OUT, new self payload:0x3c720374, port:0x3c2ec964, el:0x3c2ec864-gmf_afe
```

## Planned Features

1.Support for Opus encoding and audio transmission to the server

2.Support for multiple functional modes, such as enabling the speaker to perform mixing functionalities
