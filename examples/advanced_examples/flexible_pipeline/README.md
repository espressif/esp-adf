# Flexible Pipeline Example
This example shows how to use ADF pipeline to dynamically playback different music.
To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphone to the board.
- Insert a microSD card loaded with MP3 file 'test.mp3' and AAC file 'test.aac' which are 44100Hz sample rate into SD card slot.

## Supported Features
- Press [Mode] button to pause the currently played music and playback music between `/sdcard/test.mp3` and `/sdcard/test.aac`.
