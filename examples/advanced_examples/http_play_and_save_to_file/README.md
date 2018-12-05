# HTTP Play And Save Pipeline Example
This example shows how to use ADF multi output pipeline to:
* playback a file available as http link,
* save music to a file on a SD card.

To run this example you need ESP32-LyraT or compatible board:

- Connect speakers or headphone to the board.
- Insert a microSD card into SD card slot.
- Setup the Wi-Fi connection by `make menuconfig` -> `Example configuration` -> Fill in Wi-Fi SSID and Password
- Once music stops playing and program finishes, you can check "test_output.mp3" file saved on the SD card.