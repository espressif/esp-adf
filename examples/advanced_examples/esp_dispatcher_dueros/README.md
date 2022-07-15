# ESP Dispatcher with DuerOS

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example shows how to use the ADF ESP dispatcher API to connect to DuerOS as well as how to use a high-level API (`esp_audio_`) to build a player. It is recommended to use high-level APIs to build projects.


## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration


The default board for this example is `ESP32-Lyrat V4.3`. if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information first. Go to `menuconfig> Example Configuration` and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```c
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

BluFi has undergone iterations in the ESP-IDF version update, so different versions such as ESP-IDF v3.3 and ESP-IDF v4.x need to be configured differently. For details, please check the following table:

|  Configuration Enable (Dual-Mode or BLE Only)  | ESP-IDF <br> Release/v3.3 |  ESP-IDF <br> Release/v4.1| ESP-IDF <br> Release/v4.2| ESP-IDF <br> Release/v4.3| ESP-IDF <br> Release/v4.4 |
|:----------- |:---------------------: | :---------------------:| :---------------------:|:---------------------: | :---------------------:|
| Bluedroid  BluFi  | CONFIG_BT_ENABLED(=y) <br> CONFIG_BLUEDROID_ENABLED(=y) | CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_BLE_ENABLED(=y) <br>  CONFIG_BT_BLE_BLUFI_ENABLE(=y) | CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_BLE_ENABLED(=y) <br>  CONFIG_BT_BLE_BLUFI_ENABLE(=y)  |CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_BLE_ENABLED(=y) <br>  CONFIG_BT_BLE_BLUFI_ENABLE(=y)  |CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_BLE_ENABLED(=y) <br>  CONFIG_BT_BLE_BLUFI_ENABLE(=y) |
| Nimble  BluFi  | ![alt text](../../../docs/_static/no-icon.png "not supported")  | ![alt text](../../../docs/_static/no-icon.png "not supported")  | ![alt text](../../../docs/_static/no-icon.png "not supported") | CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_NIMBLE_ENABLED(=y) <br>  CONFIG_BT_NIMBLE_BLUFI_ENABLE(=y) | CONFIG_BT_ENABLED(=y) <br>  CONFIG_BT_BLUEDROID_ENABLED(=y) <br>  CONFIG_BT_NIMBLE_ENABLED(=y) <br>  CONFIG_BT_NIMBLE_BLUFI_ENABLE(=y) |

**NOTE:**
-  Lable ![alt text](../../../docs/_static/no-icon.png) means not supported on current version.



### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- After the example starts running, please try to connect to the preset Wi-Fi hotspot first. After successfully obtaining the IP, the log is as follows:

```c
I (3162) REC_ENG: state idle
----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:00:34                     |
------------------------------------------------------------------------------
I (3206) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4183360 Bytes, Inter:160540 Bytes, Dram:131332 Bytes

I (3220) MP3_DECODER: MP3 init
W (3222) I2S: I2S driver already installed
I (3227) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (3234) AUDIO_SETUP: Func:setup_player, Line:123, MEM Total:4172724 Bytes, Inter:155644 Bytes, Dram:126436 Bytes

I (3250) AUDIO_SETUP: esp_audio instance is:0x3f81d7d4
I (3256) DISPATCHER_DUEROS: [Step 6.1] Register wanted player execution type
I (3264) DISPATCHER_DUEROS: [Step 7.0] Initialize dueros service
I (3271) DISPATCHER_DUEROS: [Step 7.1] Register dueros service execution type
W (3278) DISPATCHER: The 1002 index of function already exists
I (3296) DISPATCHER_DUEROS: [Step 8.0] Initialize Done
I (3590) esp_netif_handlers: sta ip: 192.168.1.194, mask: 255.255.255.0, gw: 192.168.1.1
I (3592) WIFI_SERV: Got ip:192.168.1.194
W (3594) WIFI_SERV: STATE type:2, pdata:0x0, len:0
```

- You can also reconfigure the Wi-Fi network. Press and hold the `SET` button for more than three seconds to enter the network configuration mode. At this time, the green light of the development board will flash. Then use Espressif's `EspBluFi` Bluetooth network configuration tool to search for the device broadcasts `BLUFI_DEVICE`. After linking, click `Configure Network`, the `Blufi` protocol will automatically configure the `Wi-Fi` link information. After successfully obtaining the IP, the log is as follows:

```c
I (194512) BLUFI_CONFIG: blufi_config_start
I (194512) WIFI_SERV: SYSTEM_EVENT_STA_DISCONNECTED, reason is WIFI_SERV_STA_BY_USER
I (194522) BLUFI_CONFIG: BLUFI VERSION 0102
W (194532) WIFI_SERV: STATE type:3, pdata:0x0, len:0
I (194532) DISPATCHER: EXE IN, cmd type:2, index:4007, data:0x0, len:0
I (194542) DUER_ACTION: dueros_action_disconnect
I (194542) DISPATCHER: EXE OUT,result type:2, index:4007, ret:0, data:0x0, len:0
I (194552) DISPATCHER: EXE IN, cmd type:2, index:3005, data:0x0, len:0
I (194562) DIS_ACTION: display_action_wifi_connected
I (194572) DISPATCHER: EXE OUT,result type:2, index:3005, ret:0, data:0x0, len:0
I (195072) DISPATCHER_DUEROS: [ * ] [Set] input key event,4
I (203232) BLUFI_CONFIG: BLUFI ble connect, func:wifi_ble_event_callback, line:147
I (207982) BLUFI_CONFIG: BLUFI Set WIFI opmode 1
I (208012) BLUFI_CONFIG: Recv STA SSID esp-office-2.4G
I (208062) BLUFI_CONFIG: Recv STA PASSWORD ***********
I (208092) BLUFI_CONFIG: BLUFI requset wifi connect to AP
I (208092) BLUFI_CONFIG: blufi_config_stop
I (208102) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
I (208102) wifi:state: init -> auth (b0)
I (209102) wifi:state: auth -> init (200)
I (209102) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
I (209102) WIFI_SERV: STA Auth Error, reason:2
W (209102) WIFI_SERV: STATE type:3, pdata:0x0, len:0
W (209112) WIFI_SERV: Disconnect reason 2
I (209112) DISPATCHER: EXE IN, cmd type:2, index:4007, data:0x0, len:0
I (209122) DUER_ACTION: dueros_action_disconnect
I (209132) DISPATCHER: EXE OUT,result type:2, index:4007, ret:0, data:0x0, len:0
I (209142) DISPATCHER: EXE IN, cmd type:2, index:3005, data:0x0, len:0
I (209142) DIS_ACTION: display_action_wifi_connected
I (209152) DISPATCHER: EXE OUT,result type:2, index:3005, ret:0, data:0x0, len:0
I (209172) WIFI_SERV: Connect to wifi ssid: esp32, pwd: esp123456
I (209182) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (209182) wifi:state: init -> auth (b0)
I (209192) wifi:state: auth -> assoc (0)
I (209202) wifi:state: assoc -> run (10)
I (209242) wifi:connected with esp32, aid = 2, channel 6, BW20, bssid = fc:ec:da:b7:11:c7
I (209242) wifi:security: WPA2-PSK, phy: bgn, rssi: -43
I (209242) wifi:pm start, type: 1

