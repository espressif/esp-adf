# Play mp3 files with different sample rates

This example plays the same music at three different sample rates (8000 Hz, 22050 Hz and 44100 Hz) in a loop. The music is stored in three separate files provided in 'main' folder. The files get embedded in the application during compilation. After application upload, the files are played one after another to show differences in audio quality.

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

Load and run the example.

- Music playback will start automatically and loop through the three files.
- The sample rate may be checked during playback on a terminal:

  ```
  I (160) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
  I (12610) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
  I (25030) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
  I (37320) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
  I (49740) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
  ...
  ```
