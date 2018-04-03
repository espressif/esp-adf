# Play MP3 stream from Baidu speech

The goal of this example is to show how to use Audio Pipeline to play audio generated from text by [Baidu Speech synthesis](http://ai.baidu.com/tech/speech/tts) service. The example provides text in Chinese, but other languages are supported as well.

To run this example you need ESP32 LyraT or compatible board:
- Connect speakers or headphones to the board. 
- Setup the Wi-Fi connection by `make menuconfig` -> `Example configuration` -> Fill Wi-Fi SSID and Password
- Provide `BAIDU_ACCESS_KEY` and `BAIDU_SECRET_KEY`  by `make menuconfig` -> `Example configuration` -> Fill BAIDU_ACCESS_KEY and BAIDU_SECRET_KEY value 


For more details on how to use the Baidu Speech synthesis service you can refer to [Speech Synthesis API](https://cloud.baidu.com/doc/SPEECH/TTS-API.html). 
Note: The language used in Baidu's services is Chinese
