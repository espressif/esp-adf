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

### Note
- DuerOS profile is device unique ID.
- There are serial spcific configurations for DuerOS example, please refer to `ADF_APTH/examples/dueros/sdkconfig.defaults`.

### Known Issues
- There is a bug on touch driver which is take a lot of cpu. The phenomenon is can't wakeup by voice or play music with stutter. Workaround is not used follow codes at `duer_service_create`.
```c
    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    esp_periph_start(touch_periph);
```
