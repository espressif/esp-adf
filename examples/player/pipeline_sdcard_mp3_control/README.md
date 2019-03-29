# Play MP3 files from microSD with control 

The demo plays MP3 files stored on the SD card using audio pipeline API. User can start, stop, pause, resume playback and advance to the next song as well as adjust volume. When playing, the application automatically advances to the next song once previous finishes.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a MP3 files 'test.mp3', 'test1.mp3' and 'test2.mp3' into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example. Use buttons to control how audio is played:

- Use [Play] to start, pause and resume playback.
- Adjust sound volume with [Vol-] or [Vol+].
- To stop the pipeline press [Set].
