# Record and Playback with Resample

When [Rec] button on an audio board is pressed, this example will record, and when the button is released, it will play back the recorded file.

For the Recorder:

- We will set up I2S and get audio at sample rate 48000 Hz, 16-bits, 2 channel.
- Using resample-filter to convert to 16000 Hz, 16-bits, 1 channel. 
- Encode with Wav encoder
- Write to microSD

For the Playback:

- We will read the recorded file, with sample rate 16000 Hz, 16-bits, 1 channel,
- Decode with WAV decoder
- Using resample-filter to convert to 48000 Hz, 16-bits, 2 channel.
- Write the audio to I2S

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- By using [Rec] button on the ESP32-LyraT board, when the button is pressed, it will start recording, and when the button is released, it will play back the recorded file.
- To stop the pipeline press [Mode] button on the ESP32-LyraT board.
