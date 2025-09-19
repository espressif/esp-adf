# 百度RTC 语音助手演示

- [English](./README.md)|中文

## 例程简介

本例程实现了基于百度 RTC 的智能语音助手演示，支持通过直接对话、语音唤醒方式实现一个可以语音聊天，播放网络音乐并直接两路音频 mixer 功能的 AI 音箱功能。

## 示例创建

### IDF 默认分支

本例程兼容 IDF release/v5.4 及以上版本的分支。

### 预备知识

首先需要在[百度云环境搭建](../../README.md)申请 `App id`、`user id`、`workflows` 和 `licence key` 账号信息。

更多的百度RTC文档可以参考 [百度RTC官方文档](https://cloud.baidu.com/doc/RTC/index.html)

本示例基于 [ESP-GMF](https://github.com/espressif/esp-gmf) 框架，演示了音频初始化及 3A（自动增益、回声消除、噪声抑制）算法的应用。

## 前期准备

### 硬件准备

- 本例程默认的是 `esp32-s3-korvo-2` 开发板，硬件参考相关[文档](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)。当前的代码中使用的是 [esp_board_manager](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.3.0/readme) 组件，如何切换 board 和 添加 board 可以参加 esp_board_manager 文档

### 软件准备

- 在 menuconfig->Example Connection Configuration 中配置相关的 `wifi` 信息
- 在 menuconfig->Example Audio Configuration 中填写申请的 `app id`、`user id`、`workflows`、 `licence key` 和 `instance id` 信息

### 关于工作模式

> 目前系统支持以下几种工作模式：

- **普通模式**：用户无需使用唤醒词，可以直接连续与设备进行语音交互。

- **唤醒对话模式**：用户通过唤醒词唤醒设备，唤醒后可进行语音交互。默认唤醒词为 `Hi 乐鑫`，可在 `menuconfig -> ESP Speech Recognition -> use wakenet -> Select wake words` 中选择唤醒词。

> 默认启用的工作模式为 **普通模式**

### 配置

1. 将获取到的 `app id`、`user id`、`workflows` 、 `licence key` 和 `instance id` 信息填入 `Menuconfig->Example Configuration` 中。
2. 將 wifi 信息填入 `Menuconfig->Example Configuration` 中。

### 编译和下载

编译和下载
在编译本例程之前，请确保已配置好 ESP-IDF 环境。如果已配置，可以跳过此步骤，直接进行后续配置。如果尚未配置，请在 ESP-IDF 根目录运行以下脚本来设置编译环境。有关完整的配置和使用步骤，请参考 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```bash
./install.sh
. ./export.sh
```

- 选择编译芯片，以 esp32s3 为例：

```bash
idf.py set-target esp32s3
```

- 使用 esp_board_manager 选择 `esp32-s3-korvo-2` 开发板
```bash
export IDF_EXTRA_ACTIONS_PATH=./managed_components/espressif__esp_board_manager
idf.py gen-bmgr-config -b esp32_s3_korvo2_v3
```

- 编译程序

```bash
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```bash
idf.py -p PORT flash monitor
```

## 如何使用例程

### 功能和用法

- 例程开始运行后，下方 log 表明与百度RTC服务端成功建立连接，已具备对话条件:

```text
Creating client...
BRTC SDK initialized successfully.
Login room success
I (7809) brtc: Call state changed: 1
I (7813) brtc: Login success
I (7815) brtc: join room success
I (7820) main_task: Returned from app_main()
onRtcMessage 100
I (8358) brtc: Call state changed: 1
I (8358) brtc: Login success
Login ok!
I (8506) wifi:<ba-add>idx:0 (ifx:0, ec:56:23:e9:7e:f0), tid:6, ssn:259, winSize:64
08:00:07:750 client_session: create pc (type=offer)
08:00:07:750 peerconnection(p): new: laddr = 127.0.0.1, got_offer=0
08:00:07:751 peerconnection(p): using mnat 'ice'
08:00:07:754 peerconnection(p): using menc 'srtp-rtp'
08:00:07:759 peerconnection(p): add audio (codecs=1)
08:00:07:767 stream: starting mediaenc 'srtp-rtp'
audio_alloc nack:1
08:00:07:770 client_session_create_offer -- send offer
08:00:07:775 peerconnection(p): create offer
08:00:07:779 publisherCreateOffer -- before.
08:00:07:783 publisherCreateOffer -- after.
08:00:07:787 reply_descr -- exit.
onRtcMessage 300
User joined room id: 1, display: ChatAgent-1.5.39-981564e.
onRtcMessage 104
08:00:07:799 ice: audio: Default local candidates: 192.168.3.59:8668 / ?
08:00:07:804 peerconnection(p): medianat established/gathered (all streams)
08:00:07:811 client_session: session gathered -- send offer, 2837597182464051
08:00:07:818 peerconnection(p): start ice
08:00:07:822 peerconnection: ice: sdp not ready
08:00:07:826 peerconnection: ice: start retry 1 in tmr
08:00:07:880 descr: decode: type='answer'
08:00:07:881 peerconnection(p): set remote description. type=answer
08:00:07:903 peerconnection(p): start ice
08:00:07:952 ice(p): audio: connectivity check is complete (update=1)
08:00:07:952 stream: mnat connected: raddr 180.101.50.165:10010
08:00:07:953 stream: starting mediaenc 'srtp-rtp'
08:00:07:957 peerconnection(p): ice connected (audio)
08:00:07:962 stream: audio: starting mediaenc 'srtp-rtp' (wait_secure=0)
08:00:07:968 client_session: stream established: 'audio'
08:00:07:973 mediatrack: start audio
08:00:07:977 outer_audio: 16000 Hz, 1 channels, frequency 440 Hz CH_MODE: 3
08:00:07:983 outer_audio: audio ptime=20 sampc=320
08:00:07:988 audio: source started with sample format S16LE
08:00:07:993 ice: audio: sending Re-INVITE with updated default candidates
08:00:08:000 peerconnection(p): medianat established/gathered (all streams)
08:00:08:006 client_session: session gathered -- send offer, 2837597182464051
08:00:08:013 peerconnection(p): start ice
08:00:08:020 client_session: create pc (type=answer)
08:00:08:022 peerconnection(s): new: laddr = 127.0.0.1, got_offer=1
08:00:08:028 peerconnection(s): using mnat 'ice'
08:00:08:032 peerconnection(s): using menc 'srtp-rtp'
08:00:08:037 peerconnection(s): add audio (codecs=1)
08:00:08:045 stream: starting mediaenc 'srtp-rtp'
audio_alloc nack:1
08:00:08:049 descr: decode: type='offer'
08:00:08:051 peerconnection(s): set remote description. type=offer
08:00:08:060 client_session_create_answer -- send answer
08:00:08:062 peerconnection(s): create answer
08:00:08:066 client_session_create_answer -- send answer
onRtcMessage 2000
Stream up, send/got video now.
08:00:08:076 ice: audio: Default local candidates: 192.168.3.59:8308 / ?
08:00:08:082 peerconnection(s): medianat established/gathered (all streams)
08:00:08:089 client_session: session gathered -- send offer, 2837617315123235
08:00:08:096 peerconnection(s): start ice
onRtcMessage 2001
08:00:08:253 ice(s): audio: connectivity check is complete (update=0)
08:00:08:254 stream: mnat connected: raddr 180.101.50.165:10010
08:00:08:254 stream: starting mediaenc 'srtp-rtp'
08:00:08:258 peerconnection(s): ice connected (audio)
08:00:08:263 stream: audio: starting mediaenc 'srtp-rtp' (wait_secure=0)
onRtcMessage 2000
Stream up, send/got video now.
I (9229) brtc: Media setup completed.
08:00:08:351 peerconnection(s): rtp established (audio)
I (9393) ESP_GMF_TASK: One times job is complete, del[wk:0x3c3770d0, ctx:0x3c3759f8, label:aud_rate_cvt_open]
onRtcMessage 302
I (9400) brtc: onAIAgentSubtitle 你一出现，气氛就变得好软萌呀，快来和我说说话
onRtcMessage 302
I (10116) brtc: onAIAgentSpeaking is Start
onRtcMessage 302
I (16797) brtc: onAIAgentSpeaking is Stop
```

## 故障排查

### 自问自答现象

**问题描述：**

设备出现自问自答现象，即设备播放的音频被麦克风重新采集并识别，导致系统误触发。

**问题原因：**

不同硬件平台采用的麦克风和扬声器型号存在差异，且麦克风与扬声器之间的物理距离各不相同。这些硬件差异可能导致音频采集数据出现增益过大或过小的情况，从而影响回声消除（AEC）算法的效果。

**解决方案：**

可通过调整音频增益参数来解决此问题。如修改 [参考信号](./main/brtc_app.c#304)的增益
