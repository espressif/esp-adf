# Play MP3 file from flash (spiffs system) 

The demo plays a MP3 stored on the flash using audio pipeline API

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

**Prepare the audio board:**

- Connect speakers or headphones to the board.

**Configure the example:**

- Select compatible audio board in `menuconfig` > `Audio HAL`.

 **Make Spiffs file**

- Get the mkspiffs from  [**github/spiffs**](https://github.com/igrr/mkspiffs.git) 
  - git clone https://github.com/igrr/mkspiffs.git
- Complie the mkspiffs
  ```
  cd mkspiffs
  make clean
  make dist CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4"
  ```
- Copy music files into tools folder(There is only one adf_music.mp3 file in the default tools file).

- Running command `./mkspiffs -c ./tools -b 4096 -p 256 -s 0x100000 ./tools/adf_music.bin`. Then, all of the music files are compressed into `adf_music.bin` file.

**Download**
- Create partition table as follow
  ```
    nvs,      data, nvs,     ,        0x6000,
    phy_init, data, phy,     ,        0x1000,
    factory,  app,  factory, ,        1M,
    storage,  data, spiffs,  0x110000,1M, 
  ```
- Download the spiffs bin. Now the `./tools/adf_music.bin` include `adf_music.mp3` only (All MP3 files will eventually generate a bin file).
  ```
  python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x110000 ./tools/adf_music.bin
  ```