W (209242) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
W (209252) wifi:<ba-add>idx:0 (ifx:0, fc:ec:da:b7:11:c7), tid:0, ssn:0, winSize:64
I (209272) wifi:AP's beacon interval = 102400 us, DTIM period = 3
W (210332) wifi:<ba-add>idx:1 (ifx:0, fc:ec:da:b7:11:c7), tid:4, ssn:1, winSize:64
I (211312) esp_netif_handlers: sta ip: 192.168.5.240, mask: 255.255.255.0, gw: 192.168.5.1
I (211312) WIFI_SERV: Got ip:192.168.5.240
W (211312) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (211342) DISPATCHER: EXE IN, cmd type:2, index:4008, data:0x0, len:0
I (211342) DUER_ACTION: dueros_action_connect
I (211342) DISPATCHER: EXE OUT,result type:2, index:4008, ret:0, data:0x0, len:0
E (211342) DUEROS: Recv Que DUER_CMD_LOGIN
```


- After the Device successfully obtains the IP address, the program will use the built-in `$ADF_PATH/components/dueros_service/duer_profile` to connect to the DuerOS server for authentication. After successfully connected, DuerOS will wait for speech commands. The log is as follows:

```c
I (3609) DISPATCHER: EXE IN, cmd type:2, index:4008, data:0x0, len:0
I (3610) DUER_ACTION: dueros_action_connect
I (3611) DISPATCHER: EXE OUT,result type:2, index:4008, ret:0, data:0x0, len:0
I (3619) DISPATCHER: EXE IN, cmd type:2, index:3005, data:0x0, len:0
I (3626) DIS_ACTION: display_action_wifi_connected
I (3632) DISPATCHER: EXE OUT,result type:2, index:3005, ret:0, data:0x0, len:0
I (2842,tid:3ffb7b9c) lightduer_session.c(  44):random = 75933
E (4401) DUEROS: Recv Que DUER_CMD_LOGIN
I (4401) DUEROS: duer_start, len:1469
{"configures":"{}","bindToken":"a8c828082478df3febae70a848d4f0d7","coapPort":443,"token":"UdxwaacTY4D0kwujg5vT5xsHaaLNqDZh","serverAddr":"device.iot.baidu.com","lwm2mPort":443,"uuid":"18c10000000014","rsaCaCrt":"-----BEGIN CERTIFICATE-----\npjqPJPqrZvW0w0cmYsYibNDy985JB87MJMfJVESG/v0Y/YbvcnRoi5gAenWXQNL4\nh2hf08A5wEQfLO/EaD1GTH3OIierKYZ6GItGrz4uFKHV5fTMiflABCdu37ALGjrA\nrIjwjxQG6WwLr9468hkKrWNG3dMBHKvmqO8x42sZOFRJMkqBbKzaBd1uW4xY5XwM\nS1QX56tVrgO0A3S+4dEg5uiLVN4YVP/Vqh4SMtYkL7ZZiZAxD9GtNnhRyFsWlC2r\nOVSdXs1ttZxEaEBGUl7tgsBte556BIvufZX+BXGyycVJdBu3\n-----END CERTIFICATE-----\n","macId":"","version":11581}
E (6686) DUER_AUDIO: duer_dcs_get_speaker_state
I (6690) DISPATCHER: EXE IN, cmd type:2, index:1008, data:0x0, len:0
I (6698) DCS_WRAPPER: duer_dcs_action_get_state, vol:60, 0x3f827004
I (6704) DISPATCHER: EXE OUT,result type:2, index:1008, ret:0, data:0x3f827004, len:4
E (6716) DUEROS: event: DUER_EVENT_STARTED
I (6717) DUEROS: Dueros DUER_CMD_CONNECTED, duer_state:2
W (5160,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 401416D0, timespent = 125
I (5184,tid:3ffb7b9c) lightduer_connagent.c( 222):add resource successfully!!
W (6743) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:3, data_len:0
I (5191,tid:3ffb7b9c) lightduer_connagent.c( 222):add resource successfully!!
W (5205,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (5218,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (5223,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (5245,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (5261,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (5268,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (5283,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1

```

- At this point, you can say the wake word "Hi, Le Xin", and then say the command word "How is the weather in Suzhou?”. The server will reply with the current weather condition in speech. The log is as follows:

```c
W (844084) REC_ENG: ### spot keyword ###
I (844085) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (844085) DISPATCHER: EXE IN, cmd type:2, index:1002, data:0x0, len:0
I (844092) PLAYER_ACTION: player_action_pause
W (844097) ESP_AUDIO_CTRL: [media_ctrl_pause]-It's not running status[0], can't puase
I (844106) DISPATCHER: EXE OUT,result type:2, index:1002, ret:81009, data:0x0, len:0
I (844114) DISPATCHER: EXE IN, cmd type:2, index:3001, data:0x0, len:0
I (844122) DIS_ACTION: display_action_turn_on
I (844127) DISPATCHER: EXE OUT,result type:2, index:3001, ret:0, data:0x0, len:0
W (844863) REC_ENG: State VAD silence_time >= vad_off_window
I (845136) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (845137) DUEROS: Recv Que DUER_CMD_START
E (845139) DUER_AUDIO: duer_dcs_get_speaker_state
I (845142) DISPATCHER: EXE IN, cmd type:2, index:1008, data:0x0, len:0
I (845151) DCS_WRAPPER: duer_dcs_action_get_state, vol:60, 0x3f827004
I (845157) DISPATCHER: EXE OUT,result type:2, index:1008, ret:0, data:0x3f827004, len:4
I (843608,tid:3ffbe570) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000001440b18ccf000cdf5800000003
W (845177) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:4, data_len:0
W (847293) REC_ENG: State VAD silence_time >= vad_off_window
I (847293) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (847296) DUEROS: Dueros DUER_CMD_STOP
W (847302) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:5, data_len:0
I (846434,tid:3ffb7b9c) lightduer_dcs_router.c( 460):Directive name: StopListen
I (847992) DUER_AUDIO: stop_listen, close mic
I (847994) DISPATCHER: EXE IN, cmd type:2, index:2002, data:0x0, len:0
I (848001) REC_ACTION: recorder_action_rec_wav_turn_off
I (848006) REC_ENG: Recorder trigger stop
I (848011) DISPATCHER: EXE OUT,result type:2, index:2002, ret:0, data:0x0, len:0
E (848017) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
I (846468,tid:3ffb7b9c) W (848027) REC_ENG: State WAKEUP_END
lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (848034) DISPATCHER: EXE IN, cmd type:2, index:3002, data:0x0, len:0
I (848047) DIS_ACTION: display_action_turn_off
I (848052) DISPATCHER: EXE OUT,result type:2, index:3002, ret:0, data:0x0, len:0
I (848060) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
W (846508,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:3, qcache_len:1
I (848067) REC_ENG: state idle
W (846522,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (846543,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
I (847117,tid:3ffb8004) lightduer_dcs_router.c( 460):Directive name: Speak
I (848677) DUER_AUDIO: Playing speak: 0x3f82849c, http://res.iot.baidu.com:80/api/v1/tts/vIA0-xlSrBG4Q2olj2U4_XCPKxgCXJx0SP_rfxLLf_tZ2oIr0nnis2NCIaZMzAxLOiqQsoLQHOOUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3
I (848689) DISPATCHER: EXE IN, cmd type:2, index:4002, data:0x3f82849c, len:150
I (848707) DCS_WRAPPER: duer_dcs_action_speak, 0x3f82849c, http://res.iot.baidu.com:80/api/v1/tts/vIA0-xlSrBG4Q2olj2U4_XCPKxgCXJx0SP_rfxLLf_tZ2oIr0nnis2NCIaZMzAxLOiqQsoLQHOOUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3, 150
I (848724) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (848730) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (848736) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1bfc
I (848744) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4094864 Bytes, Inter:120064 Bytes, Dram:90856 Bytes

I (848755) ESP_AUDIO_TASK: It's a decoder
I (848760) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (848771) ESP_AUDIO_TASK: 2.Handles,IN:0x3f81dfb8,CODEC:0x3f81ea18,FILTER:0x3f826c00,OUT:0x3f81ef1c
I (848780) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (848786) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (848792) AUDIO_PIPELINE: link el->rb, el:0x3f81dfb8, tag:IN_http, rb:0x3f826dd8
I (848800) AUDIO_PIPELINE: link el->rb, el:0x3f81ea18, tag:DEC_mp3, rb:0x3f8303c4
I (848808) AUDIO_PIPELINE: link el->rb, el:0x3f826c00, tag:Audio_forge, rb:0x3f8323f0
I (848816) ESP_AUDIO_TASK: 3. Previous starting...
I (848849) AUDIO_ELEMENT: [IN_http-0x3f81dfb8] Element task created
I (848850) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (848996) AUDIO_ELEMENT: [DEC_mp3-0x3f81ea18] Element task created
I (848997) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (849017) MP3_DECODER: MP3 opened
I (849018) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (851947) HTTP_STREAM: total_bytes=0
I (851948) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f81dfb8] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858808) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f81ea18] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858811) ESP_AUDIO_TASK: Received music info then on play
I (858815) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (858822) AUDIO_ELEMENT: [Audio_forge-0x3f826c00] Element task created
I (858829) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (858923) AUDIO_FORGE: audio_forge opened
I (858924) AUDIO_ELEMENT: [OUT_iis-0x3f81ef1c] Element task created
I (858925) AUDIO_FORGE: audio_forge reopen
I (858929) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (858940) I2S_STREAM: AUDIO_STREAM_WRITER
I (858945) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f826c00] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858957) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f81ef1c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858968) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:802, is_stopping:0
I (858980) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3977296 Bytes, Inter:94132 Bytes, Dram:64924 Bytes
W (903937) HTTP_STREAM: No more data,errno:0, total_bytes:20594, rlen = 0
I (903938) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (903946) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f81dfb8] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (904391) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (905536) MP3_DECODER: Closed
I (905539) ESP_AUDIO_TASK: Received last pos: 20592 bytes
I (905541) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f81ea18] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905804) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (905805) AUDIO_FORGE: audio forge closed
I (905805) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f826c00] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905816) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (905854) ESP_AUDIO_TASK: Received last time: 46800 ms
I (905855) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f81ef1c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905862) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:802, is_stopping:0
I (905871) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4045864 Bytes, Inter:97444 Bytes, Dram:68236 Bytes

