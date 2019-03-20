# Record and Playback with Resample

By using the [Rec] button on the audio board board, when the button is pressed, it will record and when the button is released, it will playback the recorded file.

For Spiffs Partition:

- We need to add a spiffs partiton to the partition table like the last line of partition_spiffs_example.csv.

For the Recorder:

- We will set up I2S and get audio at sample rate 48000 Hz, 16-bits, 2 channels.
- Using resample-filter to convert to 8000 Hz, 16-bits, 1 channel. 
- Encode with AMRNB encoder.
- Write to Spiffs.

For the Playback:

- We will read the recorded file with sample rate 8000 Hz, 16-bits, 1 channel.
- Decode with AMR decoder.
- Using resample-filter to convert to 48000 Hz, 16-bits, 2 channels.
- Write the audio to I2S.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- Press [Rec] button to start sound receding to a file.
- When [Rec] button is released, it will playback the recorded file.
- To stop the pipeline press [Mode] Button on the audio board.
