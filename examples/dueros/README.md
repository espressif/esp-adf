# DuerOS3.0 Example

This example shows how to use ADF APIs to connect DuerOS3.0

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-Dul1906 | ![alt text](../../docs/_static/esp32-korvo-dul1906-v1.1-small.jpg "ESP32-Korvo-DUL1906") | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | ![alt text](../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit") | <img src="../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../docs/_static/no-button.png "Compatible") |


## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.
- Insert a microSD card loaded with file `tone/DingDing.wav` into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up the Wi-Fi connection by running `menuconfig` > `Example Configuration` and filling in `WiFi SSID` and `WiFi Password`.
- Select your DuerOS device profile instead of `ADF_PATH/components/dueros_service/duer_profile`. If you don't have a DuerOS device profile, please refer to [DuerOS Developer Certification Guide](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown) and apply for a DuerOS Developer Account.

Load and run the example.


## Supported Features

- Say "Hi Lexin" to trigger the DuerOS voice interaction, green LED will turn on for indicate wakeup, if the `DingDing.wav` exist in microSD, "Dingding" should be heard at same time.
- Press [Rec] button to talk to DuerOS. The device will play back the DuerOS response. You can say anything you want, e.g."今天天气怎么样?" or "现在几点了?", which means "What's the weather today?" or " what time is it?".
- The green LED indicates Wi-Fi status:
    - Green LED on if Wi-Fi connected
    - Green LED blinks normally if Wi-Fi disconnected
    - Green LED fast blinks if Wi-Fi is under setting
- Adjust volume via [Vol-] or [Vol+]
- Configure Wi-Fi by [Set] button

### Note

- DuerOS profile is device unique ID.
- There is a specific configuration for DuerOS example, please refer to [sdkconfig.defaults](sdkconfig.defaults).

### Known Issues

- There is a limitation in touch driver that it takes a lot of CPU, and the impacts are either system can't be waken up by voice or system will play music with stutter. The workaround is not to  use the following code at `duer_service_create`.

```c
    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    esp_periph_start(touch_periph);
```