W (905883) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (905929) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
E (905930) DISPATCHER_DUEROS: ESP_AUDIO_CALLBACK_FUNC, st:4,err:0,src:802
I (904375,tid:3ffb8004) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
W (905163,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (905166,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
```

### Example Log
A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 15:10:57
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 80MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x11586c (1136748) map
I (461) esp_image: segment 1: paddr=0x00125894 vaddr=0x3ffb0000 size=0x0398c ( 14732) load
I (466) esp_image: segment 2: paddr=0x00129228 vaddr=0x40080000 size=0x06df0 ( 28144) load
0x40080000: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (478) esp_image: segment 3: paddr=0x00130020 vaddr=0x400d0020 size=0x130de8 (1248744) map
0x400d0020: _stext at ??:?

I (879) esp_image: segment 4: paddr=0x00260e10 vaddr=0x40086df0 size=0x11fd4 ( 73684) load
0x40086df0: lmacAdjustTimestamp at ??:?

I (921) boot: Loaded app from partition at offset 0x10000
I (921) boot: Disabling RNG early entropy source...
I (921) psram: This chip is ESP32-D0WD
I (926) spiram: Found 64MBit SPI RAM device
I (930) spiram: SPI RAM mode: flash 80m sram 80m
I (936) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (943) cpu_start: Pro cpu up.
I (947) cpu_start: Application information:
I (951) cpu_start: Project name:     esp_dispatcher_dueros
I (958) cpu_start: App version:      v2.2-210-g86396c1f-dirty
I (964) cpu_start: Compile time:     Nov  2 2021 15:22:50
I (970) cpu_start: ELF file SHA256:  9b5aacc30b3efb77...
I (976) cpu_start: ESP-IDF:          v4.2.2
I (981) cpu_start: Starting app cpu, entry point is 0x40081e6c
0x40081e6c: call_start_cpu1 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1479) spiram: SPI SRAM memory test OK
I (1480) heap_init: Initializing. RAM available for dynamic allocation:
I (1480) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1485) heap_init: At 3FFB6450 len 00029BB0 (166 KiB): DRAM
I (1492) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1498) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1505) heap_init: At 40098DC4 len 0000723C (28 KiB): IRAM
I (1511) cpu_start: Pro cpu start user code
I (1516) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1537) spi_flash: detected chip: gd
I (1537) spi_flash: flash io: dio
W (1537) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1548) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1563) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1589) DISPATCHER_DUEROS: ADF version is v2.2-210-g86396c1f-dirty
I (1589) DISPATCHER_DUEROS: Step 1. Create dueros_speaker instance
E (1592) DISPATCHER: exe first list: 0x0
I (1597) DISPATCHER: dispatcher_event_task is running...
I (1602) DISPATCHER_DUEROS: [Step 2.0] Create esp_periph_set_handle_t instance and initialize Touch, Button, SDcard
I (1614) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1623) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
W (1658) PERIPH_TOUCH: _touch_init
I (1658) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1704) SDCARD: CID name SD16G!

I (2133) DISPATCHER_DUEROS: [Step 2.1] Initialize input key service
I (2133) DISPATCHER_DUEROS: [Step 3.0] Create display service instance
I (2136) DISPATCHER_DUEROS: [Step 3.1] Register display service execution type
I (2153) DISPATCHER_DUEROS: [Step 4.0] Create Wi-Fi service instance
E (2155) DISPATCHER: exe first list: 0x0
I (2159) DISPATCHER: dispatcher_event_task is running...
I (2170) DISPATCHER_DUEROS: [Step 4.1] Register wanted display service execution type
I (2171) wifi:wifi driver task: 3ffcc620, prio:23, stack:6656, core=0
I (2180) system_api: Base MAC address is not set
I (2185) system_api: read default base MAC address from EFUSE
I (2195) wifi:wifi firmware version: bb6888c
I (2196) wifi:wifi certification version: v7.0
I (2200) wifi:config NVS flash: enabled
I (2204) wifi:config nano formating: disabled
I (2208) wifi:Init data frame dynamic rx buffer num: 32
I (2213) wifi:Init management frame dynamic rx buffer num: 32
I (2218) wifi:Init management short buffer num: 32
I (2223) wifi:Init static tx buffer num: 16
I (2227) wifi:Init tx cache buffer num: 32
I (2230) wifi:Init static rx buffer size: 1600
I (2235) wifi:Init static rx buffer num: 16
I (2238) wifi:Init dynamic rx buffer num: 32
I (2243) wifi_init: rx ba win: 16
I (2246) wifi_init: tcpip mbox: 32
I (2250) wifi_init: udp mbox: 6
I (2254) wifi_init: tcp mbox: 6
I (2258) wifi_init: tcp tx win: 5744
I (2262) wifi_init: tcp rx win: 5744
I (2267) wifi_init: tcp mss: 1440
I (2271) wifi_init: WiFi/LWIP prefer SPIRAM
I (2275) wifi_init: WiFi IRAM OP enabled
I (2280) wifi_init: WiFi RX IRAM OP enabled
I (2286) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2383) wifi:mode : sta (e0:e2:e6:72:9d:40)
I (2400) DISPATCHER_DUEROS: [Step 4.2] Initialize Wi-Fi provisioning type(AIRKISS or SMARTCONFIG)
I (2406) WIFI_SERV: Connect to wifi ssid: shootao1, pwd: esp123456
I (2407) DISPATCHER_DUEROS: [Step 5.0] Initialize recorder engine
I (2412) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
I (2897) wifi:state: init -> auth (b0)
I (2901) wifi:state: auth -> assoc (0)
I (2907) wifi:state: assoc -> run (10)
I (2917) wifi:connected with shootao1, aid = 2, channel 1, BW20, bssid = 48:7d:2e:c9:a0:a5
I (2917) wifi:security: WPA2-PSK, phy: bgn, rssi: -18
I (2919) wifi:pm start, type: 1

I (2923) DISPATCHER_DUEROS: [Step 5.1] Register wanted recorder execution type
I (2924) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
W (2924) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2937) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2930) DISPATCHER_DUEROS: [Step 6.0] Initialize esp player
I (2961) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (2965) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (2967) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
E (2973) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2988) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (3001) AUDIO_PIPELINE: link el->rb, el:0x3f818d8c, tag:i2s, rb:0x3f81926c
I (3008) AUDIO_PIPELINE: link el->rb, el:0x3f818fe4, tag:filter, rb:0x3f81b2ac
I (3016) AUDIO_ELEMENT: [i2s-0x3f818d8c] Element task created
I (3022) AUDIO_ELEMENT: [raw-0x3f819104] Element task created
I (3024) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (3038) ES8388_DRIVER: init,out:02, in:00
I (3043) AUDIO_ELEMENT: [filter-0x3f818fe4] Element task created
I (3050) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4217248 Bytes, Inter:192992 Bytes, Dram:163784 Bytes

I (3060) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (3062) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (3073) I2S_STREAM: AUDIO_STREAM_READER,Rate:48000,ch:2
I (3094) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (3098) AUDIO_ELEMENT: [filter] AEL_MSG_CMD_RESUME,state:1
I (3104) AUDIO_PIPELINE: Pipeline started
I (3108) AUDIO_SETUP: Recorder has been created
I (3107) RSP_FILTER: sample rate of source data : 48000, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
Quantized wakeNet5: wakeNet5X3_v5_hilexin_5_0.97_0.90, mode:0 (Oct 14 2020 16:26:17)
I (3130) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f81d7d4
I (3136) REC_ENG: ESP SR Engine, chunksize is 480， FRAME_SIZE:960, frequency:16000
E (3151) REC_ENG: Recorder Engine Running ..., vad_window=9, wakeup=10000 ms, vad_off=800 ms, threshold=90 ms, sensitivity=0

I (3162) REC_ENG: state idle
----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:00:34                     |
------------------------------------------------------------------------------
I (3206) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4183360 Bytes, Inter:160540 Bytes, Dram:131332 Bytes

I (3220) MP3_DECODER: MP3 init
W (3222) I2S: I2S driver already installed
I (3227) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (3234) AUDIO_SETUP: Func:setup_player, Line:123, MEM Total:4172724 Bytes, Inter:155644 Bytes, Dram:126436 Bytes

I (3250) AUDIO_SETUP: esp_audio instance is:0x3f81d7d4
I (3256) DISPATCHER_DUEROS: [Step 6.1] Register wanted player execution type
I (3264) DISPATCHER_DUEROS: [Step 7.0] Initialize dueros service
I (3271) DISPATCHER_DUEROS: [Step 7.1] Register dueros service execution type
W (3278) DISPATCHER: The 1002 index of function already exists
I (3296) DISPATCHER_DUEROS: [Step 8.0] Initialize Done
I (3590) esp_netif_handlers: sta ip: 192.168.1.194, mask: 255.255.255.0, gw: 192.168.1.1
I (3592) WIFI_SERV: Got ip:192.168.1.194
W (3594) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (3609) DISPATCHER: EXE IN, cmd type:2, index:4008, data:0x0, len:0
I (3610) DUER_ACTION: dueros_action_connect
I (3611) DISPATCHER: EXE OUT,result type:2, index:4008, ret:0, data:0x0, len:0
I (3619) DISPATCHER: EXE IN, cmd type:2, index:3005, data:0x0, len:0
I (3626) DIS_ACTION: display_action_wifi_connected
I (3632) DISPATCHER: EXE OUT,result type:2, index:3005, ret:0, data:0x0, len:0
I (2842,tid:3ffb7b9c) lightduer_session.c(  44):random = 75933
E (4401) DUEROS: Recv Que DUER_CMD_LOGIN
I (4401) DUEROS: duer_start, len:1469
{"configures":"{}","bindToken":"a8c828082478df3febae70a848d4f0d7","coapPort":443,"token":"UdxwaacTY4D0kwujg5vT5xsHaaLNqDZh","serverAddr":"device.iot.baidu.com","lwm2mPort":443,"uuid":"18c10000000014","rsaCaCrt":"-----BEGIN CERTIFICATE-----\nMIIDUDCCAjgCCQCmVPUErMYmCjANBgkqhkiG9w0BAQUFADBqMQswCQYDVQQGEwJD\nTjETMBEGA1UECAwKU29tZS1TdGF0ZTEOMAwGA1UECgwFYmFpZHUxGDAWBgNVBAMM\nDyouaW90LmJhaWR1LmNvbTEcMBoGCSqGSIb3DQEJARYNaW90QGJhaWR1LmNvbTAe\nFw0xNjAzMTEwMzMwNDlaFw0yNjAzMDkwMzMwNDlaMGoxCzAJBgNVBAYTAkNOMRMw\nEQYDVQQIDApTb21lLVN0YXRlMQ4wDAYDVQQKDAViYWlkdTEYMBYGA1UEAwwPKi5p\nb3QuYmFpZHUuY29tMRwwGgYJKoZIhvcNAQkBFg1pb3RAYmFpZHUuY29tMIIBIjAN\nBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtbhIeiN7pznzuMwsLKQj2xB02+51\nOvCJ5d116ZFLjecp9qtllqOfN7bm+AJa5N2aAHJtsetcTHMitY4dtGmOpw4dlGqx\nluoz50kWJWQjVR+z6DLPnGE4uELOS8vbKHUoYPPQTT80eNVnl9S9h/l7DcjEAJYC\nIYJbf6+K9x+Ti9VRChvWcvgZQHMRym9j1g/7CKGMCIwkC+6ihkGD/XG40r7KRCyH\nbD53KnBjBO9FH4IL3rGlZWKWzMw3zC6RTS2ekfEsgAtYDvROKd4rNs+uDU9xaBLO\ndXTl5uxgudH2VnVzWtj09OUbBtXcQFD2IhmOl20BrckYul+HEIMR0oDibwIDAQAB\nMA0GCSqGSIb3DQEBBQUAA4IBAQCzTTH91jNh/uYBEFekSVNg1h1kPSujlwEDDf/W\npjqPJPqrZvW0w0cmYsYibNDy985JB87MJMfJVESG/v0Y/YbvcnRoi5gAenWXQNL4\nh2hf08A5wEQfLO/EaD1GTH3OIierKYZ6GItGrz4uFKHV5fTMiflABCdu37ALGjrA\nrIjwjxQG6WwLr9468hkKrWNG3dMBHKvmqO8x42sZOFRJMkqBbKzaBd1uW4xY5XwM\nS1QX56tVrgO0A3S+4dEg5uiLVN4YVP/Vqh4SMtYkL7ZZiZAxD9GtNnhRyFsWlC2r\nOVSdXs1ttZxEaEBGUl7tgsBte556BIvufZX+BXGyycVJdBu3\n-----END CERTIFICATE-----\n","macId":"","version":11581}k3����?���?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?o�?��{?�ht?Eri?{[?ۄI?�5?J�?Ap
I (3181,tid:3ffb7b9c) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82032C, length:1469, profile:3F820F14
I (3188,tid:3ffb7b9c) lightduer_ca_conf.c(  38):    duer_conf_get_string: uuid = 18c10000000014
I (3195,tid:3ffb7b9c) lightduer_ca_conf.c(  38):    duer_conf_get_string: serverAddr = device.iot.baidu.com
W (4742) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:2, data_len:0
I (3245,tid:3ffb7b9c) baidu_ca_socket_adp.c( 139):DNS lookup succeeded. IP=180.97.33.165
I (3280,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (3281,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 40141510, timespent = 100
I (4713,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (4713,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 401416D0, timespent = 1384
I (4723,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4733,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4765,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4768,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4773,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4780,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4790,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4797,tid:3ffb7b9c) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82032C, length:0, profile:00000000
I (4809,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4825,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4826,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4832,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (4844,tid:3ffb7b9c) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (5042,tid:3ffb7b9c) lightduer_voice.c( 915):Mutex initializing
I (5043,tid:3ffb7b9c) lightduer_connagent.c( 189):connect started!
I (5045,tid:3ffb7b9c) lightduer_ds_log_cache.c(  85):no cache report
E (6607) DUEROS: event: 0
I (6610) DUEROS: duer_dcs_init
I (5061,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.private.protocol
I (5065,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen_extended_card
I (5074,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen
I (5083,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.system
I (5093,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_input
I (5100,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_output
I (5109,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.speaker_controller
I (5118,tid:3ffb7b9c) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.audio_player
E (6686) DUER_AUDIO: duer_dcs_get_speaker_state
I (6690) DISPATCHER: EXE IN, cmd type:2, index:1008, data:0x0, len:0
I (6698) DCS_WRAPPER: duer_dcs_action_get_state, vol:60, 0x3f827004
I (6704) DISPATCHER: EXE OUT,result type:2, index:1008, ret:0, data:0x3f827004, len:4
E (6716) DUEROS: event: DUER_EVENT_STARTED
I (6717) DUEROS: Dueros DUER_CMD_CONNECTED, duer_state:2
W (5160,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 401416D0, timespent = 125
I (5184,tid:3ffb7b9c) lightduer_connagent.c( 222):add resource successfully!!
W (6743) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:3, data_len:0
I (5191,tid:3ffb7b9c) lightduer_connagent.c( 222):add resource successfully!!
W (5205,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (5218,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (5223,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (5245,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (5261,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (5268,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (5283,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (305173,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (305175,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (605161,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (605163,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (844084) REC_ENG: ### spot keyword ###
I (844085) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (844085) DISPATCHER: EXE IN, cmd type:2, index:1002, data:0x0, len:0
I (844092) PLAYER_ACTION: player_action_pause
W (844097) ESP_AUDIO_CTRL: [media_ctrl_pause]-It's not running status[0], can't puase
I (844106) DISPATCHER: EXE OUT,result type:2, index:1002, ret:81009, data:0x0, len:0
I (844114) DISPATCHER: EXE IN, cmd type:2, index:3001, data:0x0, len:0
I (844122) DIS_ACTION: display_action_turn_on
I (844127) DISPATCHER: EXE OUT,result type:2, index:3001, ret:0, data:0x0, len:0
W (844863) REC_ENG: State VAD silence_time >= vad_off_window
I (845136) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (845137) DUEROS: Recv Que DUER_CMD_START
E (845139) DUER_AUDIO: duer_dcs_get_speaker_state
I (845142) DISPATCHER: EXE IN, cmd type:2, index:1008, data:0x0, len:0
I (845151) DCS_WRAPPER: duer_dcs_action_get_state, vol:60, 0x3f827004
I (845157) DISPATCHER: EXE OUT,result type:2, index:1008, ret:0, data:0x3f827004, len:4
I (843608,tid:3ffbe570) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000001440b18ccf000cdf5800000003
W (845177) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:4, data_len:0
W (847293) REC_ENG: State VAD silence_time >= vad_off_window
I (847293) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (847296) DUEROS: Dueros DUER_CMD_STOP
W (847302) DISPATCHER_DUEROS: duer_callback: type:0, source:0x3f81f120 data:5, data_len:0
I (846434,tid:3ffb7b9c) lightduer_dcs_router.c( 460):Directive name: StopListen
I (847992) DUER_AUDIO: stop_listen, close mic
I (847994) DISPATCHER: EXE IN, cmd type:2, index:2002, data:0x0, len:0
I (848001) REC_ACTION: recorder_action_rec_wav_turn_off
I (848006) REC_ENG: Recorder trigger stop
I (848011) DISPATCHER: EXE OUT,result type:2, index:2002, ret:0, data:0x0, len:0
E (848017) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
I (846468,tid:3ffb7b9c) W (848027) REC_ENG: State WAKEUP_END
lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (848034) DISPATCHER: EXE IN, cmd type:2, index:3002, data:0x0, len:0
I (848047) DIS_ACTION: display_action_turn_off
I (848052) DISPATCHER: EXE OUT,result type:2, index:3002, ret:0, data:0x0, len:0
I (848060) DISPATCHER_DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
W (846508,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:3, qcache_len:1
I (848067) REC_ENG: state idle
W (846522,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (846543,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
I (847117,tid:3ffb8004) lightduer_dcs_router.c( 460):Directive name: Speak
I (848677) DUER_AUDIO: Playing speak: 0x3f82849c, http://res.iot.baidu.com:80/api/v1/tts/vIA0-xlSrBG4Q2olj2U4_XCPKxgCXJx0SP_rfxLLf_tZ2oIr0nnis2NCIaZMzAxLOiqQsoLQHOOUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3
I (848689) DISPATCHER: EXE IN, cmd type:2, index:4002, data:0x3f82849c, len:150
I (848707) DCS_WRAPPER: duer_dcs_action_speak, 0x3f82849c, http://res.iot.baidu.com:80/api/v1/tts/vIA0-xlSrBG4Q2olj2U4_XCPKxgCXJx0SP_rfxLLf_tZ2oIr0nnis2NCIaZMzAxLOiqQsoLQHOOUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3, 150
I (848724) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (848730) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (848736) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1bfc
I (848744) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4094864 Bytes, Inter:120064 Bytes, Dram:90856 Bytes

I (848755) ESP_AUDIO_TASK: It's a decoder
I (848760) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (848771) ESP_AUDIO_TASK: 2.Handles,IN:0x3f81dfb8,CODEC:0x3f81ea18,FILTER:0x3f826c00,OUT:0x3f81ef1c
I (848780) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (848786) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (848792) AUDIO_PIPELINE: link el->rb, el:0x3f81dfb8, tag:IN_http, rb:0x3f826dd8
I (848800) AUDIO_PIPELINE: link el->rb, el:0x3f81ea18, tag:DEC_mp3, rb:0x3f8303c4
I (848808) AUDIO_PIPELINE: link el->rb, el:0x3f826c00, tag:Audio_forge, rb:0x3f8323f0
I (848816) ESP_AUDIO_TASK: 3. Previous starting...
I (848849) AUDIO_ELEMENT: [IN_http-0x3f81dfb8] Element task created
I (848850) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (848996) AUDIO_ELEMENT: [DEC_mp3-0x3f81ea18] Element task created
I (848997) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (849017) MP3_DECODER: MP3 opened
I (849018) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (851947) HTTP_STREAM: total_bytes=0
I (851948) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f81dfb8] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858808) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f81ea18] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858811) ESP_AUDIO_TASK: Received music info then on play
I (858815) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (858822) AUDIO_ELEMENT: [Audio_forge-0x3f826c00] Element task created
I (858829) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (858923) AUDIO_FORGE: audio_forge opened
I (858924) AUDIO_ELEMENT: [OUT_iis-0x3f81ef1c] Element task created
I (858925) AUDIO_FORGE: audio_forge reopen
I (858929) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (858940) I2S_STREAM: AUDIO_STREAM_WRITER
I (858945) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f826c00] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858957) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f81ef1c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (858968) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:802, is_stopping:0
I (858980) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3977296 Bytes, Inter:94132 Bytes, Dram:64924 Bytes

I (858989) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (858996) DISPATCHER: EXE OUT,result type:2, index:4002, ret:0, data:0x0, len:0
E (858996) DISPATCHER_DUEROS: ESP_AUDIO_CALLBACK_FUNC, st:1,err:0,src:802
E (859003) DUER_AUDIO: Speak ret:0
W (857458,tid:3ffb8004) lightduer_events.c(  81):[lightduer_handler] <== event end = 40143324, timespent = 10342
W (857460,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 401416D0, timespent = 9705
E (891155,tid:3ffb7b9c) lightduer_dcs_router.c( 877):Failed to parse payload
E (892715) DUER_AUDIO: duer_dcs_get_speaker_state
I (892716) DISPATCHER: EXE IN, cmd type:2, index:1008, data:0x0, len:0
I (892724) DCS_WRAPPER: duer_dcs_action_get_state, vol:60, 0x3f8270f0
I (892730) DISPATCHER: EXE OUT,result type:2, index:1008, ret:0, data:0x3f8270f0, len:4
W (891247,tid:3ffb7b9c) lightduer_connagent.c( 471):cached more than 10 message!!
W (891250,tid:3ffb7b9c) lightduer_connagent.c( 471):cached more than 10 message!!
W (891253,tid:3ffb7b9c) lightduer_connagent.c( 471):cached more than 10 message!!
W (891259,tid:3ffb7b9c) lightduer_connagent.c( 471):cached more than 10 message!!
W (891266,tid:3ffb7b9c) lightduer_events.c(  81):[lightduer_ca] <== event end = 401416D0, timespent = 124
W (891302,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:12
W (891302,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:12
W (891312,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:12
W (891322,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:12
W (891332,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:12
W (891355,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:11
W (891364,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:10
W (892886,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:9
W (893056,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:8
W (893080,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:7
W (893217,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:6
W (893382,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:5
W (893660,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (893888,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (893903,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (893988,tid:3ffb7b9c) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (903937) HTTP_STREAM: No more data,errno:0, total_bytes:20594, rlen = 0
I (903938) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (903946) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f81dfb8] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (904391) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (905536) MP3_DECODER: Closed
I (905539) ESP_AUDIO_TASK: Received last pos: 20592 bytes
I (905541) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f81ea18] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905804) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (905805) AUDIO_FORGE: audio forge closed
I (905805) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f826c00] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905816) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (905854) ESP_AUDIO_TASK: Received last time: 46800 ms
I (905855) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f81ef1c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (905862) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:802, is_stopping:0
I (905871) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4045864 Bytes, Inter:97444 Bytes, Dram:68236 Bytes

W (905883) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (905929) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
E (905930) DISPATCHER_DUEROS: ESP_AUDIO_CALLBACK_FUNC, st:4,err:0,src:802
I (904375,tid:3ffb8004) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
W (905163,tid:3ffb7b9c) lightduer_system_info.c( 306):Undefined memory type, 0
E (905166,tid:3ffb7b9c) lightduer_system_info.c( 389):Sys Info: Get disk info failed
```

## Troubleshooting

If your device fails to recognize voice commands after starting running, you can check in the following order:
1. Make sure that the Wi-Fi settings are correct and the device connects to the router successfully and obtains an IP address.
2. Make sure that the duer_profile is correct and is obtained from [Baidu official website](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown).


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
