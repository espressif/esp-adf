# A2DP and HFP Example

## Play music from Bluetooth 

The example plays music received by Bluetooth from any A2DP Bluetooth source. Also control the music on your phone, with Play (Play Music), Set (Stop Music), Vol+ (Next Song), Vol- (Prev Song) Touch on the lyraT board.

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Then a device needs to play music and send the audio output to the headphones.

## Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:----------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## HFP Hands Free Unit

This is the demo of API for a Hands Free Unit to communicate with an Audio Gateway (e.g. a smart phone). If there is an incoming or outgoing voice call, the audio connection will be set up by AG. ESP32 as a HF can be used  to communicate with a peer device.
