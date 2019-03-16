# Partial audio pipeline with custom read callback registered by the application

The demo plays an AMR-NB or AMR-WB file stored in the SD card with an application specified custom callback. 

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a AMR-NB or AMR-WB file 'test.amr' into the card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example.

- Audio playback will start automatically.
