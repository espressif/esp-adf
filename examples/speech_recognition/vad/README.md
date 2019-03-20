# Voice activity detection (VAD) example

The example demonstrates the voice activity detection, also known as speech activity detection or speech detection.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- Say something to the board and you should see a message "Speech detected" printed out in the monitor.

### Note

This application will likely report as the speech some other randoms sounds of similar frequency to the human voice, e.g. knocking the board, music with certain tones, etc.
