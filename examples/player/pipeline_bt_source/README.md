# Bluetooth Source Example

This example will download an mp3 file from HTTP and decode it into PCM format,
then push it to the BT Speaker through BT Source to play it.

To run this example you need a compatible audio board and BT Speaker (or you can use another audio board to run the [pipeline_bt_sink](../pipeline_bt_sink) example as a BT Speaker).

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board and the BT Speaker:

- Connect speakers or headphones to the board.
- Switch the BT Speaker on.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up the Wi-Fi and BT connection in `menuconfig` -> `Example Configuration` -> Fill in `Wi-Fi SSID` & `WiFi Password` and `BT remote device name`.

Load and run the example:

- The audio board will first connect to the Wi-Fi and the BT Speaker.
- Then the audio board will start transmitting to play on the BT Speaker.
