# VoIP example

- [中文版本](./README_CN.md)
- Complex Example: ![alt text](../../../docs/_static/level_complex.png "Complex Example")

## Example Brief

ESP VoIP is a telephone client based on the standard SIP protocol, which can be used in some P2P or audio conference scenarios.

### Resources

Memory consumption of the entire example:

esp32-lyrat-mini-v1.2:

|Memory_total (Bytes)|Memory_inram (Bytes)|Memory_psram (Bytes)
|---|---|---
|392008 |236328 |155680

other boards:

|Memory_total (Bytes)|Memory_inram (Bytes)|Memory_psram (Bytes)
|---|---|---
|252900 |111076 |141824

### Prerequisites

- [SIP Protocol](https://en.wikipedia.org/wiki/Session_Initiation_Protocol)
- [RFC3261](https://tools.ietf.org/html/rfc3261)

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

Recommended to use `ESP32-S3-Korvo-2L` and `ESP32-LyraT-Mini` boards to run。

### Additional Requirements

- You need to build one of the following SIP PBX servers:

  - [Asterisk FreePBX](https://www.freepbx.org/downloads/)

  - [Asterisk for Raspberry Pi](http://www.raspberry-asterisk.org/)

  - [Freeswitch](https://freeswitch.org/confluence/display/FREESWITCH/Installation)
      - Recommended to turn off the server event `NOTIFY` by setting `<param name="send-message-query-on-register" value="false"/>` in `conf/sip_profiles/internal.xml`.
      - Recommended to turn off the server timer by setting `<param name="enable-timer" value="false"/>` in `conf/sip_profiles/internal.xml`.
      - Recommended to delete the unsupported Video Codec in `conf/vars.xml`.
      - Recommended to turn off the auto answer by setting `<action application="export" data="sip_auto_answer=false"/>` in `conf/dialplan/default.xml`.
      - Recommended to turn off the force answer before brigde by commenting out the following three lines in `conf/dialplan/default.xml`.
      `<action application="answer"/>`
      `<action application="sleep" data="1000"/>`
      `<action application="bridge" data="loopback/app=voicemail:default ${domain_name} ${dialed_extension}"/>`

  - [Kamailio](https://kamailio.org/docs/tutorials/5.3.x/kamailio-install-guide-git/)

  - [Yate Server](http://docs.yate.ro/wiki/Beginners_in_Yate)

- We recommend building a Freeswitch server for testing.

## Example Set Up

### Default IDF Branch

This example supports IDF release/v4.4 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Open the project configuration menu (`make menuconfig` or `idf.py menuconfig`).

- Select a compatible audio board in `menuconfig` > `Audio HAL`.
- Set up Wi-Fi connection in `Example Configuration` > `WiFi SSID` and `WiFi Password` or you can use the `Esptouch` application to configure.
- Select compatible audio codec in `VoIP App Configuration` > `SIP Codec`.
- Set up SIP URI (Transport://user:password@server:port) in `VoIP App Configuration` > `SIP_URI`.
  - eg: `tcp://100:100@192.168.1.123:5060`

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
make -j8 flash monitor ESPPORT=PORT
```

Or, for CMake based build system:

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/get-started/index.html) to build projects.

### Download flash tone

- To use this example, you need to download a tone onto flash, note that partition size 0x210000 needs to match the information in partitions.csv:

```
  python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x210000 ../components/audio_flash_tone/bin/audio-esp.bin
```

## How to use the Example

### Example Functionality

- After the example starts to run, when the network is not connected, it will play a prompt tone that the network needs to be configured, Entering the smartconfig mode will prompt that the network configuration is in progress, After the SIP server is connected, it will play that the server is connnected.
- When the device prompts that you need to configure the network, please long press the `Set` key to enter the network configuration mode, and open the phone app [Esptouch](https://www.espressif.com/en/support/download/apps?keys=ESP-TOUCH) to configure the network.
- After the server is connected, you can press the `Play` button to make a call or answer an incoming call, and the `Mode` button to hang up or cancel the call.
- The `Vol+` and `Vol-` keys can adjust the call volume of the development board. On the `esp32-lyrat-mini` development board, you can press the `Rec` key to mute the `MIC`.
- You can use the Open source clients such as Linphone or MicroSIP to make VoIP calls with the development board.
- If the AEC effect is not very good, you can open the `DEBUG_AEC_INPUT` define to get the original input data (left channel is the signal captured from the microphone, and right channel is the signal played to the speaker), and then check the delay with an audio analysis tool.
- The AEC internal buffering mechanism requires that the recording signal is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal.

### Example Logs

- SIP register
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

- call in
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

- hangup
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

- If you have a crash when you use `Esptouch` to configure the network, please increase the size of `SC_ACK_TASK_STACK_SIZE` appropriately.
- If the board are unable to connect to the server, please use the Linphone or MicroSIP open source client to verify that your server is working properly.
- If you need reduce the RTT(Response time), you can set esp_log_level_set("SIP", ESP_LOG_WARN).

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
