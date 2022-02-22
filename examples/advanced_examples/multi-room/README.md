# Multi-Room Music example

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")

## Example Brief

ESP Multi-Room Music is a Wi-Fi-based communication protocol to share music among multiple interconnected speakers. Under this protocol, those connected speakers form a Group. They can play music synchronously and are controlled together, which can easily achieve a theater-grade stereo surround sound system.

### Resources

Memory consumption:

| memory_total (byte) | memory_inram (byte) | memory_psram (byte) |
|---------------------|---------------------|---------------------|
| 279632              | 145064              | 134568              |

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Example Set Up

### Use Special IDF Branch

- This example needs to select a branch above IDF `release/v4.2` to compile:
```c
cd $IDF_PATH
git checkout master
git pull
git checkout release/v4.2
git submodule update --init --recursive
```

### Configuration

Open the project configuration menu (`idf.py menuconfig`).

- Select a compatible audio board in `menuconfig` > `Audio HAL`
- Set up Wi-Fi connection in `Example Configuration` > `WiFi SSID` and `WiFi Password`

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- Prepare some audio boards, load and run the example on all of them.
- After all the boards are in `Slave start searching` mode, press the `PLAY/REC` button on one of the boards to start Multi-Room Music playback.

### Example Log

- Slave start searching
```c
I (5451) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f805a60

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.6.6-10-gf944a6b-aca0d7f-2d83f7a               |
|                     Compile date: Mar 15 2021-11:14:21                     |
------------------------------------------------------------------------------
I (5492) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:342, MEM Total:4338832 Bytes, Inter:220956 Bytes, Dram:205124 Bytes

I (5504) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (5510) MRM_EXAMPLE: [ 3 ] Create and start input key service
I (5516) ESP_DECODER: esp_decoder_init, stack size is 30720
I (5522) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (5545) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (5549) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (5554) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (5560) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffd5320
I (5568) ESP_AUDIO_TASK: It's a decoder
I (5572) ESP_AUDIO_TASK: 1.CUR IN:[IN_raw],CODEC:[DEC_auto],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (5583) ESP_AUDIO_TASK: 2.Handles,IN:0x3f806fd0,CODEC:0x3f807254,FILTER:0x3f8075b8,OUT:0x3f807414
I (5592) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (5597) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (5603) AUDIO_PIPELINE: link el->rb, el:0x3f806fd0, tag:IN_raw, rb:0x3f807758
I (5611) AUDIO_PIPELINE: link el->rb, el:0x3f807254, tag:DEC_auto, rb:0x3f809f98
I (5619) AUDIO_PIPELINE: link el->rb, el:0x3f8075b8, tag:Resample, rb:0x3f80bfd8
I (5627) ESP_AUDIO_TASK: 3. Previous starting...
I (5632) AUDIO_ELEMENT: [IN_raw-0x3f806fd0] Element task created
I (5639) AUDIO_THREAD: The DEC_auto task allocate stack on external memory
I (5648) AUDIO_ELEMENT: [DEC_auto-0x3f807254] Element task created
I (5653) AUDIO_ELEMENT: [DEC_auto] AEL_MSG_CMD_RESUME,state:1
I (5660) ESP_AUDIO_TASK: No Blocking play with decoder
I (5665) ESP_AUDIO_TASK: Recv Element[IN_raw-0x3f806fd0] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (5677) ESP_AUDIO_TASK: Recv Element[IN_raw-0x3f806fd0] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (5688) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (5695) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:763, MEM Total:4267572 Bytes, Inter:206344 Bytes, Dram:190512 Bytes

I (5706) ESP_AUDIO_CTRL: Exit play procedure, ret:0
W (5714) MRM_MULTICAST: creat addr 239.255.255.252 port 1900
I (5718) AUDIO_THREAD: The mrm_slave_client task allocate stack on external memory
I (5726) MRM_CLIENT: Slave start searching...
```

