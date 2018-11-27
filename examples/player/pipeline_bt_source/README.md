# Bluetooth Source Example

To run this example you need ESP32-LyraT and BT-Speaker (or you can use another ESP32-LyraT to run the pipeline_bt_sink example as a BT Speaker):

This example will download an mp3 file from HTTP and decode it into PCM format,
then push it to the BT Speaker through BT Source to play it.

- Connect speakers or headphones to the board.
- Setup the Wi-Fi and BT connection by `make menuconfig` -> `Example configuration` -> Fill in Wi-Fi SSID & Password and BT REMOTE DEVICE NAME

