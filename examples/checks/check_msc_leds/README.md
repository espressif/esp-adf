# Check ESP32-LyraTD-MSC Board LEDs

The ESP32-LyraTD-MSC has 14 LEDs (twelve blue, one green and one red) that may be individually switched on and off. The purpose of this application is to provide a simple demonstration how to operate LEDs as well as a quick verification that all LEDs are operational.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/no-button.png "Not Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- The application is switching each LED on in a sequence one by one. When all LEDs are on, they are switched off in the same order. The sequence advances every second and repeats without a definite end until the board is reset or the power is cycled.
