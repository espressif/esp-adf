# DuerOS3.0 Example.
This example shows how to use ADF APIs to connect DuerOS3.0
To run this example you need ESP32 LyraT or compatible board:

- Setup Wi-Fi SSID and Password.
- Connect speakers or headphone to the board. 
- Select your DuerOS device profile instead of `ADF_APTH/examples/dueros/main/duer_profile`. If you have not, please refer to [DuerOS Developer Certification Guide](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown) apply for the DuerOS developer.

## Supported Features
- Press [Rec] button to talk to DuerOS. The device will playback the DuerOS response. You can say anything what do you want, e.g."今天天气怎么样?" or "现在几点了?", what means "What's the weather?" or " what time is it?".
- Wi-Fi Status indicate by green led. Wi-Fi connected green led turn on, disconnect with normal blink, setting will be fast blink.
- Adjust volume by by touching [Vol-] or [Vol+]
- Config Wi-Fi by [Set] button

### Note:
- DuerOS profile is device unique ID. 


