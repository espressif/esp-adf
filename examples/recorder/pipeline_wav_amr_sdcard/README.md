# Recoding WAV and AMR file to microSD 

The example records 10 seconds WAV and AMR(AMR-NB or AMR-WB) audio file to microSD card via audio pipeline API. 

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:---------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Insert a microSD card required memory of 1 MB into board's SD card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- You may select encoders between AMR-NB and AMR-WB in `menuconfig` > `Example configuration` > `Audio encoder file type`. 

Load and run the example:

- Speak to the board once prompted on the serial monitor.
- After finish, you can open `/sdcard/rec_out.wav`and `/sdcard/rec_out.amr` (or `/sdcard/rec_out.Wamr`) to hear the recorded file.
