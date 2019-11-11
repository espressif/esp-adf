# Example of Playing and Recording at the same time

This example performs a combination of the following two actions at the same time:

- play mp3 song from SD card or from http server
- record a wav file to SD card or do speech recognition

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:----------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |[![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.
- Insert a microSD card loaded with a MP3 file 'test.mp3' into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- The board will start playing automatically.
- Press the any button or touch(except Boot and Rst), the board will stop running.

## Note

This example uses HTTP by default to play music and voice recognition at the same time.
You can configure input and output stream in menuconfig.
