# DuerOS3.0 Example
This example shows how to use ADF APIs to connect DuerOS3.0
To run this example you need ESP32 LyraT or compatible board:

- Setup Wi-Fi SSID and Password.
- Connect speakers or headphone to the board.
- Select your DuerOS device profile instead of `ADF_APTH/examples/dueros/main/duer_profile`. If you don't have a DuerOS device profile, please refer to [DuerOS Developer Certification Guide](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown) and apply for a DuerOS Developer Account.

## Supported Features
- Support say "Hi Lexin" trigger the DuerOS voice interaction, green LED turn on for indicate wakeup.
- Press [Rec] button to talk to DuerOS. The device will playback the DuerOS response. You can say anything you want, e.g."今天天气怎么样?" or "现在几点了?", which means "What's the weather today?" or " what time is it?".
- The green LED indicates Wi-Fi status
    - Green LED on if Wi-Fi connected;
    - Green LED blinks normally if Wi-Fi disconnected;
    - Green LED fast blinks if Wi-Fi is under setting;
- Adjust volume via [Vol-] or [Vol+]
- Configure Wi-Fi by [Set] button

### Note:
- DuerOS profile is device unique ID.
- There are serial spcific configurations for DuerOS example, please refer to `ADF_APTH/examples/dueros/sdkconfig.defaults`.
