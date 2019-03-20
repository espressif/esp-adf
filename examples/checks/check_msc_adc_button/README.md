# Check ESP32-LyraTD-MSC on Board Buttons

The ESP32-LyraTD-MSC has six buttons (Set, Play, Rec, Mode, Vol- and Vol+) that are connected through a ladder of resistors to ADC 1, channel 3 of the ESP32. A status of these buttons may be read by the application using a dedicated API "ADC Button" and then used to control the application flow. 

The purpose of this application is to provide a simple demonstration how to read the status of buttons as well as a quick verification that all buttons are operational.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/no-button.png "Not Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`

Load and run the example:

- Press each button on the board to see a response logged the the serial terminal.
