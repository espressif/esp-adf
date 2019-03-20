# Recording WAV file and upload to HTTP Server

By using the [Rec] button on the audio board, when the button is pressed, it will record and upload data to HTTP Stream, and when the button is released, it will stop recording, server will write all data to .wav file.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the Server:
- Require python 2.7 or higher.
- Connect the computer to same Wi-Fi network with ESP32 board.
- Run `python server.py` - place the file `server.py` at root of this example, and make sure this directory is writable.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up the Wi-Fi connection by running `menuconfig` > `Example Configuration` and filling in `WiFi SSID` and `WiFi Password`.
- In the same menu set up server URI, for example `http://192.168.0.248:8000/upload` where `http://192.168.0.248` is the server LAN IP address.

Load and run the example:

- Speak to the board once prompted on the serial monitor.
- After finish, retrieve recorded file from directory where server is located.
- To stop the pipeline press [Mode] button on the audio board.
