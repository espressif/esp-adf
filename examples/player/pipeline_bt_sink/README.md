# Play music from Bluetooth 

The example plays music received by Bluetooth from any A2DP Bluetooth source. 

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:----------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- Connect with Bluetooth on your smartphone to the audio board identified as "ESP-ADF-SPEAKER"
- Play some audio from the smartphone and it will be transmitted over Bluetooth to the audio bard.
- The audio playback may be controlled from the smartphone, as well as from the audio board. The following controls may be used:

    |   Smartphone   | Audio Board |
    |:--------------:|:-----------:|
    |   Play Music   |    [Play]   |
    |   Stop Music   |    [Set]    |
    |   Next Song    |    [Vol+]   |
    | Previous Song  |    [Vol-]   |
    |   Volume Up    |    [n/a]    |
    |  Volume Down   |    [n/a]    |