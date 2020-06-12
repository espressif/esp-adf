# Uart Console example

This example shows how to use `periph_console` to control `esp_audio` APIs and system APIs.

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with 'test.amr', 'test.flac', 'test.ogg', 'test.opus', 'test.mp3', 'test.wav', 'test.aac', 'test.ts' and 'test.m4a' into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- Set up Wi-Fi SSID and Password by console, refer to system commands join.

Load and run the example.

If the `ESP_AUDIO_AUTO_PLAY` is opened, it can automatically recognize audio formats, which are added in the `auto_decode` list. 

## Supported Commands

### Audio Commands

- play n: Play music with given index
- play URI: Play music by given URI, e.g. 'play file://sdcard/test.mp3'.
- pause: Pause the playing music
- resume: Resume the paused music
- stop: Stop the playing music
- setvol: Set volume
- getvol: Get volume
- getpos: Get current position in seconds

### System Commands

- reboot: Restart system
- free: Get the free memory
- stat: Show the processor time amongst FreeRTOS tasks
- tasks: Get information about running tasks
- join: Connect Wi-Fi with ssid and password
- wifi: Get connected AP SSID
- led: Show some led bar patterns with ESP32-LyraT-MSC board

### Note

- To run _stat_ command, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS must be enabled in `menuconfig` > `Component Config` > `FreeRTOS` > `Enable FreeRTOS to collect run time stats`.
- To run _tasklist_ command, CONFIG_FREERTOS_USE_TRACE_FACILITY must be enabled in `menuconfig` >`Component Config` > `FreeRTOS` > `Enable FreeRTOS trace facility and Enable FreeRTOS stats formatting functions`
- To run aac decoder, CONFIG_FREERTOS_HZ should be 1000 Hz.
- If choose board `ESP32-S2-kaluga-1` , not support microSD card.
