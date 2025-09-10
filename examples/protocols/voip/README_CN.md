# VoIP example

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_complex.png "高级")

## 例程简介

ESP VoIP 是一个基于标准 SIP 协议的电话客户端，可以用于点对点通话和音频会议等场景。

### 资源列表

示例内存消耗：

ESP32-LyraT-Mini：

|Memory_total (Bytes)|Memory_inram (Bytes)|Memory_psram (Bytes)
|---|---|---
|392008 |236328 |155680

其他开发板：

|Memory_total (Bytes)|Memory_inram (Bytes)|Memory_psram (Bytes)
|---|---|---
|252900 |111076 |141824

### 预备知识

- [SIP 协议栈](https://en.wikipedia.org/wiki/Session_Initiation_Protocol)
- [RFC3261](https://tools.ietf.org/html/rfc3261)

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

建议您使用 `ESP32-S3-Korvo-2L` 和 `ESP32-LyraT-Mini` 开发板来运行本例程。

### 其他要求

- 您需要搭建如下 SIP PBX 服务器中的一个：

  - [Asterisk FreePBX](https://www.freepbx.org/downloads/)

  - [Asterisk for Raspberry Pi](http://www.raspberry-asterisk.org/)

  - [Freeswitch](https://freeswitch.org/confluence/display/FREESWITCH/Installation)
      - 建议关闭服务器事件通知 `NOTIFY`，可以通过在 `conf/sip_profiles/internal.xml` 中设置 `<param name="send-message-query-on-register" value="false"/>` 关闭通知。

      - 建议关闭服务器 timer，可以通过在 `conf/sip_profiles/internal.xml` 中设置 `<param name="enable-timer" value="false"/>` 来关闭。

      - 建议在 `conf/vars.xml` 中删除暂不支持的 Video Codec。

      - 建议关闭服务器自动应答，可以通过在 `conf/dialplan/default.xml` 中设置 `<action application="export" data="sip_auto_answer=false"/>` 来关闭。

      - 建议关闭服务器强制应答，可以通过在 `conf/dialplan/default.xml` 中注释以下三行来关闭。
      `<action application="answer"/>`
      `<action application="sleep" data="1000"/>`
      `<action application="bridge" data="loopback/app=voicemail:default ${domain_name} ${dialed_extension}"/>`

  - [Kamailio](https://kamailio.org/docs/tutorials/5.3.x/kamailio-install-guide-git/)

  - [Yate Server](http://docs.yate.ro/wiki/Beginners_in_Yate)

- 我们建议搭建 Freeswitch 服务器来测试。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

打开配置选项 `make menuconfig` 或 `idf.py menuconfig`

- 在 `menuconfig` > `Audio HAL` 中选择合适的开发板。
- 在 `VoIP App Configuration` > `WiFi SSID` 和 `WiFi Password` 配置 Wi-Fi 网络或者使用 `Esptouch` 应用来配置。
- 在 `VoIP App Configuration` > `SIP Codec` 中选择想要的编解码器。
- 在 `VoIP App Configuration` > `SIP_URI` 中配置 SIP 登陆相关信息（Transport://user:password@server:port）。
  - 例如：`tcp://100:100@192.168.1.123:5060`

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
make -j8 flash monitor ESPPORT=PORT
```

或者，基于 CMake 编译系统：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/get-started/index.html)。

### 下载 flash 提示音

- 此应用额外需要下载一个提示音的 bin 到 flash 中，注意partition size 0x210000 需要与 partitions.csv 中的信息匹配：

```
  python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x210000 ../components/audio_flash_tone/bin/audio-esp.bin
```

## 如何使用例程

### 功能和用法

- 例程开始运行后，在网络未连接时会播放需要进行配网的提示音，进入配网模式会提示正在进行配网，服务器连接成功后会播放服务器已连接。
- 当设备提示需要配置网络时，请长按 `Set` 键进入配网模式，同时打开手机 [Esptouch](https://www.espressif.com/zh-hans/support/download/apps?keys=ESP-TOUCH) App 来进行配网。
- 服务器连接成功后，您可以按下 `Play` 键进行呼叫或者接听，`Mode` 键进行挂断或者取消呼叫。
- `Vol+` 和 `Vol-` 键可以调节开发板通话音量，在 `esp32-lyrat-mini` 开发板上可以按下 `Rec` 键进行 `MIC` 静音的操作。
- 您也可以使用 Linphone/MicroSIP 等开源客户端与开发板进行通话。
- 如果 AEC 效果不是很好，你可以打开 `DEBUG_AEC_INPUT` define，就可以获取到原始的输入数据（左声道是从麦克风获取到的信号，右声道是扬声器端获取到的信号），并使用音频分析工具查看麦克风信号和扬声器信号之间的延迟。
- AEC 内部缓冲机制要求录制信号相对于相应的参考（回放）信号延迟 0 - 10 ms 左右。

### 日志输出

- SIP 注册
```c
I (4164) VOIP_EXAMPLE: [ 3 ] Create and start input key service
I (4164) VOIP_EXAMPLE: [ 4 ] Create SIP Service
I (4184) SIP: Conecting...
W (4184) SIP: CHANGE STATE FROM 0, TO 1, :func: sip_connect:1564
I (4184) SIP: [1970-01-01/00:00:01]=======WRITE 0587 bytes>>
I (4184) SIP:

REGISTER sip:1000@192.168.50.50:5060 SIP/2.0
Via: SIP/2.0/UDP 192.168.50.51:16810;branch=z9hG4bK--1374356523;rport
From: <sip:1000@192.168.50.50:5060>;tag=319434100
To: <sip:1000@192.168.50.50:5060>
Contact: <sip:1000@192.168.50.51:16810>
Max-Forwards: 70
Call-ID: 92FA07B61E9DFFAEA89F08C3F382F7AF342CE46DC71F
CSeq: 1 REGISTER
Expires: 3600
User-Agent: ESP32 SIP/2.0
Content-Length: 0
Allow: INVITE, ACK, CANCEL, BYE, UPDATE, REFER, MESSAGE, OPTIONS, INFO, SUBSCRIBE
Supported: replaces, norefersub, extended-refer, timer, X-cisco-serviceuri
Allow-Events: presence, kpml

I (4244) SIP: [1970-01-01/00:00:01]=======================>>
I (4294) SIP: [1970-01-01/00:00:01]<<=====READ 0612 bytes==
I (4294) SIP:

SIP/2.0 401 Unauthorized
Via: SIP/2.0/UDP 192.168.50.51:16810;branch=z9hG4bK--1374356523;rport=16810
From: <sip:1000@192.168.50.50:5060>;tag=319434100
To: <sip:1000@192.168.50.50:5060>;tag=DFvg6Q9830p9a
Call-ID: 92FA07B61E9DFFAEA89F08C3F382F7AF342CE46DC71F
CSeq: 1 REGISTER
User-Agent: FreeSWITCH-mod_sofia/1.8.5~64bit
Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, INFO, UPDATE, REGISTER, REFER, NOTIFY, PUBLISH, SUBSCRIBE
Supported: timer, path, replaces
WWW-Authenticate: Digest realm="192.168.50.50", nonce="f5d78ba3-36fd-4726-a4bb-a669fa0c575c", algorithm=MD5, qop="auth"
Content-Length: 0

I (4344) SIP: [1970-01-01/00:00:01]<<======================
I (4354) SIP: Required authentication
I (4354) SIP: [1970-01-01/00:00:01]=======WRITE 0837 bytes>>
I (4364) SIP:

REGISTER sip:1000@192.168.50.50:5060 SIP/2.0
Via: SIP/2.0/UDP 192.168.50.51:16810;branch=z9hG4bK-260131642;rport
From: <sip:1000@192.168.50.50:5060>;tag=-1948762171
To: <sip:1000@192.168.50.50:5060>
Contact: <sip:1000@192.168.50.51:16810>
Max-Forwards: 70
Call-ID: 92FA07B61E9DFFAEA89F08C3F382F7AF342CE46DC71F
CSeq: 2 REGISTER
Expires: 3600
User-Agent: ESP32 SIP/2.0
Content-Length: 0
Allow: INVITE, ACK, CANCEL, BYE, UPDATE, REFER, MESSAGE, OPTIONS, INFO, SUBSCRIBE
Supported: replaces, norefersub, extended-refer, timer, X-cisco-serviceuri
Allow-Events: presence, kpml
Authorization: Digest username="1000", realm="192.168.50.50", nonce="f5d78ba3-36fd-4726-a4bb-a669fa0c575c", uri="sip:192.168.50.50:5060", response="3394d8e71a7d36abcced2a0912e3ff5c", algorithm=MD5, nc=00000001, cnonce="6fe907854a9bc351", qop="auth"

I (4434) SIP: [1970-01-01/00:00:01]=======================>>
I (4464) SIP: [1970-01-01/00:00:01]<<=====READ 0572 bytes==
I (4464) SIP:

SIP/2.0 200 OK
Via: SIP/2.0/UDP 192.168.50.51:16810;branch=z9hG4bK-260131642;rport=16810
From: <sip:1000@192.168.50.50:5060>;tag=-1948762171
To: <sip:1000@192.168.50.50:5060>;tag=erN97jtc19cvp
Call-ID: 92FA07B61E9DFFAEA89F08C3F382F7AF342CE46DC71F
CSeq: 2 REGISTER
Contact: <sip:1000@192.168.50.51:16810>;expires=3600
Date: Tue, 16 Jun 2020 12:09:04 GMT
User-Agent: FreeSWITCH-mod_sofia/1.8.5~64bit
Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, INFO, UPDATE, REGISTER, REFER, NOTIFY, PUBLISH, SUBSCRIBE
Supported: timer, path, replaces
Content-Length: 0

I (4514) SIP: [1970-01-01/00:00:01]<<======================
I (4524) VOIP_EXAMPLE: SIP_EVENT_REGISTERED
W (4524) SIP: CHANGE STATE FROM 1, TO 2, :func: sip_register:1592
```

- 收到来电
```c
I (688034) SIP: [1970-01-01/00:11:13]<<=====READ 1372 bytes==
I (688034) SIP:

INVITE sip:1000@192.168.50.51:16810 SIP/2.0
Via: SIP/2.0/UDP 192.168.50.50;rport;branch=z9hG4bKBXFpg22KXpmHj
Max-Forwards: 69
From: "Extension 1001" <sip:1001@192.168.50.50>;tag=9B06QD1BKt6mF
To: <sip:1000@192.168.50.51:16810>
Call-ID: 9a8db3e8-2a6e-1239-0d93-0b85a3e82ded
CSeq: 21601221 INVITE
Contact: <sip:mod_sofia@192.168.50.50:5060>
User-Agent: FreeSWITCH-mod_sofia/1.8.5~64bit
Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, INFO, UPDATE, REGISTER, REFER, NOTIFY, PUBLISH, SUBSCRIBE
Supported: timer, path, replaces
Allow-Events: talk, hold, conference, presence, as-feature-event, dialog, line-seize, call-info, sla, include-session-description, presence.winfo, message-summary, refer
Content-Type: application/sdp
Content-Disposition: session
Content-Length: 443
X-FS-Support: update_display,send_info
Remote-Party-ID: "Extension 1001" <sip:1001@192.168.50.50>;party=calling;screen=yes;privacy=off

v=0
o=FreeSWITCH 1592283967 1592283968 IN IP4 192.168.50.50
s=FreeSWITCH
c=IN IP4 192.168.50.50
t=0 0
m=audio 26060 RTP/AVP 8 0 96 97 98 101 102 104
a=rtpmap:8 PCMA/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:96 SPEEX/16000
a=rtpmap:97 SPEEX/8000
a=rtpmap:98 SPEEX/32000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-16
a=rtpmap:102 telephone-event/16000
a=fmtp:102 0-16
a=rtpmap:104 telephone-event/32000
a=fmtp:104 0-16
a=ptime:20

I (688154) SIP: [1970-01-01/00:11:13]<<======================
I (688164) SIP: Remote RTP port=26060
I (688164) SIP: Remote RTP addr=192.168.50.50
I (688164) SIP: call from 1001
I (688174) SIP: [1970-01-01/00:11:13]=======WRITE 0414 bytes>>
I (688174) SIP:

SIP/2.0 100 Trying
Via: SIP/2.0/UDP 192.168.50.50;rport;branch=z9hG4bKBXFpg22KXpmHj
Contact: <sip:1000@192.168.50.51:16810>
From: "Extension 1001" <sip:1001@192.168.50.50>;tag=9B06QD1BKt6mF
To: <sip:1000@192.168.50.51:16810>;tag=645213922
Call-ID: 9a8db3e8-2a6e-1239-0d93-0b85a3e82ded
CSeq: 21601221 INVITE
Server: ESP32 SIP/2.0
Allow: ACK, INVITE, BYE, UPDATE, CANCEL, OPTIONS, INFO
Content-Length: 0

I (688214) SIP: [1970-01-01/00:11:13]=======================>>
I (688224) SIP: [1970-01-01/00:11:13]=======WRITE 0417 bytes>>
I (688234) SIP:

SIP/2.0 180 Ringing
Via: SIP/2.0/UDP 192.168.50.50;rport;branch=z9hG4bKBXFpg22KXpmHj
Contact: <sip:1000@192.168.50.51:16810>
From: "Extension 1001" <sip:1001@192.168.50.50>;tag=9B06QD1BKt6mF
To: <sip:1000@192.168.50.51:16810>;tag=-1799498926
Call-ID: 9a8db3e8-2a6e-1239-0d93-0b85a3e82ded
CSeq: 21601221 INVITE
Server: ESP32 SIP/2.0
Allow: ACK, INVITE, BYE, UPDATE, CANCEL, OPTIONS, INFO
Content-Length: 0

I (688274) SIP: [1970-01-01/00:11:13]=======================>>
W (688284) SIP: CHANGE STATE FROM 2, TO 16, :func: _sip_uas_process_req_invite:810
I (689384) VOIP_EXAMPLE: ringing... RemotePhoneNum 1001
I (690384) VOIP_EXAMPLE: ringing... RemotePhoneNum 1001
I (691384) VOIP_EXAMPLE: ringing... RemotePhoneNum 1001
I (692384) VOIP_EXAMPLE: ringing... RemotePhoneNum 1001
```

- 挂断
```c
I (1372804) SIP: User call sip BYE
I (1372834) SIP: [1970-01-01/00:22:25]=======WRITE 0664 bytes>>
I (1372834) SIP:

BYE sip:mod_sofia@192.168.50.50:5060 SIP/2.0
Via: SIP/2.0/UDP 192.168.50.50;rport;branch=z9hG4bKDF27Kr4tQ80pS
From: <sip:1000@192.168.50.51:16810>;tag=1647388708
To: "Extension 1001" <sip:1001@192.168.50.50>;tag=6HXNBy9HZvcKF
Contact: <sip:1000@192.168.50.51:16810>
Max-Forwards: 70
Call-ID: 2f65a33b-2a70-1239-0d93-0b85a3e82ded
CSeq: 43 BYE
Expires: 3600
User-Agent: ESP32 SIP/2.0
Content-Length: 0
Authorization: Digest username="1000", realm="192.168.50.50", nonce="f5d78ba3-36fd-4726-a4bb-a669fa0c575c", uri="sip:192.168.50.50:5060", response="f8c3de6f561f54dec9498a28f2fb5c73", algorithm=MD5, nc=0000002a, cnonce="19f9e2e22ddc905c", qop="auth"

I (1372884) SIP: [1970-01-01/00:22:25]=======================>>
I (1372914) SIP: [1970-01-01/00:22:25]<<=====READ 0501 bytes==
I (1372914) SIP:

SIP/2.0 200 OK
Via: SIP/2.0/UDP 192.168.50.50;rport=16810;branch=z9hG4bKDF27Kr4tQ80pS;received=192.168.50.51
From: <sip:1000@192.168.50.51:16810>;tag=1647388708
To: "Extension 1001" <sip:1001@192.168.50.50>;tag=6HXNBy9HZvcKF
Call-ID: 2f65a33b-2a70-1239-0d93-0b85a3e82ded
CSeq: 43 BYE
User-Agent: FreeSWITCH-mod_sofia/1.8.5~64bit
Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, INFO, UPDATE, REGISTER, REFER, NOTIFY, PUBLISH, SUBSCRIBE
Supported: timer, path, replaces
Content-Length: 0

I (1372954) SIP: [1970-01-01/00:22:25]<<======================
W (1372964) SIP: CHANGE STATE FROM 32, TO 2, :func: _sip_request_bye:1368
I (1372974) SIP_RTP: send task stopped
I (1373024) VOIP_EXAMPLE: SIP_EVENT_AUDIO_SESSION_END
```

## Troubleshooting

- 如果您使用 `Esptouch` 配网出现 Crash，请将 `SC_ACK_TASK_STACK_SIZE` 适当调大一些。
- 如果您遇到无法连接服务器的情况，请先使用 Linphone/MicroSIP 开源客户端验证您的服务器是否工作正常。
- 如果您需要减少 SIP 信令 RTT 响应时间，您可以设置 esp_log_level_set("SIP", ESP_LOG_WARN)。

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
