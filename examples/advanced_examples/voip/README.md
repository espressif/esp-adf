# VoIP Example

This example allows users to make calls over the Internet.

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:---------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |[![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up Wi-Fi connection by running `menuconfig` > `VOIP App Configuration` and filling in `WiFi SSID` and `WiFi Password`.
- Select compatible audio codec in `menuconfig` > `VOIP App Configuration` > `SIP Codec`.
- Create the SIP extension, ex: 100 (see below)
- Set up SIP URI in `menuconfig` > `VOIP App Configuration` > `SIP_URI`.

Configure external application:

 Setup the PBX Server like Yet Another Telephony Engine (FreePBX/FreeSwitch or any other PBXs)
 http://docs.yate.ro/wiki/Beginners_in_Yate

## Features
- Lightweight
- Support multiple transports for SIP (UDP, TCP, TLS)
- Support G711A/8000 & G711U/8000 Audio Codec
- Easy setting up by using URI

## Reference
http://www.yate.ro/
https://www.tutorialspoint.com/session_initiation_protocol/index.htm
https://tools.ietf.org/html/rfc3261
