# Audio passthru

This demo passes audio received at the "aux_in" port to the headphone and speaker outputs.

Typical use cases:

- Exercising the audio pipeline from end to end when bringing up new hardware.
- Checking for left and right channel consistency through the audio path.
- Used in conjunction with an audio test set to measure THD+N, for production line testing or performance evaluation.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Connect audio source to "aux_in".

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`

Load and run the example:

- You should hear the audio passed from "aux_in" in the headphone and speaker outputs.