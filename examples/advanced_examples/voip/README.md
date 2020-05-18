# VoIP Example

This example allows users to make calls over the Internet.

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Dul1906 | ![alt text](../../../docs/_static/esp32-korvo-dul1906-v1.1-small.jpg "ESP32-Korvo-DUL1906") | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | ![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit") | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

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
