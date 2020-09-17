# Record and Playback with Resample

When [Rec] button on an audio board is pressed, this example will record, and when the button is released, it will play back the recorded file.

For the Recorder:

- We will set up I2S and get audio at sample rate 48000 Hz, 16-bits, stereo.
- Using resample-filter to convert to 16000 Hz, 16-bits, 1 channel.
- Encode with Wav encoder
- Write to microSD

For the Playback:

- We will read the recorded file, with sample rate 16000 Hz, 16-bits, 1 channel,
- Decode with WAV decoder
- Using resample-filter to convert to 48000 Hz, 16-bits, 2 channel.
- Write the audio to I2S

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.
- Insert a microSD card.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- By using [Rec] button on the ESP32-LyraT board, when the button is pressed, it will start recording, and when the button is released, it will play back the recorded file.
- To stop the pipeline press [Mode] button on the ESP32-LyraT board.