- Master playing
```c
I (56177) MRM_EXAMPLE: [ * ] [Play] input key event
I (56734) MRM_CLIENT: slave client task stoped
I (56735) AUDIO_THREAD: The mrm_master_client task allocate stack on external memory
I (58324) HTTP_STREAM: total_bytes=2994349
I (58325) AUDIO_ELEMENT: [http-0x3f805e44] Element task created
I (58325) AUDIO_THREAD: The multi_room_play task allocate stack on external memory
I (58334) ESP_DECODER: Detect audio type is MP3
I (58338) MP3_DECODER: MP3 opened
I (58342) ADF_BIT_STREAM: The element is 0x3f807254. The reserve data 2 is 0x0.
I (58353) ESP_AUDIO_TASK: Recv Element[DEC_auto-0x3f807254] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58367) ESP_AUDIO_TASK: Received music info then on play
I (58368) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (58374) AUDIO_THREAD: The Resample task allocate stack on external memory
I (58382) AUDIO_ELEMENT: [Resample-0x3f8075b8] Element task created
I (58391) AUDIO_ELEMENT: [Resample] AEL_MSG_CMD_RESUME,state:1
I (58395) AUDIO_ELEMENT: [OUT_iis-0x3f807414] Element task created
I (58402) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (58408) I2S_STREAM: AUDIO_STREAM_WRITER
I (58413) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f807414] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58567) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 2
I (58571) ESP_AUDIO_TASK: Recv Element[Resample-0x3f8075b8] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58583) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (58592) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:860, MEM Total:4169352 Bytes, Inter:155804 Bytes, Dram:139972 Bytes

I (59139) MRM_CLIENT: ===== Send Sync =====
Version:ESP32 MRM/1.0
Tag:SYNC_PTS
TSF:5784716
Sync:533

I (59543) MRM_CLIENT: ===== Send Sync =====
Version:ESP32 MRM/1.0
Tag:SYNC_PTS
TSF:5785121
Sync:938

I (59947) MRM_CLIENT: ===== Send Sync =====
Version:ESP32 MRM/1.0
Tag:SYNC_PTS
TSF:5785525
Sync:1344

I (60350) MRM_CLIENT: ===== Send Sync =====
Version:ESP32 MRM/1.0
Tag:SYNC_PTS
TSF:5785929
Sync:1749
```

- Slave playing
```c
I (59095) MRM_EXAMPLE: slave set url https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3
I (60563) HTTP_STREAM: total_bytes=2994349
I (60563) AUDIO_ELEMENT: [http-0x3f805e44] Element task created
W (60563) AUIDO_MEM: Can't support stack on external memory due to ESP32 chip is 1
I (60573) ESP_DECODER: Detect audio type is MP3
I (60577) MP3_DECODER: MP3 opened
I (60581) ADF_BIT_STREAM: The element is 0x3f806250. The reserve data 2 is 0x0.
I (60592) ESP_AUDIO_TASK: Recv Element[DEC_auto-0x3f806250] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (60607) ESP_AUDIO_TASK: Received music info then on play
I (60608) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
W (60613) AUIDO_MEM: Can't support stack on external memory due to ESP32 chip is 1
I (60621) AUDIO_ELEMENT: [Resample-0x3f8065b4] Element task created
I (60629) AUDIO_ELEMENT: [Resample] AEL_MSG_CMD_RESUME,state:1
I (60634) AUDIO_ELEMENT: [OUT_iis-0x3f806410] Element task created
I (60641) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (60647) I2S_STREAM: AUDIO_STREAM_WRITER
I (60653) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f806410] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (60800) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 2
I (60804) ESP_AUDIO_TASK: Recv Element[Resample-0x3f8065b4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (60815) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (60824) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:860, MEM Total:4167400 Bytes, Inter:108288 Bytes, Dram:92456 Bytes

I (61554) MRM_EXAMPLE: slave got sync 601
I (61554) MRM_CLIENT: [sync] Sync 601 PTS 714 E2E_delay [68] sync diff [-13] ms
I (61964) MRM_EXAMPLE: slave got sync 1011
I (61965) MRM_CLIENT: [sync] Sync 1011 PTS 1024 E2E_delay [73] sync diff [-3] ms
```

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
