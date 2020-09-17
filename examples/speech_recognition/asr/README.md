# Speech recognition example

The example demonstrates the speech recognition function on ESP32-LyraT and ESP32-LyraTD-MSC board. It contains nine keywords.

## WakeUp words

| # | word    | English Meaning | Pronunciation|
|:-:|---------|--------------------|-------------------|
| 1 | 嗨，乐鑫 | Hi, Espressif      | Hāi, lè xīn        |

## Key words

| #  | word      | English Meaning                | Pronunciation             |
|:-: |-----------|--------------------------------|---------------------------|
| 0  | 打开空调    | turn on air-condition         | dā kaī kōng tiáo          |
| 1  | 关闭空调    |turn off air-condition         | guān bì kōng tiáo         |
| 2  | 增大风速    | increase in speed             | zēng dà fēng sù           |
| 3  | 减小风速    | decrease in speed             | jiǎn xiǎo fēng sù         |
| 4  | 升高一度    | increase in temperature       | shēng gāo yī dù            |
| 5  | 降低一度    | decrease in temperature       | jiàng dī yī dù             |
| 6  | 制热模式    | hot mode                      | zhì rè moó shì             |
| 7  | 制冷模式    | slow mode                     | zhì lěng mó shì           |
| 8  | 送风模式    | blower mode                   | jie néng mó shì           |
| 9  | 节能模式    | save power mode               | jíe néng mó shì           |
| 10 | 关闭节能模式 | turn off save power mode      | guān bì jíe néng mó shì   |
| 11 | 除湿模式    | dampness mode                 | chú shi mó shi            |
| 12 | 关闭除湿模式 | turn off dampness mode        | guān bì chú shī mó shì    |
| 13 | 打开蓝牙    | turn on bt                    | dā kaī lán yá             |
| 14 | 关闭蓝牙    | turn off bt                   | guān bì lán yá            |
| 15 | 播放歌曲    | turn on                       | bō fàng gē qǚ             |
| 16 | 暂停播放    | turn off                      | zàn tíng bō fàng          |
| 17 | 定时一小时  | timer one hour                | dìng shí yī xiǎo shí      |
| 18 | 打开电灯    | turn on lignt                 | dā kaī diàn dēng          |
| 19 | 关闭点灯    | turn off lignt                | guān bì diàn dēng         |

### Note

If your Chinese pronunciation is not good, you can try the "Alexa" keyword. You need to modify some configurations to use them, see "Usage" for details.

## Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

## Usage

Prepare the audio board:

- Insert a microSD card loaded with  MP3 files 'dingdong.mp3' and 'haode.mp3' into board's slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- If you want to use "nihaoxiaozhi" as a wakeup word, open menuconfig, go to `Speech Recognition Configuration` and select:
    - `Wake word engine` > `WakeNet 5 (quantized)` or > `WakeNet 6 (quantized)`
    - `Wake word name` > `nihaoxiaozhi`
- If you want to use "nihaoxiaozhi" as a wakeup word, open menuconfig, go to `Speech Recognition Configuration` and select:
    - `Wake word engine` > `WakeNet 5 (quantized)`
    - `Wake word name` > `hi jeson`
Load and run the example:

- Say the keywords to the board and you should see them printed out in the monitor.
