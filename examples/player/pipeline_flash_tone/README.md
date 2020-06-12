# Play MP3 file from Flash

The demo plays a MP3 stored in the flash using audio pipeline API

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board.
- Config flash partition in **partition_flash_tone.csv**
```
flashTone,data,  0x04,  0x110000 , 500K,
```
- Download audio-esp.bin
```  
  python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x110000 ./tools/audio-esp.bin
```
Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Download and run the example:

- The board will start playing automatically.


## Document Making

- Please use mk_audio_bin.py and prompt sound file in the same directory(the mk_audio_bin.py in tools folder), then run mk_audio_bin.py file, and eventually generate audio-esp.bin file and a components folder in upper level directory, and the components folder include the url of tone.
```c
  python tools/mk_audio_bin.py tone_mp3_folder
```


- The default audio-esp.bin include tone as follows,and we will judge the tone based on the number
``` c
   "flash://tone/0_Bt_Reconnect.mp3",
   "flash://tone/1_Wechat.mp3",
   "flash://tone/2_Welcome_To_Wifi.mp3",
   "flash://tone/3_New_Version_Available.mp3",
   "flash://tone/4_Bt_Success.mp3",
   "flash://tone/5_Freetalk.mp3",
   "flash://tone/6_Upgrade_Done.mp3",
   "flash://tone/7_shutdown.mp3",
   "flash://tone/8_Alarm.mp3",
   "flash://tone/9_Wifi_Success.mp3",
   "flash://tone/10_Under_Smartconfig.mp3",
   "flash://tone/11_Out_Of_Power.mp3",
   "flash://tone/12_server_connect.mp3",
   "flash://tone/13_hello.mp3",
   "flash://tone/14_new_message.mp3",
   "flash://tone/15_Please_Retry_Wifi.mp3",
   "flash://tone/16_please_setting_wifi.mp3",
   "flash://tone/17_Welcome_To_Bt.mp3",
   "flash://tone/18_Wifi_Time_Out.mp3",
   "flash://tone/19_Wifi_Reconnect.mp3",
   "flash://tone/20_server_disconnect.mp3",
```
