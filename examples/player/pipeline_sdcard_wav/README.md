# Play WAV file from microSD sample

The demo plays a WAV stored on the microSD card using audio pipeline API

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.
- Insert a microSD card loaded with a WAV file 'test.wav' into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- The board will start playing automatically.
