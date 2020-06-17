# Play mp3 file using ESP32 internal DAC

This example plays a sample 7 second mp3 file provided in 'main' folder. The file gets embedded in the application during compilation. After application upload the file is then played from ESP32's internal flash.

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

To run this example you need an ESP32 board that has DAC pins GPIO25 and GPIO26 exposed

On ESP32-LyraT GPIO25 and GPIO26 are broken out on I2S pin header as LRCK and DSDIN.


## Usage

Prepare the ESP32 board by connecting GPIO25, GPIO26 and GND pins to the earphones according to schematic below.

Example connections:

```
GPIO25 +--------------+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
             330R     |
GND +-------/\/\/\----+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
                      |
GPIO26 +--------------+
```                     

Load and run the example.

## Notes on using DAC instead of codec chip in other examples

This example is based on [play_mp3](../play_mp3) example and contains only these changes that are required to output I2S stream to the internal DAC instead of external codec chip. You can adopt other examples that use the codec chip as the sound output, and do not use it for other purposes, e.g. to retrieve I2S stream from the microphones.

In particular the following changes are required:

* Remove or comment out the code that performs initialization of the codec chip:

  ```c
  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
  ```

* Locate the code that performs configuration of the output I2S stream:

  ```c
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
  ```

  and change it to process the data through DAC:

  ```c
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_INTERNAL_DAC_CFG_DEFAULT();
  ```
