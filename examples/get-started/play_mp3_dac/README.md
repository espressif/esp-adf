# Play mp3 file using ESP32 internal DAC

This example plays a sample 7 second mp3 file provided in 'main' folder. The file gets embedded in the application during compilation. After application upload the file is then played from ESP32's internal flash.

To run this example you need an ESP32 board that has DAC GPIO 25 and 26 exposed and earphones. Connect the pins to the earphones according to schematic below.


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

## Notes on using DAC instead of codec chip in other examples

This example is based on [play_mp3](../play_mp3) example and contains only these changes that are required to output I2S stream to the internal DAC instead of external codec chip. You can adopt other examples that use the codec chip as the sound output, and do not use it for other purposes, e.g. to retrieve I2S stream from the microphones.

In particular the following changes are required:

* Remove or comment out the code that performs initialization of the codec chip:

  ```
  ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
  audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
  audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
  audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
  ```

* Change configuration of the output I2S stream to send the data to the DAC instead of the codec chip:

  ```
  ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
  ```

  change to:

  ```
  ESP_LOGI(TAG, "[1.2] Create i2s stream to write data to ESP32 internal DAC");
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_INTERNAL_DAC_CFG_DEFAULT();
  ```
