# DLNA Example

This example allows you to run an UPnP/DLNA MediaRenderer in ESP32 with ESP-ADF.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphone to the board.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up the Wi-Fi connection by running `menuconfig` > `Example Configuration` and filling in `WiFi SSID` and `WiFi Password`.
- Make sure the Wi-Fi network is the same with the Computer which runs Media Center (see below).

Configure external application:

- Install [Universal Media Center](https://www.universalmediaserver.com/) (or any DLNA Media Control point)
- Select ESP32 Renderer -> Select Mp3 file from the Computer -> Play
- The steps are the same as the picture below

![UMS](./ums.png)

Load and run the example.
