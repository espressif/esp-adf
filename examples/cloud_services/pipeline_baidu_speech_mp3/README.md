# Play MP3 stream from Baidu speech

The goal of this example is to show how to use Audio Pipeline to play audio generated from text by [Baidu Speech synthesis](http://ai.baidu.com/tech/speech/tts) service. The example provides text in Chinese, but other languages are supported as well.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`
- Set up the Wi-Fi connection by running `menuconfig` > `Example Configuration` and filling in `WiFi SSID` and `WiFi Password`.
- Under the same menu provide `BAIDU_ACCESS_KEY` and `BAIDU_SECRET_KEY`

Load and run the example.

For more details on how to use the Baidu Speech synthesis service you can refer to [Speech Synthesis API](https://cloud.baidu.com/doc/SPEECH/TTS-API.html). 

Note: The language used in Baidu's services is Chinese.
