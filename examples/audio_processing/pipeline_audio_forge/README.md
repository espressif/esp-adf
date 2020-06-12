# AUDIO FORGE Pipeline Example

This example shows how to use ESP-ADF multiple input pipeline to play back multiple files with audio_forge. The audio forge contains resample, downmix, ALC, equalizer and sonic . Choose a combination of several function by `component_select`.

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |


## Usage

Prepare the audio board:

- Connect speakers or headphone to the board.
- Insert a microSD card loaded with WAV files 'test1.wav' and 'test2.wav'.

##Configure the example

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set the number of input files by `NUMBER_SOURCE_FILE`.

Load and run the example:

- The first file starts playing automatically after power on.
- Once [Mode] button is pressed, the others with be mixed with the first file playing.

## Additional Information

If equalizer is opened, the following conditions should be met:

To change the parameters of the equalizer, edit eq_gain[] table in `audio_forge_pipeline_main.c`. The center frequencies of equalizer are 31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz and 16 kHz.

- The destination file should be in format supported by the equalizer:
    * Sampling rate of 11025 Hz, 22050 Hz, 44100 Hz or 48000 Hz.
    * Number of channels: 1 or 2.

If down-mixer is opened, the following conditions should be met that the number of source files should less and equal than `SOURCE_NUM_MAX`.

If all component are opened, `NUMBER_SOURCE_FILE` less than three works better.
