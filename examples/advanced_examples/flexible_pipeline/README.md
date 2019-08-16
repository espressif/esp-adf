# Flexible Pipeline Example

This example shows how to use ADF pipeline to dynamically play back different music.

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC |ESP32-LyraT-Mini |
|:-----------:|:---------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |[![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png " Compatible") |![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphone to the board.
- Insert a microSD card loaded with MP3 file 'test.mp3' and AAC file 'test.aac' which are 44100Hz sample rate into SD card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`

Load and run the example.

- Press [Mode] button to pause the currently played music and play back music between `/sdcard/test.mp3` and `/sdcard/test.aac`.

