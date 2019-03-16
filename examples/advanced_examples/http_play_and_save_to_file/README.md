# HTTP Play and Save Pipeline Example

This example shows how to use ADF multi output pipeline to:

* playback a file available as http link,
* save music to a file on a SD card.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphone to the board.
- Insert a microSD card into SD card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up the Wi-Fi connection by running `menuconfig` > `Example Configuration` and filling in `WiFi SSID` and `WiFi Password`.

Load and run the example:

- After power on, Wi-Fi connection will be established.
- Music starts playing automatically after http server is connected to begin file retrieval.
- Once music stops playing and program finishes, you can check "test_output.mp3" file saved on the SD card.

